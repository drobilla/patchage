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

#ifndef PATCHAGE_REACTOR_HPP
#define PATCHAGE_REACTOR_HPP

#include "Action.hpp"
#include "SignalDirection.hpp"

#include <string>

namespace patchage {

struct ClientID;
struct PortID;

class Canvas;
class CanvasModule;
class CanvasPort;
class Configuration;
class Drivers;
class ILog;

/// Reacts to actions from the user
class Reactor
{
public:
  using result_type = void; ///< For boost::apply_visitor

  explicit Reactor(Configuration& conf,
                   Drivers&       drivers,
                   Canvas&        canvas,
                   ILog&          log);

  Reactor(const Reactor&) = delete;
  Reactor& operator=(const Reactor&) = delete;

  Reactor(Reactor&&) = delete;
  Reactor& operator=(Reactor&&) = delete;

  ~Reactor() = default;

  void operator()(const action::ConnectPorts& action);
  void operator()(const action::DecreaseFontSize& action);
  void operator()(const action::DisconnectClient& action);
  void operator()(const action::DisconnectPort& action);
  void operator()(const action::DisconnectPorts& action);
  void operator()(const action::IncreaseFontSize& action);
  void operator()(const action::MoveModule& action);
  void operator()(const action::Refresh& action);
  void operator()(const action::ResetFontSize& action);
  void operator()(const action::SplitModule& action);
  void operator()(const action::UnsplitModule& action);
  void operator()(const action::ZoomFull& action);
  void operator()(const action::ZoomIn& action);
  void operator()(const action::ZoomNormal& action);
  void operator()(const action::ZoomOut& action);

  void operator()(const Action& action);

private:
  std::string module_name(const ClientID& client);

  CanvasModule* find_module(const ClientID& client, SignalDirection type);
  CanvasPort*   find_port(const PortID& port);

  Configuration& _conf;
  Drivers&       _drivers;
  Canvas&        _canvas;
  ILog&          _log;
};

} // namespace patchage

#endif // PATCHAGE_REACTOR_HPP
