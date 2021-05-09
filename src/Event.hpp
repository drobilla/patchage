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

#ifndef PATCHAGE_EVENT_HPP
#define PATCHAGE_EVENT_HPP

#include "ClientID.hpp"
#include "ClientInfo.hpp"
#include "ClientType.hpp"
#include "PortID.hpp"
#include "PortInfo.hpp"

#include <boost/variant/variant.hpp>

namespace patchage {
namespace event {

struct ClientCreated {
  ClientID   id;
  ClientInfo info;
};

struct ClientDestroyed {
  ClientID id;
};

struct DriverAttached {
  ClientType type;
};

struct DriverDetached {
  ClientType type;
};

struct PortCreated {
  PortID   id;
  PortInfo info;
};

struct PortDestroyed {
  PortID id;
};

struct PortsConnected {
  PortID tail;
  PortID head;
};

struct PortsDisconnected {
  PortID tail;
  PortID head;
};

} // namespace event

/// An event from drivers that is processed by the GUI
using Event = boost::variant<event::ClientCreated,
                             event::ClientDestroyed,
                             event::DriverAttached,
                             event::DriverDetached,
                             event::PortCreated,
                             event::PortDestroyed,
                             event::PortsConnected,
                             event::PortsDisconnected>;

} // namespace patchage

#endif // PATCHAGE_EVENT_HPP
