/* This file is part of Patchage.
 * Copyright 2007-2020 David Robillard <d@drobilla.net>
 *
 * Patchage is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Patchage is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Patchage.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "AudioDriver.hpp"
#include "ClientID.hpp"
#include "ClientInfo.hpp"
#include "ClientType.hpp"
#include "ILog.hpp"
#include "PatchageEvent.hpp"
#include "PortInfo.hpp"
#include "PortNames.hpp"
#include "PortType.hpp"
#include "SignalDirection.hpp"
#include "jackey.h"
#include "make_jack_driver.hpp"
#include "patchage_config.h"
#include "warnings.hpp"

#ifdef HAVE_JACK_METADATA
#	include <jack/metadata.h>
#endif

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
PATCHAGE_RESTORE_WARNINGS

#include <jack/jack.h>
#include <jack/statistics.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>

/// Driver for JACK audio and midi ports that uses libjack
class JackLibDriver : public AudioDriver
{
public:
	explicit JackLibDriver(ILog& log, EventSink emit_event);

	JackLibDriver(const JackLibDriver&) = delete;
	JackLibDriver& operator=(const JackLibDriver&) = delete;

	JackLibDriver(JackLibDriver&&) = delete;
	JackLibDriver& operator=(JackLibDriver&&) = delete;

	~JackLibDriver() override;

	// Driver interface
	void attach(bool launch_daemon) override;
	void detach() override;
	bool is_attached() const override;
	void refresh(const EventSink& sink) override;
	bool connect(const PortID& tail_id, const PortID& head_id) override;
	bool disconnect(const PortID& tail_id, const PortID& head_id) override;

	// AudioDriver interface
	uint32_t xruns() override;
	void     reset_xruns() override;
	uint32_t buffer_size() override;
	bool     set_buffer_size(uint32_t frames) override;
	uint32_t sample_rate() override;

private:
	ClientInfo get_client_info(const char* name);
	PortInfo   get_port_info(const jack_port_t* port);

	void shutdown();

	static void jack_client_registration_cb(const char* name,
	                                        int         registered,
	                                        void*       jack_driver);

	static void jack_port_registration_cb(jack_port_id_t port_id,
	                                      int            registered,
	                                      void*          jack_driver);

	static void jack_port_connect_cb(jack_port_id_t src,
	                                 jack_port_id_t dst,
	                                 int            connect,
	                                 void*          jack_driver);

	static int jack_xrun_cb(void* jack_driver);

	static void jack_shutdown_cb(void* jack_driver);

	ILog&      _log;
	std::mutex _shutdown_mutex;

	jack_client_t* _client      = nullptr;
	jack_nframes_t _buffer_size = 0u;
	uint32_t       _xruns       = 0u;

	bool _is_activated : 1;
};

JackLibDriver::JackLibDriver(ILog& log, EventSink emit_event)
    : AudioDriver{std::move(emit_event)}
    , _log{log}
    , _is_activated{false}
{}

JackLibDriver::~JackLibDriver()
{
	detach();
}

void
JackLibDriver::attach(const bool launch_daemon)
{
	if (_client) {
		return; // Already connected
	}

	const jack_options_t options =
	    (!launch_daemon) ? JackNoStartServer : JackNullOption;

	if (!(_client = jack_client_open("Patchage", options, nullptr))) {
		_log.error("[JACK] Unable to create client");
		_is_activated = false;
		return;
	}

	jack_on_shutdown(_client, jack_shutdown_cb, this);
	jack_set_client_registration_callback(
	    _client, jack_client_registration_cb, this);
	jack_set_port_registration_callback(
	    _client, jack_port_registration_cb, this);
	jack_set_port_connect_callback(_client, jack_port_connect_cb, this);
	jack_set_xrun_callback(_client, jack_xrun_cb, this);

	if (jack_activate(_client)) {
		_log.error("[JACK] Client activation failed");
		_is_activated = false;
		_buffer_size  = 0;
		return;
	}

	_is_activated = true;
	_buffer_size  = jack_get_buffer_size(_client);

	_emit_event(DriverAttachmentEvent{ClientType::jack});
}

void
JackLibDriver::detach()
{
	std::lock_guard<std::mutex> lock{_shutdown_mutex};

	if (_client) {
		jack_deactivate(_client);
		jack_client_close(_client);
		_client = nullptr;
	}

	_is_activated = false;
	_emit_event(DriverDetachmentEvent{ClientType::jack});
}

bool
JackLibDriver::is_attached() const
{
	return _client != nullptr;
}

static std::string
get_property(const jack_uuid_t subject, const char* const key)
{
	std::string result;

#ifdef HAVE_JACK_METADATA
	char* value    = nullptr;
	char* datatype = nullptr;
	if (!jack_get_property(subject, key, &value, &datatype)) {
		result = value;
	}
	jack_free(datatype);
	jack_free(value);
#else
	(void)subject;
	(void)key;
#endif

	return result;
}

ClientInfo
JackLibDriver::get_client_info(const char* const name)
{
	return {name}; // TODO: Pretty name?
}

PortInfo
JackLibDriver::get_port_info(const jack_port_t* const port)
{
	const auto        uuid  = jack_port_uuid(port);
	const auto        flags = jack_port_flags(port);
	const std::string name  = jack_port_name(port);
	auto              label = PortNames{name}.port();

	// Get pretty name to use as a label, if present
#ifdef HAVE_JACK_METADATA
	const auto pretty_name = get_property(uuid, JACK_METADATA_PRETTY_NAME);
	if (!pretty_name.empty()) {
		label = pretty_name;
	}
#endif

	// Determine detailed type, using metadata for fancy types if possible
	const char* const type_str = jack_port_type(port);
	PortType          type     = PortType::jack_audio;
	if (!strcmp(type_str, JACK_DEFAULT_AUDIO_TYPE)) {
		if (get_property(uuid, JACKEY_SIGNAL_TYPE) == "CV") {
			type = PortType::jack_cv;
		}
	} else if (!strcmp(type_str, JACK_DEFAULT_MIDI_TYPE)) {
		type = PortType::jack_midi;
		if (get_property(uuid, JACKEY_EVENT_TYPES) == "OSC") {
			type = PortType::jack_osc;
		}
	} else {
		_log.warning(fmt::format(
		    "[JACK] Port \"{}\" has unknown type \"{}\"", name, type_str));
	}

	// Get direction from port flags
	const SignalDirection direction =
	    ((flags & JackPortIsInput) ? SignalDirection::input
	                               : SignalDirection::output);

	// Get port order from metadata if possible
	boost::optional<int> order;
	const std::string    order_str = get_property(uuid, JACKEY_ORDER);
	if (!order_str.empty()) {
		order = std::stoi(order_str);
	}

	return {label, type, direction, order, bool(flags & JackPortIsTerminal)};
}

void
JackLibDriver::shutdown()
{
	_emit_event(DriverDetachmentEvent{ClientType::jack});
}

void
JackLibDriver::refresh(const EventSink& sink)
{
	std::lock_guard<std::mutex> lock{_shutdown_mutex};

	if (!_client) {
		shutdown();
		return;
	}

	// Get all existing ports
	const char** const ports = jack_get_ports(_client, nullptr, nullptr, 0);
	if (!ports) {
		return;
	}

	// Get all client names (to only send a creation event once for each)
	std::unordered_set<std::string> client_names;
	for (auto i = 0u; ports[i]; ++i) {
		client_names.insert(PortID::jack(ports[i]).client().jack_name());
	}

	// Emit all clients
	for (const auto& client_name : client_names) {
		sink({ClientCreationEvent{ClientID::jack(client_name),
		                          get_client_info(client_name.c_str())}});
	}

	// Emit all ports
	for (auto i = 0u; ports[i]; ++i) {
		const jack_port_t* const port = jack_port_by_name(_client, ports[i]);

		sink({PortCreationEvent{PortID::jack(ports[i]), get_port_info(port)}});
	}

	// Get all connections (again to only create them once)
	std::set<std::pair<std::string, std::string>> connections;
	for (auto i = 0u; ports[i]; ++i) {
		const jack_port_t* const port = jack_port_by_name(_client, ports[i]);
		const char** const peers = jack_port_get_all_connections(_client, port);

		if (peers) {
			if (jack_port_flags(port) & JackPortIsInput) {
				for (auto j = 0u; peers[j]; ++j) {
					connections.emplace(peers[j], ports[i]);
				}
			} else {
				for (auto j = 0u; peers[j]; ++j) {
					connections.emplace(ports[i], peers[j]);
				}
			}

			jack_free(peers);
		}
	}

	// Emit all connections
	for (const auto& connection : connections) {
		sink({ConnectionEvent{PortID::jack(connection.first),
		                      PortID::jack(connection.second)}});
	}

	jack_free(ports);
}

bool
JackLibDriver::connect(const PortID& tail_id, const PortID& head_id)
{
	if (!_client) {
		return false;
	}

	const auto& tail_name = tail_id.jack_name();
	const auto& head_name = head_id.jack_name();

	const int result =
	    jack_connect(_client, tail_name.c_str(), head_name.c_str());

	if (result) {
		_log.error(fmt::format(
		    "[JACK] Failed to connect {} => {}", tail_name, head_name));

		return false;
	}

	return true;
}

bool
JackLibDriver::disconnect(const PortID& tail_id, const PortID& head_id)
{
	if (!_client) {
		return false;
	}

	const auto& tail_name = tail_id.jack_name();
	const auto& head_name = head_id.jack_name();

	const int result =
	    jack_disconnect(_client, tail_name.c_str(), head_name.c_str());

	if (result) {
		_log.error(fmt::format(
		    "[JACK] Failed to disconnect {} => {}", tail_name, head_name));
		return false;
	}

	return true;
}

uint32_t
JackLibDriver::xruns()
{
	return _xruns;
}

void
JackLibDriver::reset_xruns()
{
	_xruns = 0;
}

uint32_t
JackLibDriver::buffer_size()
{
	return _is_activated ? _buffer_size : jack_get_buffer_size(_client);
}

bool
JackLibDriver::set_buffer_size(const uint32_t frames)
{
	if (!_client) {
		_buffer_size = frames;
		return true;
	}

	if (buffer_size() == frames) {
		return true;
	}

	if (jack_set_buffer_size(_client, frames)) {
		_log.error("[JACK] Unable to set buffer size");
		return false;
	}

	_buffer_size = frames;
	return true;
}

uint32_t
JackLibDriver::sample_rate()
{
	return jack_get_sample_rate(_client);
}

void
JackLibDriver::jack_client_registration_cb(const char* const name,
                                           const int         registered,
                                           void* const       jack_driver)
{
	auto* const me = static_cast<JackLibDriver*>(jack_driver);

	if (registered) {
		me->_emit_event(ClientCreationEvent{ClientID::jack(name), {name}});
	} else {
		me->_emit_event(ClientDestructionEvent{ClientID::jack(name)});
	}
}

void
JackLibDriver::jack_port_registration_cb(const jack_port_id_t port_id,
                                         const int            registered,
                                         void* const          jack_driver)
{
	auto* const me = static_cast<JackLibDriver*>(jack_driver);

	jack_port_t* const port = jack_port_by_id(me->_client, port_id);
	const char* const  name = jack_port_name(port);
	const auto         id   = PortID::jack(name);

	if (registered) {
		me->_emit_event(PortCreationEvent{id, me->get_port_info(port)});
	} else {
		me->_emit_event(PortDestructionEvent{id});
	}
}

void
JackLibDriver::jack_port_connect_cb(const jack_port_id_t src,
                                    const jack_port_id_t dst,
                                    const int            connect,
                                    void* const          jack_driver)
{
	auto* const me = static_cast<JackLibDriver*>(jack_driver);

	jack_port_t* const src_port = jack_port_by_id(me->_client, src);
	jack_port_t* const dst_port = jack_port_by_id(me->_client, dst);
	const char* const  src_name = jack_port_name(src_port);
	const char* const  dst_name = jack_port_name(dst_port);

	if (connect) {
		me->_emit_event(
		    ConnectionEvent{PortID::jack(src_name), PortID::jack(dst_name)});
	} else {
		me->_emit_event(
		    DisconnectionEvent{PortID::jack(src_name), PortID::jack(dst_name)});
	}
}

int
JackLibDriver::jack_xrun_cb(void* const jack_driver)
{
	auto* const me = static_cast<JackLibDriver*>(jack_driver);

	++me->_xruns;

	return 0;
}

void
JackLibDriver::jack_shutdown_cb(void* const jack_driver)
{
	auto* const me = static_cast<JackLibDriver*>(jack_driver);

	std::lock_guard<std::mutex> lock{me->_shutdown_mutex};

	me->_client       = nullptr;
	me->_is_activated = false;

	me->_emit_event(DriverDetachmentEvent{ClientType::jack});
}

std::unique_ptr<AudioDriver>
make_jack_driver(ILog& log, Driver::EventSink emit_event)
{
	return std::unique_ptr<AudioDriver>{
	    new JackLibDriver{log, std::move(emit_event)}};
}
