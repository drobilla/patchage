/* This file is part of Patchage.
 * Copyright 2008-2020 David Robillard <d@drobilla.net>
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

#ifndef PATCHAGE_JACKDBUSDRIVER_HPP
#define PATCHAGE_JACKDBUSDRIVER_HPP

#include "Driver.hpp"
#include "Patchage.hpp"
#include "PatchageModule.hpp"

#include <dbus/dbus.h>
#include <glibmm/thread.h>
#include <jack/jack.h>
#include <jack/statistics.h>

#include <string>

class ILog;
class PatchageCanvas;
class PatchageEvent;
class PatchagePort;

class JackDriver : public Driver
{
public:
	explicit JackDriver(Patchage* app, ILog& log);

	JackDriver(const JackDriver&) = delete;
	JackDriver& operator=(const JackDriver&) = delete;

	JackDriver(JackDriver&&) = delete;
	JackDriver& operator=(JackDriver&&) = delete;

	~JackDriver() override;

	void attach(bool launch_daemon) override;
	void detach() override;

	bool is_attached() const override;
	bool is_realtime() const;

	void refresh() override;
	void destroy_all() override;

	bool connect(PortID tail_id, PortID head_id) override;
	bool disconnect(PortID tail_id, PortID head_id) override;

	uint32_t get_xruns();
	void     reset_xruns();
	float    get_max_dsp_load();
	void     reset_max_dsp_load();

	float          sample_rate();
	jack_nframes_t buffer_size();
	bool           set_buffer_size(jack_nframes_t size);

	void process_events(Patchage*) override {}

	PatchagePort*
	create_port_view(Patchage* patchage, const PortID& ref) override;

private:
	void error_msg(const std::string& msg) const;
	void info_msg(const std::string& msg) const;

	PatchageModule*
	find_or_create_module(ModuleType type, const std::string& name);

	void add_port(PatchageModule*    module,
	              PortType           type,
	              PortID             id,
	              const std::string& name,
	              bool               is_input);

	void add_port(dbus_uint64_t client_id,
	              const char*   client_name,
	              dbus_uint64_t port_id,
	              const char*   port_name,
	              dbus_uint32_t port_flags,
	              dbus_uint32_t port_type);

	void remove_port(dbus_uint64_t client_id,
	                 const char*   client_name,
	                 dbus_uint64_t port_id,
	                 const char*   port_name);

	void connect_ports(dbus_uint64_t connection_id,
	                   dbus_uint64_t client1_id,
	                   const char*   client1_name,
	                   dbus_uint64_t port1_id,
	                   const char*   port1_name,
	                   dbus_uint64_t client2_id,
	                   const char*   client2_name,
	                   dbus_uint64_t port2_id,
	                   const char*   port2_name);

	void disconnect_ports(dbus_uint64_t connection_id,
	                      dbus_uint64_t client1_id,
	                      const char*   client1_name,
	                      dbus_uint64_t port1_id,
	                      const char*   port1_name,
	                      dbus_uint64_t client2_id,
	                      const char*   client2_name,
	                      dbus_uint64_t port2_id,
	                      const char*   port2_name);

	bool call(bool          response_expected,
	          const char*   iface,
	          const char*   method,
	          DBusMessage** reply_ptr_ptr,
	          int           in_type,
	          ...);

	void update_attached();

	bool is_started();

	void start_server();

	void stop_server();

	void refresh_internal(bool force);

	static DBusHandlerResult dbus_message_hook(DBusConnection* connection,
	                                           DBusMessage*    message,
	                                           void*           jack_driver);

	void on_jack_appeared();

	void on_jack_disappeared();

	Patchage*       _app;
	ILog&           _log;
	DBusError       _dbus_error;
	DBusConnection* _dbus_connection;
	float           _max_dsp_load;

	bool _server_responding;
	bool _server_started;

	dbus_uint64_t _graph_version;
};

#endif // PATCHAGE_JACKDBUSDRIVER_HPP
