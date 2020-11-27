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
#include "PatchageModule.hpp"
#include "Queue.hpp"
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

JackDriver::JackDriver(Patchage* app, ILog& log)
    : _app(app)
    , _log(log)
    , _client(nullptr)
    , _events(128)
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

/** Connect to Jack.
 */
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

static bool
is_jack_port(const PatchagePort* port)
{
	return (port->type() == PortType::jack_audio ||
	        port->type() == PortType::jack_midi ||
	        port->type() == PortType::jack_osc ||
	        port->type() == PortType::jack_cv);
}

/** Destroy all JACK (canvas) ports.
 */
void
JackDriver::destroy_all()
{
	if (_app->canvas()) {
		_app->canvas()->remove_ports(is_jack_port);
	}
}

PatchagePort*
JackDriver::create_port_view(Patchage* patchage, const PortID& id)
{
	assert(id.type == PortID::Type::jack_id);

	jack_port_t* jack_port = jack_port_by_id(_client, id.id.jack_id);
	if (!jack_port) {
		_log.error(fmt::format("[JACK] Failed to find port with ID \"{}\"",
		                       id.id.jack_id));
		return nullptr;
	}

	const int jack_flags = jack_port_flags(jack_port);

	std::string module_name;
	std::string port_name;
	port_names(id, module_name, port_name);

	ModuleType type = ModuleType::input_output;
	if (_app->conf()->get_module_split(module_name,
	                                   (jack_flags & JackPortIsTerminal))) {
		if (jack_flags & JackPortIsInput) {
			type = ModuleType::input;
		} else {
			type = ModuleType::output;
		}
	}

	PatchageModule* parent = _app->canvas()->find_module(module_name, type);
	if (!parent) {
		parent = new PatchageModule(
		    patchage, module_name, type, ClientID::jack(module_name));
		parent->load_location();
		patchage->canvas()->add_module(module_name, parent);
	}

	if (parent->get_port(port_name)) {
		_log.error(fmt::format("[JACK] Module \"{}\" already has port \"{}\"",
		                       module_name,
		                       port_name));
		return nullptr;
	}

	PatchagePort* port = create_port(*parent, jack_port, id);
	port->show();
	if (port->is_input()) {
		parent->set_is_source(false);
	}

	return port;
}

#ifdef HAVE_JACK_METADATA
static std::string
get_property(jack_uuid_t subject, const char* key)
{
	std::string result;

	char* value    = nullptr;
	char* datatype = nullptr;
	if (!jack_get_property(subject, key, &value, &datatype)) {
		result = value;
	}
	jack_free(datatype);
	jack_free(value);

	return result;
}
#endif

PatchagePort*
JackDriver::create_port(PatchageModule& parent,
                        jack_port_t*    port,
                        const PortID&   id)
{
	if (!port) {
		return nullptr;
	}

	std::string          label;
	boost::optional<int> order;

#ifdef HAVE_JACK_METADATA
	const jack_uuid_t uuid = jack_port_uuid(port);
	if (_app->conf()->get_sort_ports()) {
		const std::string order_str = get_property(uuid, JACKEY_ORDER);
		label = get_property(uuid, JACK_METADATA_PRETTY_NAME);
		if (!order_str.empty()) {
			order = atoi(order_str.c_str());
		}
	}
#endif

	const char* const type_str  = jack_port_type(port);
	PortType          port_type = PortType::jack_audio;
	if (!strcmp(type_str, JACK_DEFAULT_AUDIO_TYPE)) {
		port_type = PortType::jack_audio;
#ifdef HAVE_JACK_METADATA
		if (get_property(uuid, JACKEY_SIGNAL_TYPE) == "CV") {
			port_type = PortType::jack_cv;
		}
#endif
	} else if (!strcmp(type_str, JACK_DEFAULT_MIDI_TYPE)) {
		port_type = PortType::jack_midi;
#ifdef HAVE_JACK_METADATA
		if (get_property(uuid, JACKEY_EVENT_TYPES) == "OSC") {
			port_type = PortType::jack_osc;
		}
#endif
	} else {
		_log.warning(fmt::format("[JACK] Port \"{}\" has unknown type \"{}\"",
		                         jack_port_name(port),
		                         type_str));
		return nullptr;
	}

	auto* ret = new PatchagePort(parent,
	                             port_type,
	                             id,
	                             jack_port_short_name(port),
	                             label,
	                             (jack_port_flags(port) & JackPortIsInput),
	                             _app->conf()->get_port_color(port_type),
	                             _app->show_human_names(),
	                             order);

	if (id.type != PortID::Type::nothing) {
		dynamic_cast<PatchageCanvas*>(parent.canvas())->index_port(id, ret);
	}

	return ret;
}

void
JackDriver::shutdown()
{
	signal_detached.emit();
}

/** Refresh all Jack audio ports/connections.
 * To be called from GTK thread only.
 */
