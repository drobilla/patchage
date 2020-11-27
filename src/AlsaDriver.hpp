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

#ifndef PATCHAGE_ALSADRIVER_HPP
#define PATCHAGE_ALSADRIVER_HPP

#include "Driver.hpp"
#include "PatchageModule.hpp"

#include <alsa/asoundlib.h>
#include <pthread.h>

#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <string>

class Patchage;
class PatchagePort;

/** Handles all externally driven functionality, registering ports etc.
 */
class AlsaDriver : public Driver
{
public:
	explicit AlsaDriver(Patchage* app);

	~AlsaDriver() override;

	void attach(bool launch_daemon) override;
	void detach() override;

	bool is_attached() const override { return (_seq != nullptr); }

	void refresh() override;
	void destroy_all() override;

	PatchagePort*
	create_port_view(Patchage* patchage, const PortID& id) override;

	bool connect(PatchagePort* src_port, PatchagePort* dst_port) override;

	bool disconnect(PatchagePort* src_port, PatchagePort* dst_port) override;

	void print_addr(snd_seq_addr_t addr);

	void process_events(Patchage* app) override;

private:
	bool         create_refresh_port();
	static void* refresh_main(void* me);
	void         _refresh_main();

	PatchageModule* find_module(uint8_t client_id, ModuleType type);

	PatchageModule* find_or_create_module(Patchage*          patchage,
	                                      uint8_t            client_id,
	                                      const std::string& client_name,
	                                      ModuleType         type);

	void create_port_view_internal(snd_seq_addr_t   addr,
	                               PatchageModule*& parent,
	                               PatchagePort*&   port);

	PatchagePort* create_port(PatchageModule&    parent,
	                          const std::string& name,
	                          bool               is_input,
	                          snd_seq_addr_t     addr);

	Patchage*  _app;
	snd_seq_t* _seq;
	pthread_t  _refresh_thread;

	std::mutex                _events_mutex;
	std::queue<PatchageEvent> _events;

	struct SeqAddrComparator
	{
		bool operator()(const snd_seq_addr_t& a, const snd_seq_addr_t& b) const
		{
			return ((a.client < b.client) ||
			        ((a.client == b.client) && a.port < b.port));
		}
	};

	using Ignored   = std::set<snd_seq_addr_t, SeqAddrComparator>;
	using Modules   = std::multimap<uint8_t, PatchageModule*>;
	using PortAddrs = std::map<PatchagePort*, PortID>;

	Ignored   _ignored;
	Modules   _modules;
	PortAddrs _port_addrs;

	bool ignore(const snd_seq_addr_t& addr, bool add = true);
};

#endif // PATCHAGE_ALSADRIVER_HPP
