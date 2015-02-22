/* This file is part of Patchage.
 * Copyright 2007-2014 David Robillard <http://drobilla.net>
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

#include <cassert>
#include <cstring>
#include <set>
#include <string>

#include <boost/format.hpp>

#include <jack/jack.h>
#include <jack/statistics.h>

#include "JackDriver.hpp"
#include "Patchage.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageEvent.hpp"
#include "PatchageModule.hpp"
#include "Queue.hpp"
#include "patchage_config.h"
#ifdef HAVE_JACK_METADATA
#include <jack/metadata.h>
#include "jackey.h"
#endif

using std::endl;
using std::string;
using boost::format;

JackDriver::JackDriver(Patchage* app)
	: _app(app)
	, _client(NULL)
	, _events(128)
	, _xruns(0)
	, _xrun_delay(0)
	, _is_activated(false)
{
	_last_pos.frame = 0;
	_last_pos.valid = (jack_position_bits_t)0;
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
	if (_client)
		return;

	jack_options_t options = (!launch_daemon) ? JackNoStartServer : JackNullOption;
	_client = jack_client_open("Patchage", options, NULL);
	if (_client == NULL) {
		_app->error_msg("Jack: Unable to create client.");
		_is_activated = false;
	} else {
		jack_client_t* const client = _client;

		jack_on_shutdown(client, jack_shutdown_cb, this);
		jack_set_client_registration_callback(client, jack_client_registration_cb, this);
		jack_set_port_registration_callback(client, jack_port_registration_cb, this);
		jack_set_port_connect_callback(client, jack_port_connect_cb, this);
		jack_set_xrun_callback(client, jack_xrun_cb, this);

		_buffer_size = jack_get_buffer_size(client);

		if (!jack_activate(client)) {
			_is_activated = true;
			signal_attached.emit();
			std::stringstream ss;
			_app->info_msg("Jack: Attached.");
		} else {
			_app->error_msg("Jack: Client activation failed.");
			_is_activated = false;
		}
	}
}

void
JackDriver::detach()
{
	Glib::Mutex::Lock lock(_shutdown_mutex);
	if (_client) {
		jack_deactivate(_client);
		jack_client_close(_client);
		_client = NULL;
	}
	_is_activated = false;
	signal_detached.emit();
	_app->info_msg("Jack: Detached.");
}

static bool
is_jack_port(const PatchagePort* port)
{
	return (port->type() == JACK_AUDIO ||
	        port->type() == JACK_MIDI ||
	        port->type() == JACK_OSC ||
	        port->type() == JACK_CV);
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
JackDriver::create_port_view(Patchage*     patchage,
                             const PortID& id)
{
	assert(id.type == PortID::JACK_ID);

	jack_port_t* jack_port = jack_port_by_id(_client, id.id.jack_id);
	if (!jack_port) {
		_app->error_msg((format("Jack: Failed to find port with ID `%1%'.")
		                 % id).str());;
		return NULL;
	}

	const int jack_flags = jack_port_flags(jack_port);

	string module_name, port_name;
	port_names(id, module_name, port_name);

	ModuleType type = InputOutput;
	if (_app->conf()->get_module_split(
		    module_name, (jack_flags & JackPortIsTerminal))) {
		if (jack_flags & JackPortIsInput) {
			type = Input;
		} else {
			type = Output;
		}
	}

	PatchageModule* parent = _app->canvas()->find_module(module_name, type);
	if (!parent) {
		parent = new PatchageModule(patchage, module_name, type);
		parent->load_location();
		patchage->canvas()->add_module(module_name, parent);
	}

	if (parent->get_port(port_name)) {
		_app->error_msg((format("Jack: Module `%1%' already has port `%2%'.")
		                 % module_name % port_name).str());
		return NULL;
	}

	PatchagePort* port = create_port(*parent, jack_port, id);
	port->show();
	if (port->is_input()) {
		parent->set_is_source(false);
	}

	return port;
}

static std::string
get_property(jack_uuid_t subject, const char* key)
{
	std::string result;

#ifdef HAVE_JACK_METADATA
	char* value    = NULL;
	char* datatype = NULL;
	if (!jack_get_property(subject, key, &value, &datatype)) {
		result = value;
	}
	jack_free(datatype);
	jack_free(value);
#endif
	return result;
}

PatchagePort*
JackDriver::create_port(PatchageModule& parent, jack_port_t* port, PortID id)
{
	if (!port) {
		return NULL;
	}

	const char* const    type_str  = jack_port_type(port);
	const jack_uuid_t    uuid      = jack_port_uuid(port);
	const std::string    label     = get_property(uuid, JACK_METADATA_PRETTY_NAME);
	const std::string    order_str = get_property(uuid, JACKEY_ORDER);
	boost::optional<int> order;
	if (!order_str.empty()) {
		order = atoi(order_str.c_str());
	}

	PortType port_type;
	if (!strcmp(type_str, JACK_DEFAULT_AUDIO_TYPE)) {
		port_type = JACK_AUDIO;
		if (get_property(uuid, JACKEY_SIGNAL_TYPE) == "CV") {
			port_type = JACK_CV;
		}
	} else if (!strcmp(type_str, JACK_DEFAULT_MIDI_TYPE)) {
		port_type = JACK_MIDI;
		if (get_property(uuid, JACKEY_EVENT_TYPES) == "OSC") {
			port_type = JACK_OSC;
		}
	} else {
		_app->warning_msg((format("Jack: Port `%1%' has unknown type `%2%'.")
		                   % jack_port_name(port) % type_str).str());
		return NULL;
	}

	PatchagePort* ret(
		new PatchagePort(parent, port_type, jack_port_short_name(port),
		                 label,
		                 (jack_port_flags(port) & JackPortIsInput),
		                 _app->conf()->get_port_color(port_type),
		                 _app->show_human_names(),
		                 order));

	if (id.type != PortID::NULL_PORT_ID) {
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
	const char** ports;
	jack_port_t* port;

	// Jack can take _client away from us at any time throughout here :/
	// Shortest locks possible is the best solution I can figure out

	Glib::Mutex::Lock lock(_shutdown_mutex);

	if (_client == NULL) {
		shutdown();
		return;
	}

	ports = jack_get_ports(_client, NULL, NULL, 0); // get all existing ports

	if (!ports) {
		return;
	}

	string client1_name;
	string port1_name;
	string client2_name;
	string port2_name;
	size_t colon;

	// Add all ports
	for (int i = 0; ports[i]; ++i) {
		port = jack_port_by_name(_client, ports[i]);

		client1_name = ports[i];
		client1_name = client1_name.substr(0, client1_name.find(":"));

		ModuleType type = InputOutput;
		if (_app->conf()->get_module_split(
			    client1_name,
			    (jack_port_flags(port) & JackPortIsTerminal))) {
			if (jack_port_flags(port) & JackPortIsInput) {
				type = Input;
			} else {
				type = Output;
			}
		}

		PatchageModule* m = _app->canvas()->find_module(client1_name, type);

		if (!m) {
			m = new PatchageModule(_app, client1_name, type);
			m->load_location();
			_app->canvas()->add_module(client1_name, m);
		}

		if (!m->get_port(jack_port_short_name(port)))
			create_port(*m, port, PortID());
	}

	// Add all connections
	for (int i = 0; ports[i]; ++i) {
		port = jack_port_by_name(_client, ports[i]);
		const char** connected_ports = jack_port_get_all_connections(_client, port);

		client1_name = ports[i];
		colon        = client1_name.find(':');
		port1_name   = client1_name.substr(colon + 1);
		client1_name = client1_name.substr(0, colon);

		const ModuleType port1_type = (jack_port_flags(port) & JackPortIsInput)
			? Input : Output;

		PatchageModule* client1_module
			= _app->canvas()->find_module(client1_name, port1_type);

		if (connected_ports) {
			for (int j = 0; connected_ports[j]; ++j) {

				client2_name = connected_ports[j];
				colon        = client2_name.find(':');
				port2_name   = client2_name.substr(colon+1);
				client2_name = client2_name.substr(0, colon);

				const ModuleType port2_type = (port1_type == Input) ? Output : Input;

				PatchageModule* client2_module
					= _app->canvas()->find_module(client2_name, port2_type);

				Ganv::Port* port1 = client1_module->get_port(port1_name);
				Ganv::Port* port2 = client2_module->get_port(port2_name);

				if (!port1 || !port2)
					continue;

				Ganv::Port* src = NULL;
				Ganv::Port* dst = NULL;

				if (port1->is_output() && port2->is_input()) {
					src = port1;
					dst = port2;
				} else {
					src = port2;
					dst = port1;
				}

				if (src && dst && !_app->canvas()->get_edge(src, dst))
					_app->canvas()->make_connection(src, dst);
			}

			jack_free(connected_ports);
		}
	}

	jack_free(ports);
}

bool
JackDriver::port_names(const PortID& id,
                       string&       module_name,
                       string&       port_name)
{
	jack_port_t* jack_port = NULL;

	if (id.type == PortID::JACK_ID)
		jack_port = jack_port_by_id(_client, id.id.jack_id);

	if (!jack_port) {
		module_name.clear();
		port_name.clear();
		return false;
	}

	const string full_name = jack_port_name(jack_port);

	module_name = full_name.substr(0, full_name.find(":"));
	port_name   = full_name.substr(full_name.find(":")+1);

	return true;
}

/** Connects two Jack audio ports.
 * To be called from GTK thread only.
 * \return Whether connection succeeded.
 */
