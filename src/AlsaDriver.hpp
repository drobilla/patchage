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

#include <alsa/asoundlib.h>
#include <pthread.h>

#include <map>
#include <set>
#include <string>

class ILog;

/// Driver for ALSA Sequencer ports
class AlsaDriver : public Driver
{
public:
	explicit AlsaDriver(ILog& log, EventSink emit_event);

	AlsaDriver(const AlsaDriver&) = delete;
	AlsaDriver& operator=(const AlsaDriver&) = delete;

	AlsaDriver(AlsaDriver&&) = delete;
	AlsaDriver& operator=(AlsaDriver&&) = delete;

	~AlsaDriver() override;

	void attach(bool launch_daemon) override;
	void detach() override;

	bool is_attached() const override { return (_seq != nullptr); }

	void refresh(const EventSink& sink) override;

	bool connect(PortID tail_id, PortID head_id) override;
	bool disconnect(PortID tail_id, PortID head_id) override;

	void print_addr(snd_seq_addr_t addr);

private:
	bool         create_refresh_port();
	static void* refresh_main(void* me);
	void         _refresh_main();

	ILog&      _log;
	snd_seq_t* _seq;
	pthread_t  _refresh_thread;

	struct SeqAddrComparator
	{
		bool operator()(const snd_seq_addr_t& a, const snd_seq_addr_t& b) const
		{
			return ((a.client < b.client) ||
			        ((a.client == b.client) && a.port < b.port));
		}
	};

	using Ignored = std::set<snd_seq_addr_t, SeqAddrComparator>;

	Ignored _ignored;

	bool ignore(const snd_seq_addr_t& addr, bool add = true);
};

#endif // PATCHAGE_ALSADRIVER_HPP
