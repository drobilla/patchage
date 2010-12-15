/* This file is part of Patchage.
 * Copyright (C) 2007-2009 David Robillard <http://drobilla.net>
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

#include <string>
#include <set>
#include <cassert>
#include "raul/log.hpp"
#include "raul/SharedPtr.hpp"
#include "PatchageCanvas.hpp"
#include "AlsaDriver.hpp"
#include "Patchage.hpp"
#include "PatchageModule.hpp"
#include "PatchagePort.hpp"

using namespace std;
using namespace FlowCanvas;

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
		_app->status_msg("[ALSA] Unable to attach");
		_seq = NULL;
	} else {
		_app->status_msg("[ALSA] Attached");

		snd_seq_set_client_name(_seq, "Patchage");

		pthread_attr_t attr;
		pthread_attr_init(&attr);
		pthread_attr_setstacksize(&attr, 50000);

		ret = pthread_create(&_refresh_thread, &attr, &AlsaDriver::refresh_main, this);
		if (ret)
			Raul::error << "Couldn't start refresh thread" << endl;

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
		_app->status_msg("[ALSA] Detached");
	}
}


/** Refresh all Alsa Midi ports and connections.
 */
void
AlsaDriver::refresh()
{
	if (!is_attached())
		return;

	assert(_seq);

	refresh_ports();
	refresh_connections();
}


boost::shared_ptr<PatchagePort>
AlsaDriver::create_port_view(Patchage*     patchage,
                             const PortID& id)
{
	boost::shared_ptr<PatchageModule> parent;
	boost::shared_ptr<PatchagePort>   port;
	create_port_view_internal(patchage, id.id.alsa_addr, parent, port);
	return port;
}


boost::shared_ptr<PatchageModule>
AlsaDriver::find_or_create_module(
		Patchage*          patchage,
		const std::string& client_name,
		ModuleType         type)
{
	boost::shared_ptr<PatchageModule> m = _app->canvas()->find_module(client_name, type);
	if (!m) {
		m = boost::shared_ptr<PatchageModule>(new PatchageModule(patchage, client_name, type));
		m->load_location();
		_app->canvas()->add_item(m);
		_app->enqueue_resize(m);
	}
	return m;
}


void
AlsaDriver::create_port_view_internal(
		Patchage*                          patchage,
		snd_seq_addr_t                     addr,
		boost::shared_ptr<PatchageModule>& m,
		boost::shared_ptr<PatchagePort>&   port)
{
	snd_seq_client_info_t* cinfo;
	snd_seq_client_info_alloca(&cinfo);
	snd_seq_client_info_set_client(cinfo, addr.client);
	snd_seq_get_any_client_info(_seq, addr.client, cinfo);

	snd_seq_port_info_t* pinfo;
	snd_seq_port_info_alloca(&pinfo);
	snd_seq_port_info_set_client(pinfo, addr.client);
	snd_seq_port_info_set_port(pinfo, addr.port);
	snd_seq_get_any_port_info(_seq, addr.client, addr.port, pinfo);

	const string client_name = snd_seq_client_info_get_name(cinfo);
	const string port_name = snd_seq_port_info_get_name(pinfo);
	bool is_input       = false;
	bool is_duplex      = false;
	bool is_application = true;
	bool need_refresh   = false;

	int caps = snd_seq_port_info_get_capability(pinfo);
	int type = snd_seq_port_info_get_type(pinfo);

	// Skip ports we shouldn't show
	if (caps & SND_SEQ_PORT_CAP_NO_EXPORT)
		return;
	else if ( !( (caps & SND_SEQ_PORT_CAP_READ)
				|| (caps & SND_SEQ_PORT_CAP_WRITE)
				|| (caps & SND_SEQ_PORT_CAP_DUPLEX)))
		return;
	else if ((snd_seq_client_info_get_type(cinfo) != SND_SEQ_USER_CLIENT)
			&& ((type == SND_SEQ_PORT_SYSTEM_TIMER
					|| type == SND_SEQ_PORT_SYSTEM_ANNOUNCE)))
		return;

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
			need_refresh = true;
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
		m = find_or_create_module(_app, client_name, InputOutput);
		if (!m->get_port(port_name)) {
			port = create_port(m, port_name, is_input, addr);
			port->show();
			m->add_port(port);
		}

	} else { // split
		ModuleType type = ((is_input) ? Input : Output);
		m = find_or_create_module(_app, client_name, type);
		if (!m->get_port(port_name)) {
			port = create_port(m, port_name, is_input, addr);
			port->show();
			m->add_port(port);
		}

		if (is_duplex) {
			type = ((!is_input) ? Input : Output);
			m = find_or_create_module(_app, client_name, type);
			if (!m->get_port(port_name)) {
				port = create_port(m, port_name, !is_input, addr);
				port->show();
				m->add_port(port);
			}
		}
	}
}