bool
JackDriver::connect(PatchagePort* src_port,
                    PatchagePort* dst_port)
{
	if (_client == NULL)
		return false;

	int result = jack_connect(_client, src_port->full_name().c_str(), dst_port->full_name().c_str());

	if (result == 0)
		_app->info_msg(string("Jack: Connected ")
			+ src_port->full_name() + " => " + dst_port->full_name());
	else
		_app->error_msg(string("Jack: Unable to connect ")
			+ src_port->full_name() + " => " + dst_port->full_name());

	return (!result);
}

/** Disconnects two Jack audio ports.
 * To be called from GTK thread only.
 * \return Whether disconnection succeeded.
 */
bool
JackDriver::disconnect(PatchagePort* const src_port,
                       PatchagePort* const dst_port)
{
	if (_client == NULL)
		return false;

	int result = jack_disconnect(_client, src_port->full_name().c_str(), dst_port->full_name().c_str());

	if (result == 0)
		_app->info_msg(string("Jack: Disconnected ")
			+ src_port->full_name() + " => " + dst_port->full_name());
	else
		_app->error_msg(string("Jack: Unable to disconnect ")
			+ src_port->full_name() + " => " + dst_port->full_name());

	return (!result);
}

void
JackDriver::jack_client_registration_cb(const char* name, int registered, void* jack_driver)
{
	JackDriver* me = reinterpret_cast<JackDriver*>(jack_driver);
	assert(me->_client);

	if (registered) {
		me->_events.push(PatchageEvent(PatchageEvent::CLIENT_CREATION, name));
	} else {
		me->_events.push(PatchageEvent(PatchageEvent::CLIENT_DESTRUCTION, name));
	}
}

