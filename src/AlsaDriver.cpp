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

#include "AlsaDriver.hpp"

#include "ClientID.hpp"
#include "Patchage.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageModule.hpp"
#include "PatchagePort.hpp"

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
PATCHAGE_RESTORE_WARNINGS

#include <cassert>
#include <set>
#include <string>
#include <utility>

namespace {

inline PortID
addr_to_id(const snd_seq_addr_t& addr, bool is_input)
{
	return PortID::alsa(addr.client, addr.port, is_input);
}

} // namespace

AlsaDriver::AlsaDriver(Patchage* app, ILog& log)
    : _app(app)
    , _log(log)
    , _seq(nullptr)
    , _refresh_thread{}
{}

AlsaDriver::~AlsaDriver()
{
	detach();
}

/** Attach to ALSA. */
void
AlsaDriver::attach(bool /*launch_daemon*/)
{
	int ret = snd_seq_open(&_seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
	if (ret) {
		_log.error("[ALSA] Unable to attach");
		_seq = nullptr;
	} else {
		_log.info("[ALSA] Attached");

		snd_seq_set_client_name(_seq, "Patchage");

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, 50000);

		ret = pthread_create(
		    &_refresh_thread, &attr, &AlsaDriver::refresh_main, this);
		if (ret) {
			_log.error("[ALSA] Failed to start refresh thread");
		}

		signal_attached.emit();
	}
}

void
AlsaDriver::detach()
{
	if (_seq) {
		pthread_cancel(_refresh_thread);
		pthread_join(_refresh_thread, nullptr);
		snd_seq_close(_seq);
		_seq = nullptr;
		signal_detached.emit();
		_log.info("[ALSA] Detached");
	}
}

static bool
is_alsa_port(const PatchagePort* port)
{
	return port->type() == PortType::alsa_midi;
}

/** Destroy all JACK (canvas) ports.
 */
void
AlsaDriver::destroy_all()
{
	_app->canvas()->remove_ports(is_alsa_port);
	_modules.clear();
	_port_addrs.clear();
}

/** Refresh all Alsa Midi ports and connections.
 */
void
AlsaDriver::refresh()
{
	if (!is_attached()) {
		return;
	}

	assert(_seq);

	_modules.clear();
	_ignored.clear();
	_port_addrs.clear();

	snd_seq_client_info_t* cinfo = nullptr;
	snd_seq_client_info_alloca(&cinfo);
	snd_seq_client_info_set_client(cinfo, -1);

	snd_seq_port_info_t* pinfo = nullptr;
	snd_seq_port_info_alloca(&pinfo);

	// Create port views
	{
		PatchageModule* parent = nullptr;
		PatchagePort*   port   = nullptr;

		while (snd_seq_query_next_client(_seq, cinfo) >= 0) {
			snd_seq_port_info_set_client(pinfo,
			                             snd_seq_client_info_get_client(cinfo));
			snd_seq_port_info_set_port(pinfo, -1);
			while (snd_seq_query_next_port(_seq, pinfo) >= 0) {
				const snd_seq_addr_t& addr = *snd_seq_port_info_get_addr(pinfo);
				if (ignore(addr)) {
					continue;
				}

				create_port_view_internal(addr, parent, port);
			}
		}
	}

	// Create connections
	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client(_seq, cinfo) >= 0) {
		snd_seq_port_info_set_client(pinfo,
		                             snd_seq_client_info_get_client(cinfo));
		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(_seq, pinfo) >= 0) {
			const snd_seq_addr_t* addr = snd_seq_port_info_get_addr(pinfo);
			if (ignore(*addr)) {
				continue;
			}

			PatchagePort* const port1 = _app->canvas()->find_port(
			    PortID::alsa(addr->client, addr->port, false));
			if (!port1) {
				continue;
			}

			snd_seq_query_subscribe_t* subsinfo = nullptr;
			snd_seq_query_subscribe_alloca(&subsinfo);
			snd_seq_query_subscribe_set_root(subsinfo, addr);
			snd_seq_query_subscribe_set_index(subsinfo, 0);
			while (!snd_seq_query_port_subscribers(_seq, subsinfo)) {
				const snd_seq_addr_t* addr2 =
				    snd_seq_query_subscribe_get_addr(subsinfo);
				if (addr2) {
					const PortID id2 =
					    PortID::alsa(addr2->client, addr2->port, true);
					PatchagePort* port2 = _app->canvas()->find_port(id2);
					if (port2 && !_app->canvas()->get_edge(port1, port2)) {
						_app->canvas()->make_connection(port1, port2);
					}
				}

				snd_seq_query_subscribe_set_index(
				    subsinfo, snd_seq_query_subscribe_get_index(subsinfo) + 1);
			}
		}
	}
}

