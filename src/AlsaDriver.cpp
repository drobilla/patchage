/* This file is part of Patchage.
 * Copyright 2007-2011 David Robillard <http://drobilla.net>
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
#include <set>
#include <string>
#include <utility>

#include <boost/format.hpp>

#include "AlsaDriver.hpp"
#include "Patchage.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageModule.hpp"
#include "PatchagePort.hpp"

using std::endl;
using std::string;
using boost::format;

AlsaDriver::AlsaDriver(Patchage* app)
	: _app(app)
	, _seq(NULL)
{
}

AlsaDriver::~AlsaDriver()
{
	detach();
}

/** Attach to ALSA.
 * @a launch_daemon is ignored, as ALSA has no daemon to launch/connect to.
 */
void
AlsaDriver::attach(bool /*launch_daemon*/)
{
	int ret = snd_seq_open(&_seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
	if (ret) {
		_app->error_msg("Alsa: Unable to attach.");
		_seq = NULL;
	} else {
		_app->info_msg("Alsa: Attached.");

		snd_seq_set_client_name(_seq, "Patchage");

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, 50000);

		ret = pthread_create(&_refresh_thread, &attr, &AlsaDriver::refresh_main, this);
		if (ret)
			_app->error_msg("Alsa: Failed to start refresh thread.");

		signal_attached.emit();
	}
}

void
AlsaDriver::detach()
{
	if (_seq) {
		pthread_cancel(_refresh_thread);
		pthread_join(_refresh_thread, NULL);
		snd_seq_close(_seq);
		_seq = NULL;
		signal_detached.emit();
		_app->info_msg("Alsa: Detached.");
	}
}

static bool
is_alsa_port(const PatchagePort* port)
{
	return port->type() == ALSA_MIDI;
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
	if (!is_attached())
		return;

	assert(_seq);

	_modules.clear();
	_ignored.clear();
	_port_addrs.clear();

	snd_seq_client_info_t* cinfo;
	snd_seq_client_info_alloca(&cinfo);
	snd_seq_client_info_set_client(cinfo, -1);

	snd_seq_port_info_t* pinfo;
	snd_seq_port_info_alloca(&pinfo);

	PatchageModule* parent = NULL;
	PatchagePort*   port   = NULL;

	// Create port views
	while (snd_seq_query_next_client(_seq, cinfo) >= 0) {
		snd_seq_port_info_set_client(pinfo, snd_seq_client_info_get_client(cinfo));
		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(_seq, pinfo) >= 0) {
			const snd_seq_addr_t& addr = *snd_seq_port_info_get_addr(pinfo);
			if (ignore(addr)) {
				continue;
			}

			create_port_view_internal(_app, addr, parent, port);
		}
	}

	// Create connections
	snd_seq_client_info_set_client(cinfo, -1);
	while (snd_seq_query_next_client(_seq, cinfo) >= 0) {
		snd_seq_port_info_set_client(pinfo, snd_seq_client_info_get_client(cinfo));
		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(_seq, pinfo) >= 0) {
			const snd_seq_addr_t* addr = snd_seq_port_info_get_addr(pinfo);
			if (ignore(*addr)) {
				continue;
			}

			PatchagePort* port = _app->canvas()->find_port(PortID(*addr, false));
			if (!port) {
				continue;
			}

			snd_seq_query_subscribe_t* subsinfo;
			snd_seq_query_subscribe_alloca(&subsinfo);
			snd_seq_query_subscribe_set_root(subsinfo, addr);
			snd_seq_query_subscribe_set_index(subsinfo, 0);
			while (!snd_seq_query_port_subscribers(_seq, subsinfo)) {
				const snd_seq_addr_t* addr2 = snd_seq_query_subscribe_get_addr(subsinfo);
				if (!addr2)
					continue;

				PatchagePort* port2 = _app->canvas()->find_port(PortID(*addr2, true));
				if (port2 && !_app->canvas()->get_connection(port, port2)) {
					_app->canvas()->make_connection(
						port, port2, port->get_fill_color() + 0x22222200);

					snd_seq_query_subscribe_set_index(
						subsinfo, snd_seq_query_subscribe_get_index(subsinfo) + 1);
				}
			}
		}
	}
}

PatchagePort*
AlsaDriver::create_port_view(Patchage*     patchage,
                             const PortID& id)
{
	PatchageModule* parent = NULL;
	PatchagePort*   port   = NULL;
	create_port_view_internal(patchage, id.id.alsa_addr, parent, port);
	return port;
}

