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

#include "patchage-config.h"

#include "raul/log.hpp"
#include "raul/SharedPtr.hpp"

#if defined(HAVE_JACK_DBUS)
  #include "JackDbusDriver.hpp"
#elif defined(USE_LIBJACK)
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

PatchageCanvas::PatchageCanvas(Patchage* app, int width, int height)
	: FlowCanvas::Canvas(width, height)
	, _app(app)
{
}


boost::shared_ptr<PatchageModule>
PatchageCanvas::find_module(const string& name, ModuleType type)
{
	const ModuleIndex::const_iterator i = _module_index.find(name);
	if (i == _module_index.end())
		return boost::shared_ptr<PatchageModule>();

	boost::shared_ptr<PatchageModule> io_module;
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


boost::shared_ptr<PatchagePort>
PatchageCanvas::find_port(const PortID& id)
{
	PortIndex::iterator i = _port_index.find(id);
	if (i != _port_index.end())
		return i->second;

	boost::shared_ptr<PatchagePort> pp;

#ifdef USE_LIBJACK
	assert(id.type == PortID::JACK_ID); // Alsa ports are always indexed

	jack_port_t* jack_port = jack_port_by_id(_app->jack_driver()->client(), id.id.jack_id);
	if (!jack_port)
		return boost::shared_ptr<PatchagePort>();

	string module_name;
	string port_name;
	_app->jack_driver()->port_names(id, module_name, port_name);

	SharedPtr<PatchageModule> module = find_module(module_name,
	                     (jack_port_flags(jack_port) & JackPortIsInput) ? Input : Output);

	if (module)
		pp = PtrCast<PatchagePort>(module->get_port(port_name));

	if (pp)
		index_port(id, pp);
#endif // USE_LIBJACK

	return pp;
}


void
PatchageCanvas::connect(boost::shared_ptr<Connectable> port1, boost::shared_ptr<Connectable> port2)
{
	boost::shared_ptr<PatchagePort> p1 = boost::dynamic_pointer_cast<PatchagePort>(port1);
	boost::shared_ptr<PatchagePort> p2 = boost::dynamic_pointer_cast<PatchagePort>(port2);
	if (!p1 || !p2)
		return;

	if ((p1->type() == JACK_AUDIO && p2->type() == JACK_AUDIO)
			|| ((p1->type() == JACK_MIDI && p2->type() == JACK_MIDI))) {
#if defined(USE_LIBJACK) || defined(HAVE_JACK_DBUS)
		_app->jack_driver()->connect(p1, p2);
#endif
#ifdef HAVE_ALSA
	} else if (p1->type() == ALSA_MIDI && p2->type() == ALSA_MIDI) {
		_app->alsa_driver()->connect(p1, p2);
#endif
	} else {
		status_message("WARNING: Cannot make connection, incompatible port types.");
	}
}


void
PatchageCanvas::disconnect(boost::shared_ptr<Connectable> port1, boost::shared_ptr<Connectable> port2)
{
	boost::shared_ptr<PatchagePort> input  = boost::dynamic_pointer_cast<PatchagePort>(port1);
	boost::shared_ptr<PatchagePort> output = boost::dynamic_pointer_cast<PatchagePort>(port2);

	if (!input || !output)
		return;

	if (input->is_output() && output->is_input()) {
		// Damn, guessed wrong
		boost::shared_ptr<PatchagePort> swap = input;
		input = output;
		output = swap;
	}

	if (!input || !output || input->is_output() || output->is_input()) {
		status_message("ERROR: Attempt to disconnect mismatched/unknown ports");
		return;
	}

	if ((input->type() == JACK_AUDIO && output->type() == JACK_AUDIO)
			|| (input->type() == JACK_MIDI && output->type() == JACK_MIDI)) {
#if defined(USE_LIBJACK) || defined(HAVE_JACK_DBUS)
		_app->jack_driver()->disconnect(output, input);
#endif
#ifdef HAVE_ALSA
	} else if (input->type() == ALSA_MIDI && output->type() == ALSA_MIDI) {
		_app->alsa_driver()->disconnect(output, input);
#endif
	} else {
		status_message("ERROR: Attempt to disconnect ports with mismatched types");
	}
}


void
PatchageCanvas::status_message(const string& msg)
{
	_app->status_msg(string("[Canvas] ").append(msg));
}

void
PatchageCanvas::destroy()
{
	_port_index.clear();
	_module_index.clear();
	FlowCanvas::Canvas::destroy();
}
