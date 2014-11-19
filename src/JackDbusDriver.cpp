/* This file is part of Patchage.
 * Copyright 2008 Nedko Arnaudov <nedko@arnaudov.name>
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
#include <string>
#include <set>

#include "patchage_config.h"

#include <glib.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include <boost/format.hpp>

#include "Driver.hpp"
#include "JackDbusDriver.hpp"
#include "Patchage.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageEvent.hpp"
#include "PatchageModule.hpp"

#define JACKDBUS_SERVICE        "org.jackaudio.service"
#define JACKDBUS_OBJECT         "/org/jackaudio/Controller"
#define JACKDBUS_IFACE_CONTROL  "org.jackaudio.JackControl"
#define JACKDBUS_IFACE_PATCHBAY "org.jackaudio.JackPatchbay"

#define JACKDBUS_CALL_DEFAULT_TIMEOUT 1000 // in milliseconds

#define JACKDBUS_PORT_FLAG_INPUT       0x00000001
#define JACKDBUS_PORT_FLAG_OUTPUT      0x00000002
#define JACKDBUS_PORT_FLAG_PHYSICAL    0x00000004
#define JACKDBUS_PORT_FLAG_CAN_MONITOR 0x00000008
#define JACKDBUS_PORT_FLAG_TERMINAL    0x00000010

#define JACKDBUS_PORT_TYPE_AUDIO 0
#define JACKDBUS_PORT_TYPE_MIDI  1

//#define USE_FULL_REFRESH

JackDriver::JackDriver(Patchage* app)
	: _app(app)
	, _dbus_connection(0)
	, _max_dsp_load(0)
	, _server_responding(false)
	, _server_started(false)
	, _graph_version(0)
{
	dbus_error_init(&_dbus_error);
}

JackDriver::~JackDriver()
{
	if (_dbus_connection) {
		dbus_connection_flush(_dbus_connection);
	}

	if (dbus_error_is_set(&_dbus_error)) {
		dbus_error_free(&_dbus_error);
	}
}

static bool
is_jack_port(const PatchagePort* port)
{
	return port->type() == JACK_AUDIO || port->type() == JACK_MIDI;
}

/** Destroy all JACK (canvas) ports.
 */
void
JackDriver::destroy_all()
{
	_app->canvas()->remove_ports(is_jack_port);
}

void
JackDriver::update_attached()
{
	bool was_attached = _server_started;
	_server_started = is_started();

	if (!_server_responding) {
		if (was_attached) {
			signal_detached.emit();
		}
		return;
	}

	if (_server_started && !was_attached) {
		signal_attached.emit();
		return;
	}

	if (!_server_started && was_attached) {
		signal_detached.emit();
		return;
	}
}

void
JackDriver::on_jack_appeared()
{
	info_msg("JACK appeared.");
	update_attached();
}

void
JackDriver::on_jack_disappeared()
{
	info_msg("JACK disappeared.");

	// we are not calling update_attached() here, because it will activate jackdbus

	_server_responding = false;

	if (_server_started) {
		signal_detached.emit();
	}

	_server_started = false;
}