PatchageModule*
AlsaDriver::find_module(uint8_t client_id, ModuleType type)
{
	const Modules::const_iterator i = _modules.find(client_id);
	if (i == _modules.end())
		return NULL;

	PatchageModule* io_module = NULL;
	for (Modules::const_iterator j = i;
	     j != _modules.end() && j->first == client_id;
	     ++j) {
		if (j->second->type() == type) {
			return j->second;
		} else if (j->second->type() == InputOutput) {
			io_module = j->second;
		}
	}

	// Return InputOutput module for Input or Output, or NULL if not found
	return io_module;
}

PatchageModule*
AlsaDriver::find_or_create_module(
	Patchage*          patchage,
	uint8_t            client_id,
	const std::string& client_name,
	ModuleType         type)
{
	PatchageModule* m = find_module(client_id, type);
	if (!m) {
		m = new PatchageModule(patchage, client_name, type);
		m->load_location();
		_app->canvas()->add_module(client_name, m);
		_modules.insert(std::make_pair(client_id, m));
	}
	return m;
}

void
AlsaDriver::create_port_view_internal(
	Patchage*        patchage,
	snd_seq_addr_t   addr,
	PatchageModule*& m,
	PatchagePort*&   port)
{
	if (ignore(addr))
		return;

	snd_seq_client_info_t* cinfo;
	snd_seq_client_info_alloca(&cinfo);
	snd_seq_client_info_set_client(cinfo, addr.client);
	snd_seq_get_any_client_info(_seq, addr.client, cinfo);

	snd_seq_port_info_t* pinfo;
	snd_seq_port_info_alloca(&pinfo);
	snd_seq_port_info_set_client(pinfo, addr.client);
	snd_seq_port_info_set_port(pinfo, addr.port);
	snd_seq_get_any_port_info(_seq, addr.client, addr.port, pinfo);

	const string client_name    = snd_seq_client_info_get_name(cinfo);
	const string port_name      = snd_seq_port_info_get_name(pinfo);
	bool         is_input       = false;
	bool         is_duplex      = false;
	bool         is_application = true;

	int caps = snd_seq_port_info_get_capability(pinfo);
	int type = snd_seq_port_info_get_type(pinfo);

	// Figure out direction
	if ((caps & SND_SEQ_PORT_CAP_READ) && (caps & SND_SEQ_PORT_CAP_WRITE))
		is_duplex = true;
	else if (caps & SND_SEQ_PORT_CAP_READ)
		is_input = false;
	else if (caps & SND_SEQ_PORT_CAP_WRITE)
		is_input = true;

	is_application = (type & SND_SEQ_PORT_TYPE_APPLICATION);

	// Because there would be name conflicts, we must force a split if (stupid)
	// alsa duplex ports are present on the client
	bool split = false;
	if (is_duplex) {
		split = true;
		if (!_app->state_manager()->get_module_split(client_name, !is_application)) {
			_app->state_manager()->set_module_split(client_name, true);
		}
	} else {
		split = _app->state_manager()->get_module_split(client_name, !is_application);
	}

	/*cout << "ALSA PORT: " << client_name << " : " << port_name
	  << " is_application = " << is_application
	  << " is_duplex = " << is_duplex
	  << " split = " << split << endl;*/

	if (!split) {
		m = find_or_create_module(_app, addr.client, client_name, InputOutput);
		if (!m->get_port(port_name)) {
			port = create_port(*m, port_name, is_input, addr);
			port->show();
		}

	} else { // split
		ModuleType type = ((is_input) ? Input : Output);
		m = find_or_create_module(_app, addr.client, client_name, type);
		if (!m->get_port(port_name)) {
			port = create_port(*m, port_name, is_input, addr);
			port->show();
		}

		if (is_duplex) {
			type = ((!is_input) ? Input : Output);
			m = find_or_create_module(_app, addr.client, client_name, type);
			if (!m->get_port(port_name)) {
				port = create_port(*m, port_name, !is_input, addr);
				port->show();
			}
		}
	}
}

PatchagePort*
AlsaDriver::create_port(PatchageModule& parent,
                        const string& name, bool is_input, snd_seq_addr_t addr)
{
	PatchagePort* ret = new PatchagePort(
		parent, ALSA_MIDI, name, is_input,
		_app->state_manager()->get_port_color(ALSA_MIDI));

	dynamic_cast<PatchageCanvas*>(parent.canvas())->index_port(
		PortID(addr, is_input), ret);

	_app->canvas()->index_port(PortID(addr, is_input), ret);
	_port_addrs.insert(std::make_pair(ret, PortID(addr, is_input)));
	return ret;
}

