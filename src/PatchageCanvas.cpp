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

#include <boost/format.hpp>

#include "raul/SharedPtr.hpp"

#include "patchage-config.h"

#if defined(HAVE_JACK_DBUS)
  #include "JackDbusDriver.hpp"
#elif defined(PATCHAGE_LIBJACK)
  #include "JackDriver.hpp"
#endif
#ifdef HAVE_ALSA
  #include "AlsaDriver.hpp"
#endif

#include "Patchage.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageModule.hpp"
#include "PatchagePort.hpp"

using std::string;
using boost::format;

PatchageCanvas::PatchageCanvas(Patchage* app, int width, int height)
	: FlowCanvas::Canvas(width, height)
	, _app(app)
{
}

PatchageModule*
PatchageCanvas::find_module(const string& name, ModuleType type)
{
	const ModuleIndex::const_iterator i = _module_index.find(name);
	if (i == _module_index.end())
		return NULL;

	PatchageModule* io_module = NULL;
	for (ModuleIndex::const_iterator j = i; j != _module_index.end() && j->first == name; ++j) {
		if (j->second->type() == type) {
			return j->second;
		} else if (j->second->type() == InputOutput) {
			io_module = j->second;
		}
	}

	// Return InputOutput module for Input or Output (or NULL if not found at all)
	return io_module;
}

void
PatchageCanvas::remove_module(const string& name)
{
	ModuleIndex::iterator i = _module_index.find(name);
	while (i != _module_index.end()) {
		delete i->second;
		_module_index.erase(i);
		i = _module_index.find(name);
	}
}

PatchagePort*
PatchageCanvas::find_port(const PortID& id)
{
	PatchagePort* pp = NULL;

	PortIndex::iterator i = _port_index.find(id);
	if (i != _port_index.end()) {
		assert(i->second->module());
		return i->second;
	}

#ifdef PATCHAGE_LIBJACK
	// Alsa ports are always indexed (or don't exist at all)
	if (id.type == PortID::JACK_ID) {
		jack_port_t* jack_port = jack_port_by_id(_app->jack_driver()->client(), id.id.jack_id);
		if (!jack_port)
			return NULL;

		string module_name;
		string port_name;
		_app->jack_driver()->port_names(id, module_name, port_name);

		PatchageModule* module = find_module(
			module_name, (jack_port_flags(jack_port) & JackPortIsInput) ? Input : Output);

		if (module)
			pp = dynamic_cast<PatchagePort*>(module->get_port(port_name));

		if (pp)
			index_port(id, pp);
	}
#endif // PATCHAGE_LIBJACK

	return pp;
}

void
PatchageCanvas::remove_port(const PortID& id)
{
	PatchagePort* const port = find_port(id);
	if (!port) {
		_app->error_msg((format("Failed to find port with ID `%1' to remove.")
		                 % id).str());
	}

	_port_index.erase(id);
	delete port;
}

void
PatchageCanvas::remove_ports(bool (*pred)(const PatchagePort*))
{
	std::set<PatchageModule*> empty;
	for (Items::iterator i = items().begin(); i != items().end(); ++i) {
		PatchageModule* module = dynamic_cast<PatchageModule*>(*i);
		if (!module)
			continue;

		FlowCanvas::Module::Ports ports = module->ports(); // copy
		for (FlowCanvas::Module::Ports::iterator p = ports.begin();
		     p != ports.end(); ++p) {
			if (pred(dynamic_cast<PatchagePort*>(*p))) {
				delete *p;
			}
		}

		if (module->num_ports() == 0) {
			empty.insert(module);
		}
	}

	for (PortIndex::iterator i = _port_index.begin();
	     i != _port_index.end();) {
		PortIndex::iterator next = i;
		++next;
		if (pred(i->second)) {
			_port_index.erase(i);
		}
		i = next;
	}

	for (std::set<PatchageModule*>::iterator i = empty.begin();
	     i != empty.end(); ++i) {
		delete *i;
	}
}

PatchagePort*
PatchageCanvas::find_port_by_name(const std::string& client_name,
                                  const std::string& port_name)
{
	const ModuleIndex::const_iterator i = _module_index.find(client_name);
	if (i == _module_index.end())
		return NULL;

	for (ModuleIndex::const_iterator j = i; j != _module_index.end() && j->first == client_name; ++j) {
		PatchagePort* port = dynamic_cast<PatchagePort*>(j->second->get_port(port_name));
		if (port)
			return port;
	}

	return NULL;
}

