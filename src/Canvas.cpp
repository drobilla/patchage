// Copyright 2007-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Canvas.hpp"

#include "Action.hpp"
#include "ActionSink.hpp"
#include "CanvasModule.hpp"
#include "CanvasPort.hpp"
#include "ClientID.hpp"
#include "ClientInfo.hpp"
#include "ClientType.hpp"
#include "Configuration.hpp"
#include "Coord.hpp"
#include "ILog.hpp"
#include "Metadata.hpp"
#include "PortID.hpp"
#include "PortInfo.hpp"
#include "PortNames.hpp"
#include "Setting.hpp"
#include "SignalDirection.hpp"
#include "warnings.hpp"

PATCHAGE_DISABLE_GANV_WARNINGS
#include <ganv/Canvas.hpp>
#include <ganv/Edge.hpp>
#include <ganv/Module.hpp>
#include <ganv/Node.hpp>
#include <ganv/Port.hpp>
#include <ganv/module.h>
#include <ganv/types.h>
PATCHAGE_RESTORE_WARNINGS

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
PATCHAGE_RESTORE_WARNINGS

#include <gdk/gdkkeysyms.h>
#include <sigc++/functors/mem_fun.h>
#include <sigc++/signal.h>

#include <cassert>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <variant>

namespace patchage {
namespace {

struct RemovePortsData {
  using Predicate = bool (*)(const CanvasPort*);

  explicit RemovePortsData(Predicate p)
    : pred(p)
  {}

  Predicate          pred;
  std::set<ClientID> empty_clients;
};

void
delete_port_if_matches(GanvPort* port, void* cdata)
{
  auto* data  = static_cast<RemovePortsData*>(cdata);
  auto* pport = dynamic_cast<CanvasPort*>(Glib::wrap(port));
  if (pport && data->pred(pport)) {
    delete pport;
  }
}

void
remove_ports_matching(GanvNode* node, void* cdata)
{
  if (!GANV_IS_MODULE(node)) {
    return;
  }

  Ganv::Module* cmodule = Glib::wrap(GANV_MODULE(node));
  auto*         pmodule = dynamic_cast<CanvasModule*>(cmodule);
  if (!pmodule) {
    return;
  }

  auto* data = static_cast<RemovePortsData*>(cdata);

  pmodule->for_each_port(delete_port_if_matches, data);

  if (pmodule->num_ports() == 0) {
    data->empty_clients.insert(pmodule->id());
  }
}

} // namespace

Canvas::Canvas(ILog& log, ActionSink& action_sink, int width, int height)
  : Ganv::Canvas(width, height)
  , _log(log)
  , _action_sink(action_sink)
  , _rng(width + height)
{
  signal_event.connect(sigc::mem_fun(this, &Canvas::on_event));
  signal_connect.connect(sigc::mem_fun(this, &Canvas::on_connect));
  signal_disconnect.connect(sigc::mem_fun(this, &Canvas::on_disconnect));
}

CanvasPort*
Canvas::create_port(Configuration&  conf,
                    const Metadata& metadata,
                    const PortID&   id,
                    const PortInfo& info)
{
  const auto client_id = id.client();

  const auto port_name =
    ((id.type() == PortID::Type::alsa) ? info.label : PortNames(id).port());

  // Figure out the client name, for ALSA we need the metadata cache
  std::string client_name;
  if (id.type() == PortID::Type::alsa) {
    const auto client_info = metadata.client(client_id);
    if (!client_info) {
      _log.error(fmt::format(
        u8"(Unable to add port “{}”, client “{}” is unknown)", id, client_id));

      return nullptr;
    }

    client_name = client_info->label;
  } else {
    client_name = PortNames(id).client();
  }

  // Determine the module type to place the port on in case of splitting
  SignalDirection module_type = SignalDirection::duplex;
  if (conf.get_module_split(client_name, info.is_terminal)) {
    module_type = info.direction;
  }

  // Find or create parent module
  CanvasModule* parent = find_module(client_id, module_type);
  if (!parent) {
    // Determine initial position
    Coord loc;
    if (!conf.get_module_location(client_name, module_type, loc)) {
      // No position saved, come up with a pseudo-random one
      loc.x = static_cast<double>(20 + (_rng() % 640));
      loc.y = static_cast<double>(20 + (_rng() % 480));

      conf.set_module_location(client_name, module_type, loc);
    }

    parent = new CanvasModule(
      *this, _action_sink, client_name, module_type, client_id, loc.x, loc.y);

    add_module(client_id, parent);
  }

  if (parent->get_port(id)) {
    // TODO: Update existing port?
    _log.error(fmt::format(
      u8"(Module “{}” already has port “{}”)", client_name, port_name));
    return nullptr;
  }

  auto* const port = new CanvasPort(*parent,
                                    info.type,
                                    id,
                                    port_name,
                                    info.label,
                                    info.direction == SignalDirection::input,
                                    conf.get_port_color(info.type),
                                    conf.get<setting::HumanNames>(),
                                    info.order);

  _port_index.insert(std::make_pair(id, port));

  return port;
}

CanvasModule*
Canvas::find_module(const ClientID& id, const SignalDirection type)
{
  auto i = _module_index.find(id);

  CanvasModule* io_module = nullptr;
  for (; i != _module_index.end() && i->first == id; ++i) {
    if (i->second->type() == type) {
      return i->second;
    }

    if (i->second->type() == SignalDirection::duplex) {
      io_module = i->second;
    }
  }

  // Return duplex module for input or output (or nullptr if not found)
  return io_module;
}

void
Canvas::remove_module(const ClientID& id)
{
  auto i = _module_index.find(id);
  while (i != _module_index.end() && i->first == id) {
    delete i->second;
    i = _module_index.erase(i);
  }
}

CanvasPort*
Canvas::find_port(const PortID& id)
{
  auto i = _port_index.find(id);
  if (i != _port_index.end()) {
    assert(i->second->get_module());
    return i->second;
  }

  return nullptr;
}

void
Canvas::remove_port(const PortID& id)
{
  CanvasPort* const port = find_port(id);
  _port_index.erase(id);
  delete port;
}

void
Canvas::remove_ports(bool (*pred)(const CanvasPort*))
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

