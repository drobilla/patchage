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

#ifndef PATCHAGE_ACTION_HPP
#define PATCHAGE_ACTION_HPP

#include "ClientID.hpp"
#include "PortID.hpp"
#include "SignalDirection.hpp"

#include <boost/variant/variant.hpp>

namespace patchage {
namespace action {

struct ConnectPorts {
  PortID tail;
  PortID head;
};

struct DisconnectClient {
  ClientID        client;
  SignalDirection direction;
};

struct DisconnectPort {
  PortID port;
};

struct DisconnectPorts {
  PortID tail;
  PortID head;
};

struct MoveModule {
  ClientID        client;
  SignalDirection direction;
  double          x;
  double          y;
};

struct SplitModule {
  ClientID client;
};

struct UnsplitModule {
  ClientID client;
};

} // namespace action

/// A high-level action from the user
using Action = boost::variant<action::ConnectPorts,
                              action::DisconnectClient,
                              action::DisconnectPort,
                              action::DisconnectPorts,
                              action::MoveModule,
                              action::SplitModule,
                              action::UnsplitModule>;

} // namespace patchage

#endif // PATCHAGE_ACTION_HPP