PatchagePort*
AlsaDriver::create_port_view(Patchage*, const PortID& id)
{
	PatchageModule* parent = nullptr;
	PatchagePort*   port   = nullptr;
	create_port_view_internal({id.alsa_client(), id.alsa_port()}, parent, port);

	return port;
}

PatchageModule*
AlsaDriver::find_module(uint8_t client_id, ModuleType type)
{
	const Modules::const_iterator i = _modules.find(client_id);
	if (i == _modules.end()) {
		return nullptr;
	}

	PatchageModule* io_module = nullptr;
	for (Modules::const_iterator j = i;
	     j != _modules.end() && j->first == client_id;
	     ++j) {
		if (j->second->type() == type) {
			return j->second;
		}

		if (j->second->type() == ModuleType::input_output) {
			io_module = j->second;
		}
	}

	// Return InputOutput module for Input or Output, or null if not found
	return io_module;
}

PatchageModule*
AlsaDriver::find_or_create_module(Patchage*          patchage,
                                  uint8_t            client_id,
                                  const std::string& client_name,
                                  ModuleType         type)
{
	PatchageModule* m = find_module(client_id, type);
	if (!m) {
		m = new PatchageModule(
		    patchage, client_name, type, ClientID::alsa(client_id));
		m->load_location();
		_app->canvas()->add_module(client_name, m);
		_modules.insert(std::make_pair(client_id, m));
	}
	return m;
}

void
AlsaDriver::create_port_view_internal(snd_seq_addr_t   addr,
                                      PatchageModule*& parent,
                                      PatchagePort*&   port)
{
	if (ignore(addr)) {
		return;
	}

	snd_seq_client_info_t* cinfo = nullptr;
	snd_seq_client_info_alloca(&cinfo);
	snd_seq_client_info_set_client(cinfo, addr.client);
	snd_seq_get_any_client_info(_seq, addr.client, cinfo);

	snd_seq_port_info_t* pinfo = nullptr;
	snd_seq_port_info_alloca(&pinfo);
	snd_seq_port_info_set_client(pinfo, addr.client);
	snd_seq_port_info_set_port(pinfo, addr.port);
	snd_seq_get_any_port_info(_seq, addr.client, addr.port, pinfo);

	const std::string client_name    = snd_seq_client_info_get_name(cinfo);
	const std::string port_name      = snd_seq_port_info_get_name(pinfo);
	bool              is_input       = false;
	bool              is_duplex      = false;
	bool              is_application = true;

	const int caps = snd_seq_port_info_get_capability(pinfo);
	const int type = snd_seq_port_info_get_type(pinfo);

	// Figure out direction
	if ((caps & SND_SEQ_PORT_CAP_READ) && (caps & SND_SEQ_PORT_CAP_WRITE)) {
		is_duplex = true;
	} else if (caps & SND_SEQ_PORT_CAP_READ) {
		is_input = false;
	} else if (caps & SND_SEQ_PORT_CAP_WRITE) {
		is_input = true;
	}

	is_application = (type & SND_SEQ_PORT_TYPE_APPLICATION);

	// Because there would be name conflicts, we must force a split if (stupid)
	// alsa duplex ports are present on the client
	bool split = false;
	if (is_duplex) {
		split = true;
		if (!_app->conf()->get_module_split(client_name, !is_application)) {
			_app->conf()->set_module_split(client_name, true);
		}
	} else {
		split = _app->conf()->get_module_split(client_name, !is_application);
	}

	if (!split) {
		parent = find_or_create_module(
		    _app, addr.client, client_name, ModuleType::input_output);
		if (!parent->get_port(port_name)) {
			port = create_port(*parent, port_name, is_input, addr);
			port->show();
		}

	} else { // split
		{
			const ModuleType module_type =
			    ((is_input) ? ModuleType::input : ModuleType::output);

			parent = find_or_create_module(
			    _app, addr.client, client_name, module_type);
			if (!parent->get_port(port_name)) {
				port = create_port(*parent, port_name, is_input, addr);
				port->show();
			}
		}

		if (is_duplex) {
			const ModuleType flipped_module_type =
			    ((!is_input) ? ModuleType::input : ModuleType::output);
			parent = find_or_create_module(
			    _app, addr.client, client_name, flipped_module_type);
			if (!parent->get_port(port_name)) {
				port = create_port(*parent, port_name, !is_input, addr);
				port->show();
			}
		}
	}
}

