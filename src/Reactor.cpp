/* This file is part of Patchage.
 * Copyright 2007-2021 David Robillard <d@drobilla.net>
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

#include "Reactor.hpp"

#include "Canvas.hpp"
#include "CanvasModule.hpp"
#include "CanvasPort.hpp"
#include "Configuration.hpp"
#include "Driver.hpp"
#include "ILog.hpp"
#include "Patchage.hpp"
#include "PortID.hpp"
#include "warnings.hpp"

#include "ganv/Port.hpp"

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
PATCHAGE_RESTORE_WARNINGS

#include <boost/variant/apply_visitor.hpp>

#include <memory>
#include <unordered_map>
#include <utility>

namespace patchage {

Reactor::Reactor(Patchage& patchage)
  : _patchage{patchage}
  , _log{patchage.log()}
{}

void
Reactor::add_driver(PortID::Type type, Driver* driver)
{
  _drivers.emplace(type, driver);
}

void
Reactor::operator()(const action::ConnectPorts& action)
{
  if (action.tail.type() != action.head.type()) {
    _log.warning("Unable to connect incompatible port types");
    return;
  }

  auto d = _drivers.find(action.tail.type());
  if (d == _drivers.end()) {
    _log.error(fmt::format("No driver for port type {}", action.tail.type()));
    return;
  }

  d->second->connect(action.tail, action.head);
}

void
Reactor::operator()(const action::DisconnectClient& action)
{
  if (CanvasModule* mod = find_module(action.client, action.direction)) {
    for (Ganv::Port* p : *mod) {
      p->disconnect();
    }
  }
}

void
Reactor::operator()(const action::DisconnectPort& action)
{
  if (CanvasPort* port = find_port(action.port)) {
    port->disconnect();
  }
}

void
Reactor::operator()(const action::DisconnectPorts& action)
{
  if (action.tail.type() != action.head.type()) {
    _log.error("Unable to disconnect incompatible port types");
    return;
  }

  auto d = _drivers.find(action.tail.type());
  if (d == _drivers.end()) {
    _log.error("No driver for port type");
    return;
  }

  d->second->disconnect(action.tail, action.head);
}

void
Reactor::operator()(const action::MoveModule& action)
{
  if (CanvasModule* mod = find_module(action.client, action.direction)) {
    _patchage.conf().set_module_location(
      mod->name(), action.direction, {action.x, action.y});
  }
}

void
Reactor::operator()(const action::SplitModule& action)
{
  if (CanvasModule* mod = find_module(action.client, SignalDirection::duplex)) {
    _patchage.conf().set_module_split(mod->name(), true);
    _patchage.refresh();
  }
}

void
Reactor::operator()(const action::UnsplitModule& action)
{
  CanvasModule* mod = find_module(action.client, SignalDirection::input);
  if (mod || (mod = find_module(action.client, SignalDirection::output))) {
    _patchage.conf().set_module_split(mod->name(), false);
    _patchage.refresh();
  }
}

void
Reactor::operator()(const Action& action)
{
  boost::apply_visitor(*this, action);
}

CanvasModule*
Reactor::find_module(const ClientID& client, const SignalDirection type)
{
  return _patchage.canvas()->find_module(client, type);
}

CanvasPort*
Reactor::find_port(const PortID& port)
{
  return _patchage.canvas()->find_port(port);
}

} // namespace patchage
