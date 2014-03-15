/* This file is part of Patchage.
 * Copyright 2007-2013 David Robillard <http://drobilla.net>
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

#include "patchage_config.h"

#if defined(HAVE_JACK_DBUS)
  #include "JackDbusDriver.hpp"
#elif defined(PATCHAGE_LIBJACK)
  #include "JackDriver.hpp"
#endif
#ifdef HAVE_ALSA
  #include "AlsaDriver.hpp"
#endif

#include "ganv/Edge.hpp"

#include "Patchage.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageModule.hpp"
#include "PatchagePort.hpp"

using std::string;
using boost::format;

PatchageCanvas::PatchageCanvas(Patchage* app, int width, int height)
	: Ganv::Canvas(width, height)
	, _app(app)
{
	signal_event.connect(
		sigc::mem_fun(this, &PatchageCanvas::on_event));
	signal_connect.connect(
		sigc::mem_fun(this, &PatchageCanvas::connect));
	signal_disconnect.connect(
		sigc::mem_fun(this, &PatchageCanvas::disconnect));
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
		PatchageModule* mod = i->second;
		_module_index.erase(i);
		i = _module_index.find(name);
		delete mod;
	}
}

PatchagePort*
PatchageCanvas::find_port(const PortID& id)
{
	PatchagePort* pp = NULL;

	PortIndex::iterator i = _port_index.find(id);
	if (i != _port_index.end()) {
		assert(i->second->get_module());
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
	_port_index.erase(id);
	delete port;
}

struct RemovePortsData {
	typedef bool (*Predicate)(const PatchagePort*);

	RemovePortsData(Predicate p) : pred(p) {}

	Predicate                 pred;
	std::set<PatchageModule*> empty;
};

static void
delete_port_if_matches(GanvPort* port, void* cdata)
{
	RemovePortsData* data = (RemovePortsData*)cdata;
	PatchagePort* pport = dynamic_cast<PatchagePort*>(Glib::wrap(port));
	if (pport && data->pred(pport)) {
		delete pport;
	}
}

static void
remove_ports_matching(GanvNode* node, void* cdata)
{
	if (!GANV_IS_MODULE(node)) {
		return;
	}

	Ganv::Module*   cmodule = Glib::wrap(GANV_MODULE(node));
	PatchageModule* pmodule = dynamic_cast<PatchageModule*>(cmodule);
	if (!pmodule) {
		return;
	}

	RemovePortsData* data = (RemovePortsData*)cdata;

	pmodule->for_each_port(delete_port_if_matches, data);

	if (pmodule->num_ports() == 0) {
		data->empty.insert(pmodule);
	}
}

void
PatchageCanvas::remove_ports(bool (*pred)(const PatchagePort*))
{
	RemovePortsData data(pred);

	for_each_node(remove_ports_matching, &data);

	for (PortIndex::iterator i = _port_index.begin();
	     i != _port_index.end();) {
		PortIndex::iterator next = i;
		++next;
		if (pred(i->second)) {
			_port_index.erase(i);
		}
		i = next;
	}

	for (std::set<PatchageModule*>::iterator i = data.empty.begin();
	     i != data.empty.end(); ++i) {
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
PatchageCanvas::connect(Ganv::Node* port1,
                        Ganv::Node* port2)
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
PatchageCanvas::disconnect(Ganv::Node* port1,
                           Ganv::Node* port2)
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

static void
disconnect_edge(GanvEdge* edge, void* data)
{
	PatchageCanvas* canvas = (PatchageCanvas*)data;
	Ganv::Edge*     edgemm = Glib::wrap(edge);
	canvas->disconnect(edgemm->get_tail(), edgemm->get_head());
}

bool
PatchageCanvas::on_event(GdkEvent* ev)
{
	if (ev->type == GDK_KEY_PRESS && ev->key.keyval == GDK_Delete) {
		for_each_selected_edge(disconnect_edge, this);
		clear_selection();
		return true;
	}

	return false;
}

bool
PatchageCanvas::make_connection(Ganv::Node* tail,
                                Ganv::Node* head,
                                uint32_t    color)
{
	new Ganv::Edge(*this, tail, head, color);
	return true;
}

void
PatchageCanvas::remove_module(PatchageModule* module)
{
	// Remove module from cache
	for (ModuleIndex::iterator i = _module_index.find(module->get_label());
	     i != _module_index.end() && i->first == module->get_label(); ++i) {
		if (i->second == module) {
			_module_index.erase(i);
			return;
		}
	}
}

void
PatchageCanvas::clear()
{
	_port_index.clear();
	_module_index.clear();
	Ganv::Canvas::clear();
}