/** Handle signals we have subscribed for in attach(). */
DBusHandlerResult
JackDriver::dbus_message_hook(DBusConnection* connection,
                              DBusMessage*    message,
                              void*           jack_driver)
{
	const char*   client2_name;
	const char*   client_name;
	const char*   new_owner;
	const char*   object_name;
	const char*   old_owner;
	const char*   port2_name;
	const char*   port_name;
	dbus_uint32_t port_flags;
	dbus_uint32_t port_type;
	dbus_uint64_t client2_id;
	dbus_uint64_t client_id;
	dbus_uint64_t connection_id;
	dbus_uint64_t new_graph_version;
	dbus_uint64_t port2_id;
	dbus_uint64_t port_id;

	assert(jack_driver);
	JackDriver* me = reinterpret_cast<JackDriver*>(jack_driver);
	assert(me->_dbus_connection);

	if (dbus_message_is_signal(message, DBUS_INTERFACE_DBUS, "NameOwnerChanged")) {
		if (!dbus_message_get_args( message, &me->_dbus_error,
					DBUS_TYPE_STRING, &object_name,
					DBUS_TYPE_STRING, &old_owner,
					DBUS_TYPE_STRING, &new_owner,
					DBUS_TYPE_INVALID)) {
			me->error_msg(str(boost::format("dbus_message_get_args() failed to extract "
					"NameOwnerChanged signal arguments (%s)") % me->_dbus_error.message));
			dbus_error_free(&me->_dbus_error);
			return DBUS_HANDLER_RESULT_HANDLED;
		}

		if (old_owner[0] == '\0') {
			me->on_jack_appeared();
		} else if (new_owner[0] == '\0') {
			me->on_jack_disappeared();
		}
	}

#if defined(USE_FULL_REFRESH)
	if (dbus_message_is_signal(message, JACKDBUS_IFACE_PATCHBAY, "GraphChanged")) {
		if (!dbus_message_get_args(message, &me->_dbus_error,
					DBUS_TYPE_UINT64, &new_graph_version,
					DBUS_TYPE_INVALID)) {
			me->error_msg(str(boost::format("dbus_message_get_args() failed to extract "
					"GraphChanged signal arguments (%s)") % me->_dbus_error.message));
			dbus_error_free(&me->_dbus_error);
			return DBUS_HANDLER_RESULT_HANDLED;
		}

		if (!me->_server_started) {
			me->_server_started = true;
			me->signal_attached.emit();
		}

		if (new_graph_version > me->_graph_version) {
			me->refresh_internal(false);
		}

    return DBUS_HANDLER_RESULT_HANDLED;
	}
#else
// 	if (dbus_message_is_signal(message, JACKDBUS_IFACE_PATCHBAY, "ClientAppeared")) {
// 		me->info_msg("ClientAppeared");
//     return DBUS_HANDLER_RESULT_HANDLED;
// 	}

// 	if (dbus_message_is_signal(message, JACKDBUS_IFACE_PATCHBAY, "ClientDisappeared")) {
// 		me->info_msg("ClientDisappeared");
//     return DBUS_HANDLER_RESULT_HANDLED;
// 	}

	if (dbus_message_is_signal(message, JACKDBUS_IFACE_PATCHBAY, "PortAppeared")) {
		if (!dbus_message_get_args( message, &me->_dbus_error,
					DBUS_TYPE_UINT64, &new_graph_version,
					DBUS_TYPE_UINT64, &client_id,
					DBUS_TYPE_STRING, &client_name,
					DBUS_TYPE_UINT64, &port_id,
					DBUS_TYPE_STRING, &port_name,
					DBUS_TYPE_UINT32, &port_flags,
					DBUS_TYPE_UINT32, &port_type,
					DBUS_TYPE_INVALID)) {
			me->error_msg(str(boost::format("dbus_message_get_args() failed to extract "
					"PortAppeared signal arguments (%s)") % me->_dbus_error.message));
			dbus_error_free(&me->_dbus_error);
			return DBUS_HANDLER_RESULT_HANDLED;
		}

		//me->info_msg(str(boost::format("PortAppeared, %s(%llu):%s(%llu), %lu, %lu") % client_name % client_id % port_name % port_id % port_flags % port_type));

		if (!me->_server_started) {
			me->_server_started = true;
			me->signal_attached.emit();
		}

		me->add_port(client_id, client_name, port_id, port_name, port_flags, port_type);

	    return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (dbus_message_is_signal(message, JACKDBUS_IFACE_PATCHBAY, "PortDisappeared")) {
		if (!dbus_message_get_args( message, &me->_dbus_error,
					DBUS_TYPE_UINT64, &new_graph_version,
					DBUS_TYPE_UINT64, &client_id,
					DBUS_TYPE_STRING, &client_name,
					DBUS_TYPE_UINT64, &port_id,
					DBUS_TYPE_STRING, &port_name,
					DBUS_TYPE_INVALID)) {
			me->error_msg(str(boost::format("dbus_message_get_args() failed to extract "
					"PortDisappeared signal arguments (%s)") % me->_dbus_error.message));
			dbus_error_free(&me->_dbus_error);
			return DBUS_HANDLER_RESULT_HANDLED;
		}

		//me->info_msg(str(boost::format("PortDisappeared, %s(%llu):%s(%llu)") % client_name % client_id % port_name % port_id));

		if (!me->_server_started) {
			me->_server_started = true;
			me->signal_attached.emit();
		}

		me->remove_port(client_id, client_name, port_id, port_name);

	    return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (dbus_message_is_signal(message, JACKDBUS_IFACE_PATCHBAY, "PortsConnected")) {
		if (!dbus_message_get_args(message, &me->_dbus_error,
				DBUS_TYPE_UINT64, &new_graph_version,
				DBUS_TYPE_UINT64, &client_id,
				DBUS_TYPE_STRING, &client_name,
				DBUS_TYPE_UINT64, &port_id,
				DBUS_TYPE_STRING, &port_name,
				DBUS_TYPE_UINT64, &client2_id,
				DBUS_TYPE_STRING, &client2_name,
				DBUS_TYPE_UINT64, &port2_id,
				DBUS_TYPE_STRING, &port2_name,
				DBUS_TYPE_UINT64, &connection_id,
				DBUS_TYPE_INVALID)) {
			me->error_msg(str(boost::format("dbus_message_get_args() failed to extract "
					"PortsConnected signal arguments (%s)") % me->_dbus_error.message));
			dbus_error_free(&me->_dbus_error);
			return DBUS_HANDLER_RESULT_HANDLED;
		}

		if (!me->_server_started) {
			me->_server_started = true;
			me->signal_attached.emit();
		}

		me->connect_ports(
			connection_id,
			client_id, client_name,
			port_id, port_name,
			client2_id, client2_name,
			port2_id, port2_name);

    	return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (dbus_message_is_signal(message, JACKDBUS_IFACE_PATCHBAY, "PortsDisconnected")) {
		if (!dbus_message_get_args(message, &me->_dbus_error,
					DBUS_TYPE_UINT64, &new_graph_version,
					DBUS_TYPE_UINT64, &client_id,
					DBUS_TYPE_STRING, &client_name,
					DBUS_TYPE_UINT64, &port_id,
					DBUS_TYPE_STRING, &port_name,
					DBUS_TYPE_UINT64, &client2_id,
					DBUS_TYPE_STRING, &client2_name,
					DBUS_TYPE_UINT64, &port2_id,
					DBUS_TYPE_STRING, &port2_name,
					DBUS_TYPE_UINT64, &connection_id,
					DBUS_TYPE_INVALID)) {
			me->error_msg(str(boost::format("dbus_message_get_args() failed to extract "
					"PortsConnected signal arguments (%s)") % me->_dbus_error.message));
			dbus_error_free(&me->_dbus_error);
			return DBUS_HANDLER_RESULT_HANDLED;
		}

		if (!me->_server_started) {
			me->_server_started = true;
			me->signal_attached.emit();
		}

		me->disconnect_ports(
			connection_id,
			client_id, client_name,
			port_id, port_name,
			client2_id, client2_name,
			port2_id, port2_name);

	    return DBUS_HANDLER_RESULT_HANDLED;
	}
#endif

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

bool
JackDriver::call(
	bool          response_expected,
	const char*   iface,
	const char*   method,
	DBusMessage** reply_ptr_ptr,
	int           in_type, ...)
{
	DBusMessage* request_ptr;
	DBusMessage* reply_ptr;
	va_list ap;

	request_ptr = dbus_message_new_method_call(
		JACKDBUS_SERVICE,
		JACKDBUS_OBJECT,
		iface,
		method);
	if (!request_ptr) {
		throw std::runtime_error("dbus_message_new_method_call() returned 0");
	}

	va_start(ap, in_type);

	dbus_message_append_args_valist(request_ptr, in_type, ap);

	va_end(ap);

	// send message and get a handle for a reply
	reply_ptr = dbus_connection_send_with_reply_and_block(_dbus_connection, request_ptr,
			JACKDBUS_CALL_DEFAULT_TIMEOUT, &_dbus_error);

	dbus_message_unref(request_ptr);

	if (!reply_ptr) {
		if (response_expected) {
			error_msg(str(boost::format("no reply from server when calling method '%s'"
					", error is '%s'") % method % _dbus_error.message));
		}
		_server_responding = false;
		dbus_error_free(&_dbus_error);
	} else {
		_server_responding = true;
		*reply_ptr_ptr = reply_ptr;
	}

	return reply_ptr;
}

bool
JackDriver::is_started()
{
	DBusMessage* reply_ptr;
	dbus_bool_t started;

	if (!call(false, JACKDBUS_IFACE_CONTROL, "IsStarted", &reply_ptr, DBUS_TYPE_INVALID)) {
		return false;
	}

	if (!dbus_message_get_args(reply_ptr, &_dbus_error,
				DBUS_TYPE_BOOLEAN, &started,
				DBUS_TYPE_INVALID)) {
		dbus_message_unref(reply_ptr);
		dbus_error_free(&_dbus_error);
		error_msg("decoding reply of IsStarted failed.");
		return false;
	}

	dbus_message_unref(reply_ptr);

	return started;
}

void
JackDriver::start_server()
{
	DBusMessage* reply_ptr;

	if (!call(false, JACKDBUS_IFACE_CONTROL, "StartServer", &reply_ptr, DBUS_TYPE_INVALID)) {
		return;
	}

	dbus_message_unref(reply_ptr);

	update_attached();
}

void
JackDriver::stop_server()
{
	DBusMessage* reply_ptr;

	if (!call(false, JACKDBUS_IFACE_CONTROL, "StopServer", &reply_ptr, DBUS_TYPE_INVALID)) {
		return;
	}

	dbus_message_unref(reply_ptr);

	if (!_server_started) {
		_server_started = false;
		signal_detached.emit();
	}
}

void
JackDriver::attach(bool launch_daemon)
{
	// Connect to the bus
	_dbus_connection = dbus_bus_get(DBUS_BUS_SESSION, &_dbus_error);
	if (dbus_error_is_set(&_dbus_error)) {
		error_msg("dbus_bus_get() failed");
		error_msg(_dbus_error.message);
		dbus_error_free(&_dbus_error);
		return;
	}

	dbus_connection_setup_with_g_main(_dbus_connection, NULL);

	dbus_bus_add_match(_dbus_connection, "type='signal',interface='" DBUS_INTERFACE_DBUS "',member=NameOwnerChanged,arg0='org.jackaudio.service'", NULL);
#if defined(USE_FULL_REFRESH)
	dbus_bus_add_match(_dbus_connection, "type='signal',interface='" JACKDBUS_IFACE_PATCHBAY "',member=GraphChanged", NULL);
#else
	//   dbus_bus_add_match(_dbus_connection, "type='signal',interface='" JACKDBUS_IFACE_PATCHBAY "',member=ClientAppeared", NULL);
	//   dbus_bus_add_match(_dbus_connection, "type='signal',interface='" JACKDBUS_IFACE_PATCHBAY "',member=ClientDisappeared", NULL);
	dbus_bus_add_match(_dbus_connection, "type='signal',interface='" JACKDBUS_IFACE_PATCHBAY "',member=PortAppeared", NULL);
	dbus_bus_add_match(_dbus_connection, "type='signal',interface='" JACKDBUS_IFACE_PATCHBAY "',member=PortDisappeared", NULL);
	dbus_bus_add_match(_dbus_connection, "type='signal',interface='" JACKDBUS_IFACE_PATCHBAY "',member=PortsConnected", NULL);
	dbus_bus_add_match(_dbus_connection, "type='signal',interface='" JACKDBUS_IFACE_PATCHBAY "',member=PortsDisconnected", NULL);
#endif
	dbus_connection_add_filter(_dbus_connection, dbus_message_hook, this, NULL);

	update_attached();

	if (!_server_responding) {
		return;
	}

	if (launch_daemon) {
		start_server();
	}
}

void
JackDriver::detach()
{
	stop_server();
}

bool
JackDriver::is_attached() const
{
	return _dbus_connection && _server_responding;
}

void
JackDriver::add_port(PatchageModule*    module,
                     PortType           type,
                     const std::string& name,
                     bool               is_input)
{
	if (module->get_port(name)) {
		return;
	}

	new PatchagePort(
		*module,
		type,
		name,
		"",  // TODO: pretty name
		is_input,
		_app->conf()->get_port_color(type),
		_app->show_human_names());
}

void
JackDriver::add_port(dbus_uint64_t client_id,
                     const char*   client_name,
                     dbus_uint64_t port_id,
                     const char*   port_name,
                     dbus_uint32_t port_flags,
                     dbus_uint32_t port_type)
{
	PortType local_port_type;

	switch (port_type) {
	case JACKDBUS_PORT_TYPE_AUDIO:
		local_port_type = JACK_AUDIO;
		break;
	case JACKDBUS_PORT_TYPE_MIDI:
		local_port_type = JACK_MIDI;
		break;
	default:
		error_msg("Unknown JACK D-Bus port type");
		return;
	}

	ModuleType type = InputOutput;
	if (_app->conf()->get_module_split(client_name, port_flags & JACKDBUS_PORT_FLAG_TERMINAL)) {
		if (port_flags & JACKDBUS_PORT_FLAG_INPUT) {
			type = Input;
		} else {
			type = Output;
		}
	}

	PatchageModule* module = find_or_create_module(type, client_name);

	add_port(module, local_port_type, port_name, port_flags & JACKDBUS_PORT_FLAG_INPUT);
}

void
JackDriver::remove_port(dbus_uint64_t client_id,
                        const char*   client_name,
                        dbus_uint64_t port_id,
                        const char*   port_name)
{
	PatchagePort* port = _app->canvas()->find_port_by_name(client_name, port_name);
	if (!port) {
		error_msg("Unable to remove unknown port");
		return;
	}

	PatchageModule* module = dynamic_cast<PatchageModule*>(port->get_module());

	delete port;

	// No empty modules (for now)
	if (module->num_ports() == 0) {
		delete module;
	}

	if (_app->canvas()->empty()) {
		if (_server_started) {
			signal_detached.emit();
		}

		_server_started = false;
	}
}

PatchageModule*
JackDriver::find_or_create_module(
	ModuleType type,
	const std::string& name)
{
	PatchageModule* module = _app->canvas()->find_module(name, type);

	if (!module) {
		module = new PatchageModule(_app, name, type);
		module->load_location();
		_app->canvas()->add_module(name, module);
	}

	return module;
}

void
JackDriver::connect_ports(dbus_uint64_t connection_id,
                          dbus_uint64_t client1_id,
                          const char*   client1_name,
                          dbus_uint64_t port1_id,
                          const char*   port1_name,
                          dbus_uint64_t client2_id,
                          const char*   client2_name,
                          dbus_uint64_t port2_id,
                          const char*   port2_name)
{
	PatchagePort* port1 = _app->canvas()->find_port_by_name(client1_name, port1_name);
	if (!port1) {
		error_msg((std::string)"Unable to connect unknown port '" + port1_name + "' of client '" + client1_name + "'");
		return;
	}

	PatchagePort* port2 = _app->canvas()->find_port_by_name(client2_name, port2_name);
	if (!port2) {
		error_msg((std::string)"Unable to connect unknown port '" + port2_name + "' of client '" + client2_name + "'");
		return;
	}

	_app->canvas()->connect(port1, port2);
}

void
JackDriver::disconnect_ports(dbus_uint64_t connection_id,
                             dbus_uint64_t client1_id,
                             const char*   client1_name,
                             dbus_uint64_t port1_id,
                             const char*   port1_name,
                             dbus_uint64_t client2_id,
                             const char*   client2_name,
                             dbus_uint64_t port2_id,
                             const char*   port2_name)
{
	PatchagePort* port1 = _app->canvas()->find_port_by_name(client1_name, port1_name);
	if (!port1) {
		error_msg((std::string)"Unable to disconnect unknown port '" + port1_name + "' of client '" + client1_name + "'");
		return;
	}

	PatchagePort* port2 = _app->canvas()->find_port_by_name(client2_name, port2_name);
	if (!port2) {
		error_msg((std::string)"Unable to disconnect unknown port '" + port2_name + "' of client '" + client2_name + "'");
		return;
	}

	_app->canvas()->disconnect(port1, port2);
}

void
JackDriver::refresh_internal(bool force)
{
	DBusMessage*    reply_ptr;
	DBusMessageIter iter;
	dbus_uint64_t   version;
	const char*     reply_signature;
	DBusMessageIter clients_array_iter;
	DBusMessageIter client_struct_iter;
	DBusMessageIter ports_array_iter;
	DBusMessageIter port_struct_iter;
	DBusMessageIter connections_array_iter;
	DBusMessageIter connection_struct_iter;
	dbus_uint64_t   client_id;
	const char*     client_name;
	dbus_uint64_t   port_id;
	const char*     port_name;
	dbus_uint32_t   port_flags;
	dbus_uint32_t   port_type;
	dbus_uint64_t   client2_id;
	const char*     client2_name;
	dbus_uint64_t   port2_id;
	const char*     port2_name;
	dbus_uint64_t   connection_id;

	if (force) {
		version = 0; // workaround module split/join stupidity
	} else {
		version = _graph_version;
	}

	if (!call(true, JACKDBUS_IFACE_PATCHBAY, "GetGraph", &reply_ptr, DBUS_TYPE_UINT64, &version, DBUS_TYPE_INVALID)) {
		error_msg("GetGraph() failed.");
		return;
	}

	reply_signature = dbus_message_get_signature(reply_ptr);

	if (strcmp(reply_signature, "ta(tsa(tsuu))a(tstststst)") != 0) {
		error_msg((std::string)"GetGraph() reply signature mismatch. " + reply_signature);
		goto unref;
	}

	dbus_message_iter_init(reply_ptr, &iter);

	//info_msg((string)"version " + (char)dbus_message_iter_get_arg_type(&iter));
	dbus_message_iter_get_basic(&iter, &version);
	dbus_message_iter_next(&iter);

	if (!force && version <= _graph_version) {
		goto unref;
	}

	destroy_all();

	//info_msg(str(boost::format("got new graph version %llu") % version));
	_graph_version = version;

	//info_msg((string)"clients " + (char)dbus_message_iter_get_arg_type(&iter));

	for (dbus_message_iter_recurse(&iter, &clients_array_iter);
			 dbus_message_iter_get_arg_type(&clients_array_iter) != DBUS_TYPE_INVALID;
			 dbus_message_iter_next(&clients_array_iter)) {
		//info_msg((string)"a client " + (char)dbus_message_iter_get_arg_type(&clients_array_iter));
		dbus_message_iter_recurse(&clients_array_iter, &client_struct_iter);

		dbus_message_iter_get_basic(&client_struct_iter, &client_id);
		dbus_message_iter_next(&client_struct_iter);

		dbus_message_iter_get_basic(&client_struct_iter, &client_name);
		dbus_message_iter_next(&client_struct_iter);

		//info_msg((string)"client '" + client_name + "'");

		for (dbus_message_iter_recurse(&client_struct_iter, &ports_array_iter);
				 dbus_message_iter_get_arg_type(&ports_array_iter) != DBUS_TYPE_INVALID;
				 dbus_message_iter_next(&ports_array_iter)) {
			//info_msg((string)"a port " + (char)dbus_message_iter_get_arg_type(&ports_array_iter));
			dbus_message_iter_recurse(&ports_array_iter, &port_struct_iter);

			dbus_message_iter_get_basic(&port_struct_iter, &port_id);
			dbus_message_iter_next(&port_struct_iter);

			dbus_message_iter_get_basic(&port_struct_iter, &port_name);
			dbus_message_iter_next(&port_struct_iter);

			dbus_message_iter_get_basic(&port_struct_iter, &port_flags);
			dbus_message_iter_next(&port_struct_iter);

			dbus_message_iter_get_basic(&port_struct_iter, &port_type);
			dbus_message_iter_next(&port_struct_iter);

			//info_msg((string)"port: " + port_name);

			add_port(client_id, client_name, port_id, port_name, port_flags, port_type);
		}

		dbus_message_iter_next(&client_struct_iter);
	}

	dbus_message_iter_next(&iter);

	for (dbus_message_iter_recurse(&iter, &connections_array_iter);
			 dbus_message_iter_get_arg_type(&connections_array_iter) != DBUS_TYPE_INVALID;
			 dbus_message_iter_next(&connections_array_iter)) {
		//info_msg((string)"a connection " + (char)dbus_message_iter_get_arg_type(&connections_array_iter));
		dbus_message_iter_recurse(&connections_array_iter, &connection_struct_iter);

		dbus_message_iter_get_basic(&connection_struct_iter, &client_id);
		dbus_message_iter_next(&connection_struct_iter);

		dbus_message_iter_get_basic(&connection_struct_iter, &client_name);
		dbus_message_iter_next(&connection_struct_iter);

		dbus_message_iter_get_basic(&connection_struct_iter, &port_id);
		dbus_message_iter_next(&connection_struct_iter);

		dbus_message_iter_get_basic(&connection_struct_iter, &port_name);
		dbus_message_iter_next(&connection_struct_iter);

		dbus_message_iter_get_basic(&connection_struct_iter, &client2_id);
		dbus_message_iter_next(&connection_struct_iter);

		dbus_message_iter_get_basic(&connection_struct_iter, &client2_name);
		dbus_message_iter_next(&connection_struct_iter);

		dbus_message_iter_get_basic(&connection_struct_iter, &port2_id);
		dbus_message_iter_next(&connection_struct_iter);

		dbus_message_iter_get_basic(&connection_struct_iter, &port2_name);
		dbus_message_iter_next(&connection_struct_iter);

		dbus_message_iter_get_basic(&connection_struct_iter, &connection_id);
		dbus_message_iter_next(&connection_struct_iter);

		//info_msg(str(boost::format("connection(%llu) %s(%llu):%s(%llu) <-> %s(%llu):%s(%llu)") %
		//		connection_id %
		//		client_name %
		//		client_id %
		//		port_name %
		//		port_id %
		//		client2_name %
		//		client2_id %
		//		port2_name %
		//		port2_id));

		connect_ports(
			connection_id,
			client_id, client_name,
			port_id, port_name,
			client2_id, client2_name,
			port2_id, port2_name);
	}

unref:
	dbus_message_unref(reply_ptr);
}

void
JackDriver::refresh()
{
	refresh_internal(true);
}

bool
JackDriver::connect(PatchagePort* src,
                    PatchagePort* dst)
{
	const char* client1_name = src->get_module()->get_label();
	const char* port1_name   = src->get_label();
	const char* client2_name = dst->get_module()->get_label();
	const char* port2_name   = dst->get_label();

	DBusMessage* reply_ptr;
	if (!call(true, JACKDBUS_IFACE_PATCHBAY, "ConnectPortsByName", &reply_ptr,
				DBUS_TYPE_STRING, &client1_name,
				DBUS_TYPE_STRING, &port1_name,
				DBUS_TYPE_STRING, &client2_name,
				DBUS_TYPE_STRING, &port2_name,
				DBUS_TYPE_INVALID)) {
		error_msg("ConnectPortsByName() failed.");
		return false;
	}

	return true;
}

bool
JackDriver::disconnect(PatchagePort* src,
                       PatchagePort* dst)
{
	const char* client1_name = src->get_module()->get_label();
	const char* port1_name   = src->get_label();
	const char* client2_name = dst->get_module()->get_label();
	const char* port2_name   = dst->get_label();

	DBusMessage* reply_ptr;
	if (!call(true, JACKDBUS_IFACE_PATCHBAY, "DisconnectPortsByName", &reply_ptr,
				DBUS_TYPE_STRING, &client1_name,
				DBUS_TYPE_STRING, &port1_name,
				DBUS_TYPE_STRING, &client2_name,
				DBUS_TYPE_STRING, &port2_name,
				DBUS_TYPE_INVALID)) {
		error_msg("DisconnectPortsByName() failed.");
		return false;
	}

	return true;
}

jack_nframes_t
JackDriver::buffer_size()
{
	DBusMessage* reply_ptr;
	dbus_uint32_t buffer_size;

	if (_server_responding && !_server_started) {
		goto fail;
	}

	if (!call(true, JACKDBUS_IFACE_CONTROL, "GetBufferSize", &reply_ptr, DBUS_TYPE_INVALID)) {
		goto fail;
	}

	if (!dbus_message_get_args(reply_ptr, &_dbus_error, DBUS_TYPE_UINT32, &buffer_size, DBUS_TYPE_INVALID)) {
		dbus_message_unref(reply_ptr);
		dbus_error_free(&_dbus_error);
		error_msg("decoding reply of GetBufferSize failed.");
		goto fail;
	}

	dbus_message_unref(reply_ptr);

	return buffer_size;

fail:
	return 4096; // something fake, patchage needs it to match combobox value
}

bool
JackDriver::set_buffer_size(jack_nframes_t size)
{
	DBusMessage* reply_ptr;
	dbus_uint32_t buffer_size;

	buffer_size = size;

	if (!call(true, JACKDBUS_IFACE_CONTROL, "SetBufferSize", &reply_ptr, DBUS_TYPE_UINT32, &buffer_size, DBUS_TYPE_INVALID)) {
		return false;
	}

	dbus_message_unref(reply_ptr);

	return true;
}

float
JackDriver::sample_rate()
{
	DBusMessage* reply_ptr;
	double sample_rate;

	if (!call(true, JACKDBUS_IFACE_CONTROL, "GetSampleRate", &reply_ptr, DBUS_TYPE_INVALID)) {
		return false;
	}

	if (!dbus_message_get_args(reply_ptr, &_dbus_error, DBUS_TYPE_DOUBLE, &sample_rate, DBUS_TYPE_INVALID)) {
		dbus_message_unref(reply_ptr);
		dbus_error_free(&_dbus_error);
		error_msg("decoding reply of GetSampleRate failed.");
		return false;
	}

	dbus_message_unref(reply_ptr);

	return sample_rate;
}

bool
JackDriver::is_realtime() const
{
	DBusMessage* reply_ptr;
	dbus_bool_t  realtime;

	JackDriver* me = const_cast<JackDriver*>(this);
	if (!me->call(true, JACKDBUS_IFACE_CONTROL, "IsRealtime",
	              &reply_ptr, DBUS_TYPE_INVALID)) {
		return false;
	}

	if (!dbus_message_get_args(reply_ptr, &me->_dbus_error, DBUS_TYPE_BOOLEAN,
	                           &realtime, DBUS_TYPE_INVALID)) {
		dbus_message_unref(reply_ptr);
		dbus_error_free(&me->_dbus_error);
		error_msg("decoding reply of IsRealtime failed.");
		return false;
	}

	dbus_message_unref(reply_ptr);

	return realtime;
}

size_t
JackDriver::get_xruns()
{
	DBusMessage*  reply_ptr;
	dbus_uint32_t xruns;

	if (_server_responding && !_server_started) {
		return 0;
	}

	if (!call(true, JACKDBUS_IFACE_CONTROL, "GetXruns", &reply_ptr, DBUS_TYPE_INVALID)) {
		return 0;
	}

	if (!dbus_message_get_args(reply_ptr, &_dbus_error, DBUS_TYPE_UINT32, &xruns, DBUS_TYPE_INVALID)) {
		dbus_message_unref(reply_ptr);
		dbus_error_free(&_dbus_error);
		error_msg("decoding reply of GetXruns failed.");
		return 0;
	}

	dbus_message_unref(reply_ptr);

	return xruns;
}

void
JackDriver::reset_xruns()
{
	DBusMessage* reply_ptr;

	if (!call(true, JACKDBUS_IFACE_CONTROL, "ResetXruns", &reply_ptr, DBUS_TYPE_INVALID)) {
		return;
	}

	dbus_message_unref(reply_ptr);
}

float
JackDriver::get_max_dsp_load()
{
	DBusMessage* reply_ptr;
	double       load;

	if (_server_responding && !_server_started) {
		return 0.0;
	}

	if (!call(true, JACKDBUS_IFACE_CONTROL, "GetLoad", &reply_ptr, DBUS_TYPE_INVALID)) {
		return 0.0;
	}

	if (!dbus_message_get_args(reply_ptr, &_dbus_error, DBUS_TYPE_DOUBLE, &load, DBUS_TYPE_INVALID)) {
		dbus_message_unref(reply_ptr);
		dbus_error_free(&_dbus_error);
		error_msg("decoding reply of GetLoad failed.");
		return 0.0;
	}

	dbus_message_unref(reply_ptr);

	load /= 100.0; // convert from percent to [0..1]

	if (load > _max_dsp_load) {
		_max_dsp_load = load;
	}

	return _max_dsp_load;
}


void
JackDriver::reset_max_dsp_load()
{
	_max_dsp_load = 0.0;
}

PatchagePort*
JackDriver::create_port_view(Patchage*     patchage,
                             const PortID& id)
{
	assert(false);  // we dont use events at all
	return NULL;
}

void
JackDriver::error_msg(const std::string& msg) const
{
	_app->error_msg((std::string)"Jack: " + msg);
}

void
JackDriver::info_msg(const std::string& msg) const
{
	_app->info_msg((std::string)"Jack: " + msg);
}

