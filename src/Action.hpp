// Copyright 2007-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_ACTION_HPP
#define PATCHAGE_ACTION_HPP

#include "ClientID.hpp"
#include "PortID.hpp"
#include "Setting.hpp"
#include "SignalDirection.hpp"

#include <boost/variant/variant.hpp>

namespace patchage {
namespace action {

struct ChangeSetting {
  Setting setting;
};

struct ConnectPorts {
  PortID tail;
  PortID head;
};

struct DecreaseFontSize {};

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

struct IncreaseFontSize {};

struct MoveModule {
  ClientID        client;
  SignalDirection direction;
  double          x;
  double          y;
};

struct Refresh {};

struct ResetFontSize {};

struct SplitModule {
  ClientID client;
};

struct UnsplitModule {
  ClientID client;
};

struct ZoomFull {};
struct ZoomIn {};
struct ZoomNormal {};
struct ZoomOut {};

} // namespace action

/// A high-level action from the user
using Action = boost::variant<action::ChangeSetting,
                              action::ConnectPorts,
                              action::DecreaseFontSize,
                              action::DisconnectClient,
                              action::DisconnectPort,
                              action::DisconnectPorts,
                              action::IncreaseFontSize,
                              action::MoveModule,
                              action::Refresh,
                              action::ResetFontSize,
                              action::SplitModule,
                              action::UnsplitModule,
                              action::ZoomFull,
                              action::ZoomIn,
                              action::ZoomNormal,
                              action::ZoomOut>;

} // namespace patchage

#endif // PATCHAGE_ACTION_HPP
