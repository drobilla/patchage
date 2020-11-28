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

#ifndef PATCHAGE_JACKDRIVER_HPP
#define PATCHAGE_JACKDRIVER_HPP

#include "Driver.hpp"

#include <glibmm/thread.h>
#include <jack/jack.h>

#include <mutex>
#include <queue>
#include <string>

class ILog;
class Patchage;
class PatchageCanvas;
class PatchageEvent;
class PatchageModule;
class PatchagePort;

/// Driver for JACK audio and midi ports
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

	bool is_attached() const override { return (_client != nullptr); }

	bool is_realtime() const { return _client && jack_is_realtime(_client); }

	void refresh() override;
	void destroy_all() override;

	bool port_names(const PortID& id,
	                std::string&  module_name,
	                std::string&  port_name);

	PatchagePort*
	create_port_view(Patchage* patchage, const PortID& id) override;

	bool connect(PortID tail_id, PortID head_id) override;
	bool disconnect(PortID tail_id, PortID head_id) override;

	uint32_t get_xruns() const { return _xruns; }
	void     reset_xruns();
	float    get_max_dsp_load();
	void     reset_max_dsp_load();

	jack_client_t* client() { return _client; }

	jack_nframes_t sample_rate() { return jack_get_sample_rate(_client); }
	jack_nframes_t buffer_size();
	bool           set_buffer_size(jack_nframes_t size);

	void process_events(Patchage* app) override;

private:
	PatchagePort*
	create_port(PatchageModule& parent, jack_port_t* port, const PortID& id);

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

	Patchage*      _app;
	ILog&          _log;
	jack_client_t* _client;

	std::queue<PatchageEvent> _events;

	std::mutex _shutdown_mutex;

	jack_position_t _last_pos;
	jack_nframes_t  _buffer_size;
	uint32_t        _xruns;
	float           _xrun_delay;
	bool            _is_activated : 1;
};

#endif // PATCHAGE_JACKDRIVER_HPP