PatchagePort*
AlsaDriver::create_port(PatchageModule&    parent,
                        const std::string& name,
                        bool               is_input,
                        snd_seq_addr_t     addr)
{
	const PortID id = PortID::alsa(addr.client, addr.port, is_input);

	auto* ret =
	    new PatchagePort(parent,
	                     PortType::alsa_midi,
	                     id,
	                     name,
	                     "",
	                     is_input,
	                     _app->conf()->get_port_color(PortType::alsa_midi),
	                     _app->show_human_names());

	dynamic_cast<PatchageCanvas*>(parent.canvas())->index_port(id, ret);

	_app->canvas()->index_port(id, ret);
	_port_addrs.insert(std::make_pair(ret, id));
	return ret;
}

bool
AlsaDriver::ignore(const snd_seq_addr_t& addr, bool add)
{
	if (_ignored.find(addr) != _ignored.end()) {
		return true;
	}

	if (!add) {
		return false;
	}

	snd_seq_client_info_t* cinfo = nullptr;
	snd_seq_client_info_alloca(&cinfo);
	snd_seq_client_info_set_client(cinfo, addr.client);
	snd_seq_get_any_client_info(_seq, addr.client, cinfo);

	snd_seq_port_info_t* pinfo = nullptr;
	snd_seq_port_info_alloca(&pinfo);
	snd_seq_port_info_set_client(pinfo, addr.client);
	snd_seq_port_info_set_port(pinfo, addr.port);
	snd_seq_get_any_port_info(_seq, addr.client, addr.port, pinfo);

	const int type = snd_seq_port_info_get_type(pinfo);
	const int caps = snd_seq_port_info_get_capability(pinfo);

	if (caps & SND_SEQ_PORT_CAP_NO_EXPORT) {
		_ignored.insert(addr);
		return true;
	}

	if (!((caps & SND_SEQ_PORT_CAP_READ) || (caps & SND_SEQ_PORT_CAP_WRITE) ||
	      (caps & SND_SEQ_PORT_CAP_DUPLEX))) {
		_ignored.insert(addr);
		return true;
	}

	if ((snd_seq_client_info_get_type(cinfo) != SND_SEQ_USER_CLIENT) &&
	    ((type == SND_SEQ_PORT_SYSTEM_TIMER ||
	      type == SND_SEQ_PORT_SYSTEM_ANNOUNCE))) {
		_ignored.insert(addr);
		return true;
	}

	return false;
}

/** Connects two Alsa Midi ports.
 *
 * \return Whether connection succeeded.
 */
