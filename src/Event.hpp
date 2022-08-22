// Copyright 2007-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_EVENT_HPP
#define PATCHAGE_EVENT_HPP

#include "ClientID.hpp"
#include "ClientInfo.hpp"
#include "ClientType.hpp"
#include "PortID.hpp"
#include "PortInfo.hpp"

#include <variant>

namespace patchage {
namespace event {

struct Cleared {};

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

/// An event from drivers that represents a change to the system
using Event = std::variant<event::Cleared,
                           event::ClientCreated,
                           event::ClientDestroyed,
                           event::DriverAttached,
                           event::DriverDetached,
                           event::PortCreated,
                           event::PortDestroyed,
                           event::PortsConnected,
                           event::PortsDisconnected>;

} // namespace patchage

#endif // PATCHAGE_EVENT_HPP
