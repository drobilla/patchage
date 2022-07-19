// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "event_to_string.hpp"

#include "ClientID.hpp"
#include "ClientInfo.hpp"
#include "ClientType.hpp"
#include "Event.hpp"
#include "PortID.hpp"
#include "PortInfo.hpp"
#include "PortType.hpp"
#include "SignalDirection.hpp"
#include "warnings.hpp"

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
#include <fmt/ostream.h> // IWYU pragma: keep
PATCHAGE_RESTORE_WARNINGS

#include <boost/optional/optional.hpp>
#include <boost/variant/apply_visitor.hpp>

#include <ostream> // IWYU pragma: keep
#include <string>

namespace patchage {

namespace {

struct EventPrinter {
  using result_type = std::string; ///< For boost::apply_visitor

  std::string operator()(const ClientType type)
  {
    switch (type) {
    case ClientType::jack:
      return "JACK";
    case ClientType::alsa:
      return "ALSA";
    }

    PATCHAGE_UNREACHABLE();
  }

  std::string operator()(const event::Cleared&) { return "Cleared"; }

  std::string operator()(const event::DriverAttached& event)
  {
    return fmt::format("Attached to {}", (*this)(event.type));
  }

  std::string operator()(const event::DriverDetached& event)
  {
    return fmt::format("Detached from {}", (*this)(event.type));
  }

  std::string operator()(const event::ClientCreated& event)
  {
    return fmt::format(R"(Add client "{}" ("{}"))", event.id, event.info.label);
  }

  std::string operator()(const event::ClientDestroyed& event)
  {
    return fmt::format(R"(Remove client "{}")", event.id);
  }

  std::string operator()(const PortType port_type)
  {
    switch (port_type) {
    case PortType::jack_audio:
      return "JACK audio";
    case PortType::jack_midi:
      return "JACK MIDI";
    case PortType::alsa_midi:
      return "ALSA MIDI";
    case PortType::jack_osc:
      return "JACK OSC";
    case PortType::jack_cv:
      return "JACK CV";
    }

    PATCHAGE_UNREACHABLE();
  }

  std::string operator()(const SignalDirection direction)
  {
    switch (direction) {
    case SignalDirection::input:
      return "input";
    case SignalDirection::output:
      return "output";
    case SignalDirection::duplex:
      return "duplex";
    }

    PATCHAGE_UNREACHABLE();
  }

  std::string operator()(const event::PortCreated& event)
  {
    auto result = fmt::format(R"(Add{} {} {} port "{}" ("{}"))",
                              event.info.is_terminal ? " terminal" : "",
                              (*this)(event.info.type),
                              (*this)(event.info.direction),
                              event.id,
                              event.info.label);

    if (event.info.order) {
      result += fmt::format(" order: {}", *event.info.order);
    }

    return result;
  }

  std::string operator()(const event::PortDestroyed& event)
  {
    return fmt::format(R"("Remove port "{}")", event.id);
  }

  std::string operator()(const event::PortsConnected& event)
  {
    return fmt::format(R"(Connect "{}" to "{}")", event.tail, event.head);
  }

  std::string operator()(const event::PortsDisconnected& event)
  {
    return fmt::format(R"(Disconnect "{}" from "{}")", event.tail, event.head);
  }
};

} // namespace

std::string
event_to_string(const Event& event)
{
  EventPrinter printer;
  return boost::apply_visitor(printer, event);
}

std::ostream&
operator<<(std::ostream& os, const Event& event)
{
  return os << event_to_string(event);
}

} // namespace patchage
