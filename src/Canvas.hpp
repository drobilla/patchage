// Copyright 2007-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_CANVAS_HPP
#define PATCHAGE_CANVAS_HPP

#include "ActionSink.hpp"
#include "ClientID.hpp"
#include "PortID.hpp"
#include "warnings.hpp"

PATCHAGE_DISABLE_GANV_WARNINGS
#include "ganv/Canvas.hpp"
#include "ganv/types.h"
PATCHAGE_RESTORE_WARNINGS

#include <gdk/gdk.h>

#include <map>
#include <random>

namespace Ganv {
class Node;
} // namespace Ganv

namespace patchage {

enum class SignalDirection;

struct PortInfo;

class CanvasModule;
class CanvasPort;
class ILog;
class Metadata;
class Configuration;

class Canvas : public Ganv::Canvas
{
public:
  Canvas(ILog& log, ActionSink& action_sink, int width, int height);

  CanvasPort* create_port(Configuration&  conf,
                          const Metadata& metadata,
                          const PortID&   id,
                          const PortInfo& info);

  CanvasModule* find_module(const ClientID& id, SignalDirection type);
  CanvasPort*   find_port(const PortID& id);

  void remove_module(const ClientID& id);

  void remove_ports(bool (*pred)(const CanvasPort*));

  void add_module(const ClientID& id, CanvasModule* module);

  bool make_connection(Ganv::Node* tail, Ganv::Node* head);

  void remove_port(const PortID& id);

  void clear() override;

private:
  using PortIndex   = std::map<const PortID, CanvasPort*>;
  using ModuleIndex = std::multimap<const ClientID, CanvasModule*>;

  friend void disconnect_edge(GanvEdge*, void*);

  bool on_event(GdkEvent* ev);

  void on_connect(Ganv::Node* port1, Ganv::Node* port2);
  void on_disconnect(Ganv::Node* port1, Ganv::Node* port2);

  ILog&       _log;
  ActionSink& _action_sink;
  PortIndex   _port_index;
  ModuleIndex _module_index;

  std::minstd_rand _rng;
};

} // namespace patchage

#endif // PATCHAGE_CANVAS_HPP
