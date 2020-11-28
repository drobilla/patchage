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

#include "JackDriver.hpp"

#include "ClientID.hpp"
#include "ILog.hpp"
#include "Patchage.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageEvent.hpp"
#include "PortNames.hpp"
#include "PortType.hpp"
#include "SignalDirection.hpp"
#include "patchage_config.h"

#ifdef HAVE_JACK_METADATA
#	include "jackey.h"
#	include <jack/metadata.h>
#endif

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
PATCHAGE_RESTORE_WARNINGS

#include <jack/jack.h>
#include <jack/statistics.h>

#include <cassert>
#include <cstring>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>

JackDriver::JackDriver(ILog& log, EventSink emit_event)
    : Driver{std::move(emit_event)}
    , _log(log)
    , _client(nullptr)
    , _last_pos{}
    , _buffer_size(0)
    , _xruns(0)
    , _xrun_delay(0)
    , _is_activated(false)
{
	_last_pos.frame = 0;
	_last_pos.valid = {};
}

JackDriver::~JackDriver()
{
	detach();
}

void
JackDriver::attach(bool launch_daemon)
{
	// Already connected
	if (_client) {
		return;
	}

	jack_options_t options =
	    (!launch_daemon) ? JackNoStartServer : JackNullOption;
	_client = jack_client_open("Patchage", options, nullptr);
	if (_client == nullptr) {
		_log.error("[JACK] Unable to create client");
		_is_activated = false;
	} else {
		jack_client_t* const client = _client;

		jack_on_shutdown(client, jack_shutdown_cb, this);
		jack_set_client_registration_callback(
		    client, jack_client_registration_cb, this);
		jack_set_port_registration_callback(
		    client, jack_port_registration_cb, this);
		jack_set_port_connect_callback(client, jack_port_connect_cb, this);
		jack_set_xrun_callback(client, jack_xrun_cb, this);

		_buffer_size = jack_get_buffer_size(client);

		if (!jack_activate(client)) {
			_is_activated = true;
			signal_attached.emit();
			std::stringstream ss;
			_log.info("[JACK] Attached");
		} else {
			_log.error("[JACK] Client activation failed");
			_is_activated = false;
		}
	}
}

void
JackDriver::detach()
{
	std::lock_guard<std::mutex> lock{_shutdown_mutex};

	if (_client) {
		jack_deactivate(_client);
		jack_client_close(_client);
		_client = nullptr;
	}
	_is_activated = false;
	signal_detached.emit();
	_log.info("[JACK] Detached");
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
JackDriver::get_client_info(const char* const name)
{
	return {name}; // TODO: Pretty name?
}

PortInfo
JackDriver::get_port_info(const jack_port_t* const port)
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
		order = atoi(order_str.c_str());
	}

	return {label, type, direction, order, bool(flags & JackPortIsTerminal)};
}

void
JackDriver::shutdown()
{
	signal_detached.emit();
}

void
JackDriver::refresh(const EventSink& sink)
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
JackDriver::port_names(const PortID& id,
                       std::string&  module_name,
                       std::string&  port_name)
{
	jack_port_t* jack_port = nullptr;

	if (id.type() == PortID::Type::jack) {
		jack_port = jack_port_by_name(_client, id.jack_name().c_str());
	}

	if (!jack_port) {
		module_name.clear();
		port_name.clear();
		return false;
	}

	const std::string full_name = jack_port_name(jack_port);

	module_name = full_name.substr(0, full_name.find(':'));
	port_name   = full_name.substr(full_name.find(':') + 1);

	return true;
}

bool
JackDriver::connect(const PortID tail_id, const PortID head_id)
{
	if (!_client) {
		return false;
	}

	const auto& tail_name = tail_id.jack_name();
	const auto& head_name = head_id.jack_name();

	const int result =
	    jack_connect(_client, tail_name.c_str(), head_name.c_str());

	if (result == 0) {
		_log.info(
		    fmt::format("[JACK] Connected {} => {}", tail_name, head_name));
	} else {
		_log.error(fmt::format(
		    "[JACK] Failed to connect {} => {}", tail_name, head_name));
	}

	return !result;
}

bool
JackDriver::disconnect(const PortID tail_id, const PortID head_id)
{
	if (!_client) {
		return false;
	}

	const auto& tail_name = tail_id.jack_name();
	const auto& head_name = head_id.jack_name();

	const int result =
	    jack_disconnect(_client, tail_name.c_str(), head_name.c_str());

	if (result == 0) {
		_log.info(
		    fmt::format("[JACK] Disconnected {} => {}", tail_name, head_name));
	} else {
		_log.error(fmt::format(
		    "[JACK] Failed to disconnect {} => {}", tail_name, head_name));
	}

	return !result;
}

void
JackDriver::jack_client_registration_cb(const char* name,
                                        int         registered,
                                        void*       jack_driver)
{
	auto* const me = static_cast<JackDriver*>(jack_driver);
	assert(me->_client);

	if (registered) {
		me->_emit_event(ClientCreationEvent{ClientID::jack(name), {name}});
	} else {
		me->_emit_event(ClientDestructionEvent{ClientID::jack(name)});
	}
}

void
JackDriver::jack_port_registration_cb(jack_port_id_t port_id,
                                      int            registered,
                                      void*          jack_driver)
{
	auto* me = static_cast<JackDriver*>(jack_driver);
	assert(me->_client);

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
JackDriver::jack_port_connect_cb(jack_port_id_t src,
                                 jack_port_id_t dst,
                                 int            connect,
                                 void*          jack_driver)
{
	auto* me = static_cast<JackDriver*>(jack_driver);
	assert(me->_client);

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
JackDriver::jack_xrun_cb(void* jack_driver)
{
	auto* me = static_cast<JackDriver*>(jack_driver);
	assert(me->_client);

	++me->_xruns;
	me->_xrun_delay = jack_get_xrun_delayed_usecs(me->_client);

	jack_reset_max_delayed_usecs(me->_client);

	return 0;
}

void
JackDriver::jack_shutdown_cb(void* jack_driver)
{
	assert(jack_driver);
	auto* me = static_cast<JackDriver*>(jack_driver);
	me->_log.info("[JACK] Shutdown");

	std::lock_guard<std::mutex> lock{me->_shutdown_mutex};

	me->_client       = nullptr;
	me->_is_activated = false;
	me->signal_detached.emit();
}

jack_nframes_t
JackDriver::buffer_size()
{
	if (_is_activated) {
		return _buffer_size;
	}

	return jack_get_buffer_size(_client);
}

void
JackDriver::reset_xruns()
{
	_xruns      = 0;
	_xrun_delay = 0;
}

float
JackDriver::get_max_dsp_load()
{
	float max_load = 0.0f;
	if (_client) {
		const float max_delay = jack_get_max_delayed_usecs(_client);
		const float rate      = sample_rate();
		const float size      = buffer_size();
		const float period    = size / rate * 1000000; // usec

		if (max_delay > period) {
			max_load = 1.0;
			jack_reset_max_delayed_usecs(_client);
		} else {
			max_load = max_delay / period;
		}
	}
	return max_load;
}

void
JackDriver::reset_max_dsp_load()
{
	if (_client) {
		jack_reset_max_delayed_usecs(_client);
	}
}

bool
JackDriver::set_buffer_size(jack_nframes_t size)
{
	if (buffer_size() == size) {
		return true;
	}

	if (!_client) {
		_buffer_size = size;
		return true;
	}

	if (jack_set_buffer_size(_client, size)) {
		_log.error("[JACK] Unable to set buffer size");
		return false;
	}

	_buffer_size = size;
	return true;
}
