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

#include "ClientInfo.hpp"
#include "Driver.hpp"
#include "PortInfo.hpp"

#include <jack/jack.h>

#include <cstdint>
#include <mutex>
#include <string>

class ILog;

/// Driver for JACK audio and midi ports that uses libjack
class JackDriver : public Driver
{
public:
	explicit JackDriver(ILog& log, EventSink emit_event);

	JackDriver(const JackDriver&) = delete;
	JackDriver& operator=(const JackDriver&) = delete;

	JackDriver(JackDriver&&) = delete;
	JackDriver& operator=(JackDriver&&) = delete;

	~JackDriver() override;

	void attach(bool launch_daemon) override;
	void detach() override;

	bool is_attached() const override { return (_client != nullptr); }

	void refresh(const EventSink& sink) override;

	bool connect(const PortID& tail_id, const PortID& head_id) override;

	bool disconnect(const PortID& tail_id, const PortID& head_id) override;

	uint32_t get_xruns() const { return _xruns; }
	void     reset_xruns();
	float    get_max_dsp_load();
	void     reset_max_dsp_load();

	uint32_t sample_rate() { return jack_get_sample_rate(_client); }
	uint32_t buffer_size();
	bool     set_buffer_size(jack_nframes_t size);

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
	float          _xrun_delay  = 0.0f;

	bool _is_activated : 1;
};

#endif // PATCHAGE_JACKDRIVER_HPP
