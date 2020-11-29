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
#include "ClientType.hpp"
#include "ILog.hpp"
#include "PatchageEvent.hpp"
#include "PortNames.hpp"
#include "PortType.hpp"
#include "SignalDirection.hpp"
#include "jackey.h"
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
#include <cstring>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>

JackDriver::JackDriver(ILog& log, EventSink emit_event)
    : Driver{std::move(emit_event)}
    , _log{log}
    , _is_activated{false}
{}

JackDriver::~JackDriver()
{
	detach();
}

void
JackDriver::attach(const bool launch_daemon)
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
JackDriver::detach()
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
		order = std::stoi(order_str);
	}

	return {label, type, direction, order, bool(flags & JackPortIsTerminal)};
}

void
JackDriver::shutdown()
{
	_emit_event(DriverDetachmentEvent{ClientType::jack});
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
JackDriver::connect(const PortID& tail_id, const PortID& head_id)
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
JackDriver::disconnect(const PortID& tail_id, const PortID& head_id)
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

void
JackDriver::jack_client_registration_cb(const char* const name,
                                        const int         registered,
                                        void* const       jack_driver)
{
	auto* const me = static_cast<JackDriver*>(jack_driver);

	if (registered) {
		me->_emit_event(ClientCreationEvent{ClientID::jack(name), {name}});
	} else {
		me->_emit_event(ClientDestructionEvent{ClientID::jack(name)});
	}
}

void
JackDriver::jack_port_registration_cb(const jack_port_id_t port_id,
                                      const int            registered,
                                      void* const          jack_driver)
{
	auto* const me = static_cast<JackDriver*>(jack_driver);

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
JackDriver::jack_port_connect_cb(const jack_port_id_t src,
                                 const jack_port_id_t dst,
                                 const int            connect,
                                 void* const          jack_driver)
{
	auto* const me = static_cast<JackDriver*>(jack_driver);

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
JackDriver::jack_xrun_cb(void* const jack_driver)
{
	auto* const me = static_cast<JackDriver*>(jack_driver);

	++me->_xruns;

	return 0;
}

void
JackDriver::jack_shutdown_cb(void* const jack_driver)
{
	auto* const me = static_cast<JackDriver*>(jack_driver);

	std::lock_guard<std::mutex> lock{me->_shutdown_mutex};

	me->_client       = nullptr;
	me->_is_activated = false;

	me->_emit_event(DriverDetachmentEvent{ClientType::jack});
}

jack_nframes_t
JackDriver::buffer_size()
{
	return _is_activated ? _buffer_size : jack_get_buffer_size(_client);
}

void
JackDriver::reset_xruns()
{
	_xruns = 0;
}

bool
JackDriver::set_buffer_size(jack_nframes_t size)
{
	if (!_client) {
		_buffer_size = size;
		return true;
	}

	if (buffer_size() == size) {
		return true;
	}

	if (jack_set_buffer_size(_client, size)) {
		_log.error("[JACK] Unable to set buffer size");
		return false;
	}

	_buffer_size = size;
	return true;
}