bool
AlsaDriver::connect(const PortID       tail_id,
                    const std::string& tail_client_name,
                    const std::string& tail_port_name,
                    const PortID       head_id,
                    const std::string& head_client_name,
                    const std::string& head_port_name)
{
	if (tail_id.type() != PortID::Type::alsa ||
	    head_id.type() != PortID::Type::alsa) {
		_log.error("[ALSA] Attempt to connect non-ALSA ports");
		return false;
	}

	const snd_seq_addr_t tail_addr = {tail_id.alsa_client(),
	                                  tail_id.alsa_port()};

	const snd_seq_addr_t head_addr = {head_id.alsa_client(),
	                                  head_id.alsa_port()};

	if (tail_addr.client == head_addr.client &&
	    tail_addr.port == head_addr.port) {
		_log.warning("[ALSA] Refusing to connect port to itself");
		return false;
	}

	bool result = true;

	snd_seq_port_subscribe_t* subs = nullptr;
	snd_seq_port_subscribe_malloc(&subs);
	snd_seq_port_subscribe_set_sender(subs, &tail_addr);
	snd_seq_port_subscribe_set_dest(subs, &head_addr);
	snd_seq_port_subscribe_set_exclusive(subs, 0);
	snd_seq_port_subscribe_set_time_update(subs, 0);
	snd_seq_port_subscribe_set_time_real(subs, 0);

	// Already connected (shouldn't happen)
	if (!snd_seq_get_port_subscription(_seq, subs)) {
		_log.error("[ALSA] Attempt to double subscribe ports");
		result = false;
	}

	int ret = snd_seq_subscribe_port(_seq, subs);
	if (ret < 0) {
		_log.error(
		    fmt::format("[ALSA] Subscription failed ({})", snd_strerror(ret)));
		result = false;
	}

	if (result) {
		_log.info(fmt::format("[ALSA] Connected {}:{} => {}:{}",
		                      tail_client_name,
		                      tail_port_name,
		                      head_client_name,
		                      head_port_name));
	} else {
		_log.error(fmt::format("[ALSA] Failed to connect {}:{} => {}:{}",
		                       tail_client_name,
		                       tail_port_name,
		                       head_client_name,
		                       head_port_name));
	}

	return (!result);
}

/** Disconnects two Alsa Midi ports.
 *
 * \return Whether disconnection succeeded.
 */
bool
AlsaDriver::disconnect(const PortID       tail_id,
                       const std::string& tail_client_name,
                       const std::string& tail_port_name,
                       const PortID       head_id,
                       const std::string& head_client_name,
                       const std::string& head_port_name)
{
	if (tail_id.type() != PortID::Type::alsa ||
	    head_id.type() != PortID::Type::alsa) {
		_log.error("[ALSA] Attempt to disconnect non-ALSA ports");
		return false;
	}

	const snd_seq_addr_t tail_addr = {tail_id.alsa_client(),
	                                  tail_id.alsa_port()};

	const snd_seq_addr_t head_addr = {head_id.alsa_client(),
	                                  head_id.alsa_port()};

	snd_seq_port_subscribe_t* subs = nullptr;
	snd_seq_port_subscribe_malloc(&subs);
	snd_seq_port_subscribe_set_sender(subs, &tail_addr);
	snd_seq_port_subscribe_set_dest(subs, &head_addr);
	snd_seq_port_subscribe_set_exclusive(subs, 0);
	snd_seq_port_subscribe_set_time_update(subs, 0);
	snd_seq_port_subscribe_set_time_real(subs, 0);

	// Not connected (shouldn't happen)
	if (snd_seq_get_port_subscription(_seq, subs) != 0) {
		_log.error(
		    "[ALSA] Attempt to unsubscribe ports that are not subscribed");
		return false;
	}

	int ret = snd_seq_unsubscribe_port(_seq, subs);
	if (ret < 0) {
		_log.error(
		    fmt::format("[ALSA] Failed to disconnect {}:{} => {}:{} ({})",
		                tail_client_name,
		                tail_port_name,
		                head_client_name,
		                head_port_name,
		                snd_strerror(ret)));
		return false;
	}

	_log.info(fmt::format("[ALSA] Disconnected {}:{} => {}:{}",
	                      tail_client_name,
	                      tail_port_name,
	                      head_client_name,
	                      head_port_name));

	return true;
}

bool
AlsaDriver::create_refresh_port()
{
	snd_seq_port_info_t* port_info = nullptr;
	snd_seq_port_info_alloca(&port_info);
	snd_seq_port_info_set_name(port_info, "System Announcement Receiver");
	snd_seq_port_info_set_type(port_info, SND_SEQ_PORT_TYPE_APPLICATION);
	snd_seq_port_info_set_capability(port_info,
	                                 SND_SEQ_PORT_CAP_WRITE |
	                                     SND_SEQ_PORT_CAP_SUBS_WRITE |
	                                     SND_SEQ_PORT_CAP_NO_EXPORT);

	int ret = snd_seq_create_port(_seq, port_info);
	if (ret) {
		_log.error(
		    fmt::format("[ALSA] Error creating port ({})", snd_strerror(ret)));
		return false;
	}

	// Subscribe the port to the system announcer
	ret = snd_seq_connect_from(_seq,
	                           snd_seq_port_info_get_port(port_info),
	                           SND_SEQ_CLIENT_SYSTEM,
	                           SND_SEQ_PORT_SYSTEM_ANNOUNCE);
	if (ret) {
		_log.error(
		    fmt::format("[ALSA] Failed to connect to system announce port ({})",
		                snd_strerror(ret)));
		return false;
	}

	return true;
}