void
PatchageCanvas::connect(FlowCanvas::Connectable* port1,
                        FlowCanvas::Connectable* port2)
{
	PatchagePort* p1 = dynamic_cast<PatchagePort*>(port1);
	PatchagePort* p2 = dynamic_cast<PatchagePort*>(port2);
	if (!p1 || !p2)
		return;

	if ((p1->type() == JACK_AUDIO && p2->type() == JACK_AUDIO)
			|| ((p1->type() == JACK_MIDI && p2->type() == JACK_MIDI))) {
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
		_app->jack_driver()->connect(p1, p2);
#endif
#ifdef HAVE_ALSA
	} else if (p1->type() == ALSA_MIDI && p2->type() == ALSA_MIDI) {
		_app->alsa_driver()->connect(p1, p2);
#endif
	} else {
		_app->warning_msg("Cannot make connection, incompatible port types.");
	}
}

void
PatchageCanvas::disconnect(FlowCanvas::Connectable* port1,
                           FlowCanvas::Connectable* port2)
{
	PatchagePort* input  = dynamic_cast<PatchagePort*>(port1);
	PatchagePort* output = dynamic_cast<PatchagePort*>(port2);
	if (!input || !output)
		return;

	if (input->is_output() && output->is_input()) {
		// Damn, guessed wrong
		PatchagePort* swap = input;
		input = output;
		output = swap;
	}

	if (!input || !output || input->is_output() || output->is_input()) {
		_app->error_msg("Attempt to disconnect mismatched/unknown ports.");
		return;
	}

	if ((input->type() == JACK_AUDIO && output->type() == JACK_AUDIO)
			|| (input->type() == JACK_MIDI && output->type() == JACK_MIDI)) {
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
		_app->jack_driver()->disconnect(output, input);
#endif
#ifdef HAVE_ALSA
	} else if (input->type() == ALSA_MIDI && output->type() == ALSA_MIDI) {
		_app->alsa_driver()->disconnect(output, input);
#endif
	} else {
		_app->error_msg("Attempt to disconnect ports with mismatched types.");
	}
}

void
PatchageCanvas::add_module(const std::string& name, PatchageModule* module)
{
	_module_index.insert(std::make_pair(name, module));

	// Join partners, if applicable
	PatchageModule* in_module = NULL;
	PatchageModule* out_module = NULL;
	if (module->type() == Input) {
		in_module  = module;
		out_module = find_module(name, Output);
	} else if (module->type() == Output) {
		in_module  = find_module(name, Output);
		out_module = module;
	}

	if (in_module && out_module)
		out_module->set_partner(in_module);
}

bool
PatchageCanvas::on_connection_event(FlowCanvas::Connection* c, GdkEvent* ev)
{
	if (ev->type == GDK_BUTTON_PRESS) {
		switch (ev->button.button) {
		case 1:
			if (!(ev->button.state & GDK_CONTROL_MASK)
			    && !(ev->button.state & GDK_SHIFT_MASK)) {
				clear_selection();
			}
			select_connection(c);
			return true;
		case 2:
			disconnect(c->source(), c->dest());
			return true;
		}
	}
	return false;
}

bool
PatchageCanvas::on_event(GdkEvent* ev)
{
	if (ev->type == GDK_KEY_PRESS && ev->key.keyval == GDK_Delete) {
		Connections cs = selected_connections();
		clear_selection();

		for (Connections::const_iterator i = cs.begin(); i != cs.end(); ++i) {
			disconnect((*i)->source(), (*i)->dest());
		}
	}

	return false;
}

bool
PatchageCanvas::add_connection(FlowCanvas::Connectable* tail,
                               FlowCanvas::Connectable* head,
                               uint32_t                 color)
{
	FlowCanvas::Connection* c = new FlowCanvas::Connection(
		*this, tail, head, color);
	c->show_handle(true);
	c->signal_event.connect(
		sigc::bind<0>(sigc::mem_fun(*this, &PatchageCanvas::on_connection_event),
		              c));
	return FlowCanvas::Canvas::add_connection(c);
}

bool
PatchageCanvas::remove_item(FlowCanvas::Item* i)
{
	// Remove item from canvas
	const bool ret = FlowCanvas::Canvas::remove_item(i);
	if (!ret)
		return ret;

	PatchageModule* const module = dynamic_cast<PatchageModule*>(i);
	if (!module)
		return ret;

	// Remove module from cache
	for (ModuleIndex::iterator i = _module_index.find(module->name());
	     i != _module_index.end() && i->first == module->name(); ++i) {
		if (i->second == module) {
			_module_index.erase(i);
			return true;
		}
	}

	return ret;
}

void
PatchageCanvas::destroy()
{
	_port_index.clear();
	_module_index.clear();
	FlowCanvas::Canvas::destroy();
}
