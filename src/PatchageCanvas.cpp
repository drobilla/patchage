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

#include "PatchageCanvas.hpp"

#include "patchage_config.h"

#include "Connector.hpp"
#include "Patchage.hpp"
#include "PatchageModule.hpp"
#include "PatchagePort.hpp"
#include "PortNames.hpp"
#include "SignalDirection.hpp"
#include "warnings.hpp"

#include <set>

PATCHAGE_DISABLE_GANV_WARNINGS
#include "ganv/Edge.hpp"
PATCHAGE_RESTORE_WARNINGS

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
#include <fmt/ostream.h>
PATCHAGE_RESTORE_WARNINGS

namespace patchage {

PatchageCanvas::PatchageCanvas(Connector& connector, int width, int height)
    : Ganv::Canvas(width, height)
    , _connector(connector)
{
	signal_event.connect(sigc::mem_fun(this, &PatchageCanvas::on_event));
	signal_connect.connect(sigc::mem_fun(this, &PatchageCanvas::on_connect));
	signal_disconnect.connect(
	    sigc::mem_fun(this, &PatchageCanvas::on_disconnect));
}

PatchageModule*
PatchageCanvas::create_module(Patchage&         patchage,
                              const ClientID&   id,
                              const ClientInfo& info)
{
	(void)patchage;
	(void)id;
	(void)info;
	return nullptr;
}

PatchagePort*
PatchageCanvas::create_port(Patchage&       patchage,
                            const PortID&   id,
                            const PortInfo& info)
{
	const auto client_id = id.client();

	const auto port_name =
	    ((id.type() == PortID::Type::alsa) ? info.label : PortNames(id).port());

	// Figure out the client name, for ALSA we need the metadata cache
	std::string client_name;
	if (id.type() == PortID::Type::alsa) {
		const auto client_info = patchage.metadata().client(client_id);
		if (!client_info.has_value()) {
			patchage.log().error(fmt::format(
			    "Unable to add port \"{}\", client \"{}\" is unknown",
			    id,
			    client_id));

			return nullptr;
		}

		client_name = client_info->label;
	} else {
		client_name = PortNames(id).client();
	}

	// Determine the module type to place the port on in case of splitting
	SignalDirection module_type = SignalDirection::duplex;
	if (patchage.conf().get_module_split(client_name, info.is_terminal)) {
		module_type = info.direction;
	}

	// Find or create parent module
	PatchageModule* parent = find_module(client_id, module_type);
	if (!parent) {
		parent =
		    new PatchageModule(&patchage, client_name, module_type, client_id);

		parent->load_location();
		add_module(client_id, parent);
	}

	if (parent->get_port(id)) {
		// TODO: Update existing port?
		patchage.log().error(fmt::format(
		    "Module \"{}\" already has port \"{}\"", client_name, port_name));
		return nullptr;
	}

	auto* const port =
	    new PatchagePort(*parent,
	                     info.type,
	                     id,
	                     port_name,
	                     info.label,
	                     info.direction == SignalDirection::input,
	                     patchage.conf().get_port_color(info.type),
	                     patchage.show_human_names(),
	                     info.order);

	index_port(id, port);

	return port;
}

PatchageModule*
PatchageCanvas::find_module(const ClientID& id, const SignalDirection type)
{
	auto i = _module_index.find(id);

	PatchageModule* io_module = nullptr;
	for (; i != _module_index.end() && i->first == id; ++i) {
		if (i->second->type() == type) {
			return i->second;
		}

		if (i->second->type() == SignalDirection::duplex) {
			io_module = i->second;
		}
	}

	// Return InputOutput module for Input or Output (or nullptr if not found)
	return io_module;
}

void
PatchageCanvas::remove_module(const ClientID& id)
{
	auto i = _module_index.find(id);
	while (i != _module_index.end()) {
		PatchageModule* mod = i->second;
		_module_index.erase(i);
		i = _module_index.find(id);
		delete mod;
	}
}

PatchagePort*
PatchageCanvas::find_port(const PortID& id)
{
	auto i = _port_index.find(id);
	if (i != _port_index.end()) {
		assert(i->second->get_module());
		return i->second;
	}

	return nullptr;
}

void
PatchageCanvas::remove_port(const PortID& id)
{
	PatchagePort* const port = find_port(id);
	_port_index.erase(id);
	delete port;
}

struct RemovePortsData
{
	using Predicate = bool (*)(const PatchagePort*);

	explicit RemovePortsData(Predicate p)
	    : pred(p)
	{}

	Predicate                 pred;
	std::set<PatchageModule*> empty;
};

static void
delete_port_if_matches(GanvPort* port, void* cdata)
{
	auto* data  = static_cast<RemovePortsData*>(cdata);
	auto* pport = dynamic_cast<PatchagePort*>(Glib::wrap(port));
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

	Ganv::Module* cmodule = Glib::wrap(GANV_MODULE(node));
	auto*         pmodule = dynamic_cast<PatchageModule*>(cmodule);
	if (!pmodule) {
		return;
	}

	auto* data = static_cast<RemovePortsData*>(cdata);

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

	for (auto i = _port_index.begin(); i != _port_index.end();) {
		auto next = i;
		++next;
		if (pred(i->second)) {
			_port_index.erase(i);
		}
		i = next;
	}

	for (PatchageModule* m : data.empty) {
		delete m;
	}
}

void
PatchageCanvas::on_connect(Ganv::Node* port1, Ganv::Node* port2)
{
	auto* const p1 = dynamic_cast<PatchagePort*>(port1);
	auto* const p2 = dynamic_cast<PatchagePort*>(port2);

	if (p1 && p2) {
		if (p1->is_output() && p2->is_input()) {
			_connector.connect(p1->id(), p2->id());
		} else if (p2->is_output() && p1->is_input()) {
			_connector.connect(p2->id(), p1->id());
		}
	}
}

void
PatchageCanvas::on_disconnect(Ganv::Node* port1, Ganv::Node* port2)
{
	auto* const p1 = dynamic_cast<PatchagePort*>(port1);
	auto* const p2 = dynamic_cast<PatchagePort*>(port2);

	if (p1 && p2) {
		if (p1->is_output() && p2->is_input()) {
			_connector.disconnect(p1->id(), p2->id());
		} else if (p2->is_output() && p1->is_input()) {
			_connector.disconnect(p2->id(), p1->id());
		}
	}
}

void
PatchageCanvas::add_module(const ClientID& id, PatchageModule* module)
{
	_module_index.emplace(id, module);

	// Join partners, if applicable
	PatchageModule* in_module  = nullptr;
	PatchageModule* out_module = nullptr;
	if (module->type() == SignalDirection::input) {
		in_module  = module;
		out_module = find_module(id, SignalDirection::output);
	} else if (module->type() == SignalDirection::output) {
		in_module  = find_module(id, SignalDirection::input);
		out_module = module;
	}

	if (in_module && out_module) {
		out_module->set_partner(in_module);
	}
}

void
disconnect_edge(GanvEdge* edge, void* data)
{
	auto*       canvas = static_cast<PatchageCanvas*>(data);
	Ganv::Edge* edgemm = Glib::wrap(edge);
	canvas->on_disconnect(edgemm->get_tail(), edgemm->get_head());
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
PatchageCanvas::make_connection(Ganv::Node* tail, Ganv::Node* head)
{
	new Ganv::Edge(*this, tail, head);
	return true;
}

void
PatchageCanvas::remove_module(PatchageModule* module)
{
	// Remove module from cache
	for (auto i = _module_index.find(module->id());
	     i != _module_index.end() && i->first == module->id();
	     ++i) {
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

} // namespace patchage