boost::shared_ptr<PatchagePort>
AlsaDriver::create_port(boost::shared_ptr<PatchageModule> parent,
		const string& name, bool is_input, snd_seq_addr_t addr)
{
	boost::shared_ptr<PatchagePort> ret(
		new PatchagePort(parent, ALSA_MIDI, name, is_input,
			_app->state_manager()->get_port_color(ALSA_MIDI)));
	ret->alsa_addr(addr);
	return ret;
}


/** Refresh all Alsa Midi ports.
 */
void
AlsaDriver::refresh_ports()
{
	assert(is_attached());
	assert(_seq);

	snd_seq_client_info_t* cinfo;
	snd_seq_client_info_alloca(&cinfo);
	snd_seq_client_info_set_client(cinfo, -1);

	snd_seq_port_info_t* pinfo;
	snd_seq_port_info_alloca(&pinfo);

	boost::shared_ptr<PatchageModule> parent;
	boost::shared_ptr<PatchagePort>   port;

	set< boost::shared_ptr<PatchageModule> > to_resize;

	while (snd_seq_query_next_client (_seq, cinfo) >= 0) {
		snd_seq_port_info_set_client(pinfo, snd_seq_client_info_get_client(cinfo));
		snd_seq_port_info_set_port(pinfo, -1);
		while (snd_seq_query_next_port(_seq, pinfo) >= 0) {
			create_port_view_internal(_app, *snd_seq_port_info_get_addr(pinfo), parent, port);
			if (parent)
				_app->enqueue_resize(parent);
		}
	}
}


/** Refresh all Alsa Midi connections.
 */
void
AlsaDriver::refresh_connections()
{
	assert(is_attached());
	assert(_seq);

	boost::shared_ptr<PatchageModule> m;
	boost::shared_ptr<PatchagePort>   p;

	for (ItemList::iterator i = _app->canvas()->items().begin();
			i != _app->canvas()->items().end(); ++i) {
		m = boost::dynamic_pointer_cast<PatchageModule>(*i);
		if (m) {
			for (PortVector::const_iterator j = m->ports().begin(); j != m->ports().end(); ++j) {
				p = boost::dynamic_pointer_cast<PatchagePort>(*j);
				if (p->type() == ALSA_MIDI)
					add_connections(p);
			}
		}
	}
}


/** Add all connections for the given port.
 */
void
AlsaDriver::add_connections(boost::shared_ptr<PatchagePort> port)
{
	assert(is_attached());
	assert(_seq);

	const snd_seq_addr_t* addr = port->alsa_addr();
	boost::shared_ptr<PatchagePort> connected_port;

	// Fix a problem with duplex->duplex connections (would show up twice)
	// No sense doing them all twice anyway..
	if (port->is_input())
		return;

	snd_seq_query_subscribe_t* subsinfo;
	snd_seq_query_subscribe_alloca(&subsinfo);
	snd_seq_query_subscribe_set_root(subsinfo, addr);
	snd_seq_query_subscribe_set_index(subsinfo, 0);

	while (!snd_seq_query_port_subscribers(_seq, subsinfo)) {
		const snd_seq_addr_t* connected_addr = snd_seq_query_subscribe_get_addr(subsinfo);
		if (!connected_addr)
			continue;

		PortID id(*connected_addr, true);
		connected_port = _app->canvas()->find_port(id);

		if (connected_port && !port->is_connected_to(connected_port))
			_app->canvas()->add_connection(port, connected_port, port->color() + 0x22222200);

		snd_seq_query_subscribe_set_index(subsinfo, snd_seq_query_subscribe_get_index(subsinfo) + 1);
	}

}


/** Connects two Alsa Midi ports.
 *
 * \return Whether connection succeeded.
 */
bool
AlsaDriver::connect(boost::shared_ptr<PatchagePort> src_port, boost::shared_ptr<PatchagePort> dst_port)
{
	const snd_seq_addr_t* src = src_port->alsa_addr();
	const snd_seq_addr_t* dst = dst_port->alsa_addr();

	bool result = true;

	if (src && dst) {
		snd_seq_port_subscribe_t* subs;
		snd_seq_port_subscribe_malloc(&subs);
		snd_seq_port_subscribe_set_sender(subs, src);
		snd_seq_port_subscribe_set_dest(subs, dst);
		snd_seq_port_subscribe_set_exclusive(subs, 0);
		snd_seq_port_subscribe_set_time_update(subs, 0);
		snd_seq_port_subscribe_set_time_real(subs, 0);

		// Already connected (shouldn't happen)
		if (!snd_seq_get_port_subscription(_seq, subs)) {
			Raul::error << "[ALSA] Attempt to subscribe ports that are already subscribed." << endl;
			result = false;
		}

		int ret = snd_seq_subscribe_port(_seq, subs);
		if (ret < 0) {
			Raul::error << "[ALSA] Subscription failed: " << snd_strerror(ret) << endl;
			result = false;
		}
	}

	if (result)
		_app->status_msg(string("[ALSA] Connected ")
			+ src_port->full_name() + " -> " + dst_port->full_name());
	else
		_app->status_msg(string("[ALSA] Unable to connect ")
			+ src_port->full_name() + " -> " + dst_port->full_name());

	return (!result);
}