void
JackDriver::jack_port_registration_cb(jack_port_id_t port_id, int registered, void* jack_driver)
{
	JackDriver* me = reinterpret_cast<JackDriver*>(jack_driver);
	assert(me->_client);

	if (registered) {
		me->_events.push(PatchageEvent(PatchageEvent::PORT_CREATION, port_id));
	} else {
		me->_events.push(PatchageEvent(PatchageEvent::PORT_DESTRUCTION, port_id));
	}
}

void
JackDriver::jack_port_connect_cb(jack_port_id_t src, jack_port_id_t dst, int connect, void* jack_driver)
{
	JackDriver* me = reinterpret_cast<JackDriver*>(jack_driver);
	assert(me->_client);

	if (connect) {
		me->_events.push(PatchageEvent(PatchageEvent::CONNECTION, src, dst));
	} else {
		me->_events.push(PatchageEvent(PatchageEvent::DISCONNECTION, src, dst));
	}
}

int
JackDriver::jack_xrun_cb(void* jack_driver)
{
	JackDriver* me = reinterpret_cast<JackDriver*>(jack_driver);
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
	JackDriver* me = reinterpret_cast<JackDriver*>(jack_driver);
	me->_app->info_msg("Jack: Shutdown.");
	Glib::Mutex::Lock lock(me->_shutdown_mutex);
	me->_client = NULL;
	me->_is_activated = false;
	me->signal_detached.emit();
}

jack_nframes_t
JackDriver::buffer_size()
{
	if (_is_activated)
		return _buffer_size;
	else
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
		_app->error_msg("[JACK] Unable to set buffer size");
		return false;
	} else {
		return true;
	}
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
