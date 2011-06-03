/* This file is part of Patchage.
 * Copyright 2007-2011 David Robillard <http://drobilla.net>
 *
 * Patchage is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Patchage is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef PATCHAGE_JACKDRIVER_HPP
#define PATCHAGE_JACKDRIVER_HPP

#include <string>
#include <boost/shared_ptr.hpp>
#include <jack/jack.h>
#include <jack/statistics.h>
#include <glibmm/thread.h>
#include "Driver.hpp"

class Patchage;
class PatchageEvent;
class PatchageFlowCanvas;
class PatchagePort;
class PatchageModule;

/** Handles all externally driven functionality, registering ports etc.
 *
 * Jack callbacks and connect methods and things like that live here.
 * Right now just for jack ports, but that will change...
 */
class JackDriver : public Driver
{
public:
	explicit JackDriver(Patchage* app);
	~JackDriver();

	void attach(bool launch_daemon);
	void detach();

	bool is_attached() const { return (_client != NULL); }
	bool is_realtime() const { return _client && jack_is_realtime(_client); }

	void refresh();
	void destroy_all();

	bool port_names(const PortID& id,
	                std::string&  module_name,
	                std::string&  port_name);

	boost::shared_ptr<PatchagePort> create_port_view(
			Patchage*     patchage,
			const PortID& id);

	bool connect(boost::shared_ptr<PatchagePort> src,
	             boost::shared_ptr<PatchagePort> dst);

	bool disconnect(boost::shared_ptr<PatchagePort> src,
	                boost::shared_ptr<PatchagePort> dst);

	uint32_t get_xruns() { return _xruns; }
	void     reset_xruns();

	float  get_max_dsp_load();
	void   reset_max_dsp_load();

	jack_client_t* client() { return _client; }

	jack_nframes_t sample_rate() { return jack_get_sample_rate(_client); }
	jack_nframes_t buffer_size();
	bool           set_buffer_size(jack_nframes_t size);

	void process_events(Patchage* app);

private:
	boost::shared_ptr<PatchagePort> create_port(
		boost::shared_ptr<PatchageModule> parent,
		jack_port_t*                      port,
		PortID                            id);

	static void error_cb(const char* msg);

	void shutdown();

	static void jack_client_registration_cb(const char* name, int registered, void* me);
	static void jack_port_registration_cb(jack_port_id_t port_id, int registered, void* me);
	static void jack_port_connect_cb(jack_port_id_t src, jack_port_id_t dst, int connect, void* me);
	static int  jack_graph_order_cb(void* me);
	static int  jack_buffer_size_cb(jack_nframes_t buffer_size, void* me);
	static int  jack_xrun_cb(void* me);
	static void jack_shutdown_cb(void* me);

	Patchage*      _app;
	jack_client_t* _client;

	Raul::SRSWQueue<PatchageEvent> _events;

	Glib::Mutex _shutdown_mutex;

	jack_position_t _last_pos;
	jack_nframes_t  _buffer_size;
	uint32_t        _xruns;
	float           _xrun_delay;
	bool            _is_activated :1;
};

#endif // PATCHAGE_JACKDRIVER_HPP