void
JackDriver::refresh()
{
	const char** ports = nullptr;
	jack_port_t* port  = nullptr;

	// Jack can take _client away from us at any time throughout here :/
	// Shortest locks possible is the best solution I can figure out

	std::lock_guard<std::mutex> lock{_shutdown_mutex};

	if (_client == nullptr) {
		shutdown();
		return;
	}

	ports =
	    jack_get_ports(_client, nullptr, nullptr, 0); // get all existing ports

	if (!ports) {
		return;
	}

	std::string client1_name;
	std::string port1_name;
	std::string client2_name;
	std::string port2_name;
	size_t      colon = std::string::npos;

	// Add all ports
	for (int i = 0; ports[i]; ++i) {
		port = jack_port_by_name(_client, ports[i]);

		client1_name = ports[i];
		client1_name = client1_name.substr(0, client1_name.find(':'));

		ModuleType type = ModuleType::input_output;
		if (_app->conf()->get_module_split(
		        client1_name, (jack_port_flags(port) & JackPortIsTerminal))) {
			if (jack_port_flags(port) & JackPortIsInput) {
				type = ModuleType::input;
			} else {
				type = ModuleType::output;
			}
		}

		PatchageModule* m = _app->canvas()->find_module(client1_name, type);

		if (!m) {
			m = new PatchageModule(
			    _app, client1_name, type, ClientID::jack(client1_name));
			m->load_location();
			_app->canvas()->add_module(client1_name, m);
		}

		if (!m->get_port(jack_port_short_name(port))) {
			create_port(*m, port, PortID());
		}
	}

	// Add all connections
	for (int i = 0; ports[i]; ++i) {
		port = jack_port_by_name(_client, ports[i]);
		const char** connected_ports =
		    jack_port_get_all_connections(_client, port);

		client1_name = ports[i];
		colon        = client1_name.find(':');
		port1_name   = client1_name.substr(colon + 1);
		client1_name = client1_name.substr(0, colon);

		const ModuleType port1_type = (jack_port_flags(port) & JackPortIsInput)
		                                  ? ModuleType::input
		                                  : ModuleType::output;

		PatchageModule* client1_module =
		    _app->canvas()->find_module(client1_name, port1_type);

		if (connected_ports) {
			for (int j = 0; connected_ports[j]; ++j) {

				client2_name = connected_ports[j];
				colon        = client2_name.find(':');
				port2_name   = client2_name.substr(colon + 1);
				client2_name = client2_name.substr(0, colon);

				const ModuleType port2_type = (port1_type == ModuleType::input)
				                                  ? ModuleType::output
				                                  : ModuleType::input;

				PatchageModule* client2_module =
				    _app->canvas()->find_module(client2_name, port2_type);

				Ganv::Port* port1 = client1_module->get_port(port1_name);
				Ganv::Port* port2 = client2_module->get_port(port2_name);

				if (!port1 || !port2) {
					continue;
				}

				Ganv::Port* src = nullptr;
				Ganv::Port* dst = nullptr;

				if (port1->is_output() && port2->is_input()) {
					src = port1;
					dst = port2;
				} else {
					src = port2;
					dst = port1;
				}

				if (src && dst && !_app->canvas()->get_edge(src, dst)) {
					_app->canvas()->make_connection(src, dst);
				}
			}

			jack_free(connected_ports);
		}
	}

	jack_free(ports);
}

bool
JackDriver::port_names(const PortID& id,
                       std::string&  module_name,
                       std::string&  port_name)
{
	jack_port_t* jack_port = nullptr;

	if (id.type == PortID::Type::jack_id) {
		jack_port = jack_port_by_id(_client, id.id.jack_id);
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

/** Connects two Jack audio ports.
 * To be called from GTK thread only.
 * \return Whether connection succeeded.
 */
bool
JackDriver::connect(const PortID       tail_id,
                    const std::string& tail_client_name,
                    const std::string& tail_port_name,
                    const PortID       head_id,
                    const std::string& head_client_name,
                    const std::string& head_port_name)
{
	(void)tail_id;
	(void)head_id;

	if (!_client) {
		return false;
	}

	const auto tail_name = tail_client_name + ":" + tail_port_name;
	const auto head_name = head_client_name + ":" + head_port_name;

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

/** Disconnects two Jack audio ports.
 * To be called from GTK thread only.
 * \return Whether disconnection succeeded.
 */
bool
JackDriver::disconnect(const PortID       tail_id,
                       const std::string& tail_client_name,
                       const std::string& tail_port_name,
                       const PortID       head_id,
                       const std::string& head_client_name,
                       const std::string& head_port_name)
{
	(void)tail_id;
	(void)head_id;

	if (!_client) {
		return false;
	}

	const auto tail_name = tail_client_name + ":" + tail_port_name;
	const auto head_name = head_client_name + ":" + head_port_name;

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
	auto* me = static_cast<JackDriver*>(jack_driver);
	assert(me->_client);

	if (registered) {
		me->_events.push(
		    PatchageEvent(PatchageEvent::Type::client_creation, name));
	} else {
		me->_events.push(
		    PatchageEvent(PatchageEvent::Type::client_destruction, name));
	}
}

void
JackDriver::jack_port_registration_cb(jack_port_id_t port_id,
                                      int            registered,
                                      void*          jack_driver)
{
	auto* me = static_cast<JackDriver*>(jack_driver);
	assert(me->_client);

	if (registered) {
		me->_events.push(
		    PatchageEvent(PatchageEvent::Type::port_creation, port_id));
	} else {
		me->_events.push(
		    PatchageEvent(PatchageEvent::Type::port_destruction, port_id));
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

	if (connect) {
		me->_events.push(
		    PatchageEvent(PatchageEvent::Type::connection, src, dst));
	} else {
		me->_events.push(
		    PatchageEvent(PatchageEvent::Type::disconnection, src, dst));
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

void
JackDriver::process_events(Patchage* app)
{
	while (!_events.empty()) {
		PatchageEvent& ev = _events.front();
		ev.execute(app);
		_events.pop();
	}
}