void*
AlsaDriver::refresh_main(void* me)
{
	auto* ad = static_cast<AlsaDriver*>(me);
	ad->_refresh_main();
	return nullptr;
}

void
AlsaDriver::_refresh_main()
{
	if (!create_refresh_port()) {
		_log.error(
		    "[ALSA] Could not create listen port, auto-refresh disabled");
		return;
	}

	int caps = 0;

	snd_seq_client_info_t* cinfo = nullptr;
	snd_seq_client_info_alloca(&cinfo);

	snd_seq_port_info_t* pinfo = nullptr;
	snd_seq_port_info_alloca(&pinfo);

	snd_seq_event_t* ev = nullptr;
	while (snd_seq_event_input(_seq, &ev) > 0) {
		assert(ev);

		std::lock_guard<std::mutex> lock{_events_mutex};

		switch (ev->type) {
		case SND_SEQ_EVENT_PORT_SUBSCRIBED:
			if (!ignore(ev->data.connect.sender) &&
			    !ignore(ev->data.connect.dest)) {
				_events.push(
				    PatchageEvent(PatchageEvent::Type::connection,
				                  addr_to_id(ev->data.connect.sender, false),
				                  addr_to_id(ev->data.connect.dest, true)));
			}
			break;
		case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
			if (!ignore(ev->data.connect.sender) &&
			    !ignore(ev->data.connect.dest)) {
				_events.push(
				    PatchageEvent(PatchageEvent::Type::disconnection,
				                  addr_to_id(ev->data.connect.sender, false),
				                  addr_to_id(ev->data.connect.dest, true)));
			}
			break;
		case SND_SEQ_EVENT_PORT_START:
			snd_seq_get_any_client_info(_seq, ev->data.addr.client, cinfo);
			snd_seq_get_any_port_info(
			    _seq, ev->data.addr.client, ev->data.addr.port, pinfo);
			caps = snd_seq_port_info_get_capability(pinfo);

			if (!ignore(ev->data.addr)) {
				_events.push(PatchageEvent(
				    PatchageEvent::Type::port_creation,
				    addr_to_id(ev->data.addr, (caps & SND_SEQ_PORT_CAP_READ))));
			}
			break;
		case SND_SEQ_EVENT_PORT_EXIT:
			if (!ignore(ev->data.addr, false)) {
				// Note: getting caps at this point does not work
				// Delete both inputs and outputs (to handle duplex ports)
				_events.push(
				    PatchageEvent(PatchageEvent::Type::port_destruction,
				                  addr_to_id(ev->data.addr, true)));
				_events.push(
				    PatchageEvent(PatchageEvent::Type::port_destruction,
				                  addr_to_id(ev->data.addr, false)));

				_port_addrs.erase(_app->canvas()->find_port(
				    addr_to_id(ev->data.addr, false)));
				_port_addrs.erase(
				    _app->canvas()->find_port(addr_to_id(ev->data.addr, true)));
			}
			break;
		case SND_SEQ_EVENT_CLIENT_CHANGE:
		case SND_SEQ_EVENT_CLIENT_EXIT:
		case SND_SEQ_EVENT_CLIENT_START:
		case SND_SEQ_EVENT_PORT_CHANGE:
		case SND_SEQ_EVENT_RESET:
		default:
			//_events.push(PatchageEvent(PatchageEvent::REFRESH));
			break;
		}
	}
}

void
AlsaDriver::process_events(Patchage* app)
{
	std::lock_guard<std::mutex> lock{_events_mutex};

	while (!_events.empty()) {
		PatchageEvent& ev = _events.front();
		ev.execute(app);
		_events.pop();
	}
}