  for (const ClientID& id : data.empty_clients) {
    remove_module(id);
  }
}

void
Canvas::on_connect(Ganv::Node* port1, Ganv::Node* port2)
{
  auto* const p1 = dynamic_cast<CanvasPort*>(port1);
  auto* const p2 = dynamic_cast<CanvasPort*>(port2);

  if (p1 && p2) {
    if (p1->is_output() && p2->is_input()) {
      _action_sink(action::ConnectPorts{p1->id(), p2->id()});
    } else if (p2->is_output() && p1->is_input()) {
      _action_sink(action::ConnectPorts{p2->id(), p1->id()});
    }
  }
}

void
Canvas::on_disconnect(Ganv::Node* port1, Ganv::Node* port2)
{
  auto* const p1 = dynamic_cast<CanvasPort*>(port1);
  auto* const p2 = dynamic_cast<CanvasPort*>(port2);

  if (p1 && p2) {
    if (p1->is_output() && p2->is_input()) {
      _action_sink(action::DisconnectPorts{p1->id(), p2->id()});
    } else if (p2->is_output() && p1->is_input()) {
      _action_sink(action::DisconnectPorts{p2->id(), p1->id()});
    }
  }
}

void
Canvas::add_module(const ClientID& id, CanvasModule* module)
{
  _module_index.emplace(id, module);

  // Join partners, if applicable
  CanvasModule* in_module  = nullptr;
  CanvasModule* out_module = nullptr;
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
  auto*       canvas = static_cast<Canvas*>(data);
  Ganv::Edge* edgemm = Glib::wrap(edge);

  if (canvas && edgemm) {
    canvas->on_disconnect(edgemm->get_tail(), edgemm->get_head());
  }
}

bool
Canvas::on_event(GdkEvent* ev)
{
  if (ev->type == GDK_KEY_PRESS && ev->key.keyval == GDK_KEY_Delete) {
    for_each_selected_edge(disconnect_edge, this);
    clear_selection();
    return true;
  }

  return false;
}

bool
Canvas::make_connection(Ganv::Node* tail, Ganv::Node* head)
{
  new Ganv::Edge(*this, tail, head);
  return true;
}

void
Canvas::clear()
{
  _port_index.clear();
  _module_index.clear();
  Ganv::Canvas::clear();
}

} // namespace patchage