/** Disconnects two Alsa Midi ports.
 *
 * \return Whether disconnection succeeded.
 */
bool
AlsaDriver::disconnect(boost::shared_ptr<PatchagePort> src_port, boost::shared_ptr<PatchagePort> dst_port)
{
	const snd_seq_addr_t* src = src_port->alsa_addr();
	const snd_seq_addr_t* dst = dst_port->alsa_addr();

	bool result = true;

	snd_seq_port_subscribe_t* subs;
	snd_seq_port_subscribe_malloc(&subs);
	snd_seq_port_subscribe_set_sender(subs, src);
	snd_seq_port_subscribe_set_dest(subs, dst);
	snd_seq_port_subscribe_set_exclusive(subs, 0);
	snd_seq_port_subscribe_set_time_update(subs, 0);
	snd_seq_port_subscribe_set_time_real(subs, 0);

	// Not connected (shouldn't happen)
	if (snd_seq_get_port_subscription(_seq, subs) != 0) {
		Raul::error << "[ALSA] Attempt to unsubscribe ports that are not subscribed." << endl;
		result = false;
	}

	int ret = snd_seq_unsubscribe_port(_seq, subs);
	if (ret < 0) {
		Raul::error << "[ALSA] Unsubscription failed: " << snd_strerror(ret) << endl;
		result = false;
	}

	if (result)
		_app->status_msg(string("[ALSA] Disconnected ")
			+ src_port->full_name() + " -> " + dst_port->full_name());
	else
		_app->status_msg(string("[ALSA] Unable to disconnect ")
			+ src_port->full_name() + " -> " + dst_port->full_name());

	return (!result);
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
		Raul::error << "[ALSA] Error creating port: " << snd_strerror(ret) << endl;
		return false;
	}

	// Subscribe the port to the system announcer
	ret = snd_seq_connect_from(_seq,
		snd_seq_port_info_get_port(port_info),
		SND_SEQ_CLIENT_SYSTEM,
		SND_SEQ_PORT_SYSTEM_ANNOUNCE);
	if (ret) {
		Raul::error << "[ALSA] Could not connect to system announcer port: "
		            << snd_strerror(ret) << endl;
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
		Raul::error << "[ALSA] Could not create listen port, auto-refresh will not work." << endl;
		return;
	}

	int caps = 0;

	snd_seq_port_info_t* pinfo;
	snd_seq_port_info_alloca(&pinfo);

	snd_seq_event_t* ev;
	while (snd_seq_event_input(_seq, &ev) > 0) {
		assert(ev);
			
		Glib::Mutex::Lock lock(_events_mutex);

		switch (ev->type) {
		case SND_SEQ_EVENT_PORT_SUBSCRIBED:
			_events.push(PatchageEvent(PatchageEvent::CONNECTION,
			                           ev->data.connect.sender, ev->data.connect.dest));
			break;
		case SND_SEQ_EVENT_PORT_UNSUBSCRIBED:
			_events.push(PatchageEvent(PatchageEvent::DISCONNECTION,
			                           ev->data.connect.sender, ev->data.connect.dest));
			break;
		case SND_SEQ_EVENT_PORT_START:
			snd_seq_get_any_port_info(_seq, ev->data.addr.client, ev->data.addr.port, pinfo);
			caps = snd_seq_port_info_get_capability(pinfo);
			_events.push(PatchageEvent(PatchageEvent::PORT_CREATION,
			                           PortID(ev->data.addr, (caps & SND_SEQ_PORT_CAP_READ))));
			break;
		case SND_SEQ_EVENT_PORT_EXIT:
			// Note: getting caps at this point does not work
			// Delete both inputs and outputs (in case this is a duplex port)
			_events.push(PatchageEvent(PatchageEvent::PORT_DESTRUCTION,
			                           PortID(ev->data.addr, true)));
			_events.push(PatchageEvent(PatchageEvent::PORT_DESTRUCTION,
			                           PortID(ev->data.addr, false)));
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
