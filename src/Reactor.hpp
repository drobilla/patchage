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
#include "ClientID.hpp"
#include "PortID.hpp"
#include "SignalDirection.hpp"

#include <unordered_map>

namespace patchage {

class CanvasModule;
class CanvasPort;
class Driver;
class ILog;
class Patchage;

/// Reacts to actions from the user
class Reactor
{
public:
  using result_type = void; ///< For boost::apply_visitor

  explicit Reactor(Patchage& patchage);

  Reactor(const Reactor&) = delete;
  Reactor& operator=(const Reactor&) = delete;

  Reactor(Reactor&&) = delete;
  Reactor& operator=(Reactor&&) = delete;

  ~Reactor() = default;

  void add_driver(PortID::Type type, Driver* driver);

  void operator()(const action::ConnectPorts& action);
  void operator()(const action::DisconnectClient& action);
  void operator()(const action::DisconnectPort& action);
  void operator()(const action::DisconnectPorts& action);
  void operator()(const action::MoveModule& action);
  void operator()(const action::SplitModule& action);
  void operator()(const action::UnsplitModule& action);

  void operator()(const Action& action);

private:
  CanvasModule* find_module(const ClientID& client, SignalDirection type);
  CanvasPort*   find_port(const PortID& port);

  Patchage&                                 _patchage;
  ILog&                                     _log;
  std::unordered_map<PortID::Type, Driver*> _drivers;
};

} // namespace patchage

#endif // PATCHAGE_REACTOR_HPP