bool
AlsaDriver::ignore(const snd_seq_addr_t& addr, bool add)
{
	if (_ignored.find(addr) != _ignored.end())
		return true;

	if (!add)
		return false;

	snd_seq_client_info_t* cinfo;
	snd_seq_client_info_alloca(&cinfo);
	snd_seq_client_info_set_client(cinfo, addr.client);
	snd_seq_get_any_client_info(_seq, addr.client, cinfo);

	snd_seq_port_info_t* pinfo;
	snd_seq_port_info_alloca(&pinfo);
	snd_seq_port_info_set_client(pinfo, addr.client);
	snd_seq_port_info_set_port(pinfo, addr.port);
	snd_seq_get_any_port_info(_seq, addr.client, addr.port, pinfo);

	const int type = snd_seq_port_info_get_type(pinfo);
	const int caps = snd_seq_port_info_get_capability(pinfo);

	if (caps & SND_SEQ_PORT_CAP_NO_EXPORT) {
		_ignored.insert(addr);
		return true;
	} else if ( !( (caps & SND_SEQ_PORT_CAP_READ)
	               || (caps & SND_SEQ_PORT_CAP_WRITE)
	               || (caps & SND_SEQ_PORT_CAP_DUPLEX))) {
		_ignored.insert(addr);
		return true;
	} else if ((snd_seq_client_info_get_type(cinfo) != SND_SEQ_USER_CLIENT)
	           && ((type == SND_SEQ_PORT_SYSTEM_TIMER
	                || type == SND_SEQ_PORT_SYSTEM_ANNOUNCE))) {
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
AlsaDriver::connect(PatchagePort* src_port,
                    PatchagePort* dst_port)
{
	PortAddrs::const_iterator s = _port_addrs.find(src_port);
	PortAddrs::const_iterator d = _port_addrs.find(dst_port);

	if (s == _port_addrs.end() || d == _port_addrs.end()) {
		_app->error_msg("Alsa: Attempt to connect port with no address.");
		return false;
	}

	const PortID src = s->second;
	const PortID dst = d->second;

	if (src.id.alsa_addr.client == dst.id.alsa_addr.client
	    && src.id.alsa_addr.port == dst.id.alsa_addr.port) {
		_app->warning_msg("Alsa: Refusing to connect port to itself.");
		return false;
	}

	bool result = true;

	snd_seq_port_subscribe_t* subs;
	snd_seq_port_subscribe_malloc(&subs);
	snd_seq_port_subscribe_set_sender(subs, &src.id.alsa_addr);
	snd_seq_port_subscribe_set_dest(subs, &dst.id.alsa_addr);
	snd_seq_port_subscribe_set_exclusive(subs, 0);
	snd_seq_port_subscribe_set_time_update(subs, 0);
	snd_seq_port_subscribe_set_time_real(subs, 0);

	// Already connected (shouldn't happen)
	if (!snd_seq_get_port_subscription(_seq, subs)) {
		_app->error_msg("Alsa: Attempt to double subscribe ports.");
		result = false;
	}

	int ret = snd_seq_subscribe_port(_seq, subs);
	if (ret < 0) {
		_app->error_msg((format("Alsa: Subscription failed (%1%).")
		                 % snd_strerror(ret)).str());
		result = false;
	}

	if (result)
		_app->info_msg(string("Alsa: Connected ")
		               + src_port->full_name() + " => " + dst_port->full_name());
	else
		_app->error_msg(string("Alsa: Unable to connect ")
		                + src_port->full_name() + " => " + dst_port->full_name());

	return (!result);
}

/** Disconnects two Alsa Midi ports.
 *
 * \return Whether disconnection succeeded.
 */
bool
AlsaDriver::disconnect(PatchagePort* src_port,
                       PatchagePort* dst_port)
{
	PortAddrs::const_iterator s = _port_addrs.find(src_port);
	PortAddrs::const_iterator d = _port_addrs.find(dst_port);

	if (s == _port_addrs.end() || d == _port_addrs.end()) {
		_app->error_msg("Alsa: Attempt to connect port with no address");
		return false;
	}

	const PortID src = s->second;
	const PortID dst = d->second;

	snd_seq_port_subscribe_t* subs;
	snd_seq_port_subscribe_malloc(&subs);
	snd_seq_port_subscribe_set_sender(subs, &src.id.alsa_addr);
	snd_seq_port_subscribe_set_dest(subs, &dst.id.alsa_addr);
	snd_seq_port_subscribe_set_exclusive(subs, 0);
	snd_seq_port_subscribe_set_time_update(subs, 0);
	snd_seq_port_subscribe_set_time_real(subs, 0);

	// Not connected (shouldn't happen)
	if (snd_seq_get_port_subscription(_seq, subs) != 0) {
		_app->error_msg("Alsa: Attempt to unsubscribe ports that are not subscribed.");
		return false;
	}

	int ret = snd_seq_unsubscribe_port(_seq, subs);
	if (ret < 0) {
		_app->error_msg(string("Alsa: Unable to disconnect ")
		                + src_port->full_name() + " => " + dst_port->full_name()
		                + "(" + snd_strerror(ret) + ")");
		return false;
	}

	_app->info_msg(string("Alsa: Disconnected ")
	               + src_port->full_name() + " => " + dst_port->full_name());

	return true;
}

bool
AlsaDriver::create_refresh_port()
{
	snd_seq_port_info_t* port_info;
	snd_seq_port_info_alloca(&port_info);
	snd_seq_port_info_set_name(port_info, "System Announcement Reciever");
	snd_seq_port_info_set_type(port_info, SND_SEQ_PORT_TYPE_APPLICATION);
	snd_seq_port_info_set_capability(port_info,
	                                 SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE|SND_SEQ_PORT_CAP_NO_EXPORT);

	int ret = snd_seq_create_port(_seq, port_info);
	if (ret) {
		_app->error_msg((format("Alsa: Error creating port (%1%): ")
		                 % snd_strerror(ret)).str());
		return false;
	}

	// Subscribe the port to the system announcer
	ret = snd_seq_connect_from(_seq,
	                           snd_seq_port_info_get_port(port_info),
	                           SND_SEQ_CLIENT_SYSTEM,
	                           SND_SEQ_PORT_SYSTEM_ANNOUNCE);
	if (ret) {
		_app->error_msg((format("Alsa: Failed to connect to system announce port (%1%)")
		                 % snd_strerror(ret)).str());
		return false;
	}

	return true;
}

void*
AlsaDriver::refresh_main(void* me)
{
	AlsaDriver* ad = (AlsaDriver*)me;
	ad->_refresh_main();
	return NULL;
}

void
AlsaDriver::_refresh_main()
{
	if (!create_refresh_port()) {
		_app->error_msg("Alsa: Could not create listen port, auto-refresh disabled.");
		return;
	}

	int caps = 0;

	snd_seq_client_info_t* cinfo;
	snd_seq_client_info_alloca(&cinfo);

	snd_seq_port_info_t* pinfo;
	snd_seq_port_info_alloca(&pinfo);

	snd_seq_event_t* ev;
	while (snd_seq_event_input(_seq, &ev) > 0) {
		assert(ev);

		Glib::Mutex::Lock lock(_events_mutex);

		switch (ev->type) {
		case SND_SEQ_EVENT_PORT_SUBSCRIBED:
			if (!ignore(ev->data.connect.sender) && !ignore(ev->data.connect.dest))
				_events.push(PatchageEvent(PatchageEvent::CONNECTION,
				                           ev->data.connect.sender, ev->data.connect.dest));
			break;
		case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
			if (!ignore(ev->data.connect.sender) && !ignore(ev->data.connect.dest))
				_events.push(PatchageEvent(PatchageEvent::DISCONNECTION,
				                           ev->data.connect.sender, ev->data.connect.dest));
			break;
		case SND_SEQ_EVENT_PORT_START:
			snd_seq_get_any_client_info(_seq, ev->data.addr.client, cinfo);
			snd_seq_get_any_port_info(_seq, ev->data.addr.client, ev->data.addr.port, pinfo);
			caps = snd_seq_port_info_get_capability(pinfo);

			if (!ignore(ev->data.addr))
				_events.push(PatchageEvent(PatchageEvent::PORT_CREATION,
				                           PortID(ev->data.addr, (caps & SND_SEQ_PORT_CAP_READ))));
			break;
		case SND_SEQ_EVENT_PORT_EXIT:
			if (!ignore(ev->data.addr, false)) {
				// Note: getting caps at this point does not work
				// Delete both inputs and outputs (in case this is a duplex port)
				_events.push(PatchageEvent(PatchageEvent::PORT_DESTRUCTION,
				                           PortID(ev->data.addr, true)));
				_events.push(PatchageEvent(PatchageEvent::PORT_DESTRUCTION,
				                           PortID(ev->data.addr, false)));
				_port_addrs.erase(_app->canvas()->find_port(
					                  PortID(ev->data.addr, false)));
				_port_addrs.erase(_app->canvas()->find_port(
					                  PortID(ev->data.addr, true)));
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
	Glib::Mutex::Lock lock(_events_mutex);
	while (!_events.empty()) {
		PatchageEvent& ev = _events.front();
		ev.execute(app);
		_events.pop();
	}
}
