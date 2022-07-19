// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_DRIVER_HPP
#define PATCHAGE_DRIVER_HPP

#include "Event.hpp"

#include <functional>
#include <utility>

namespace patchage {

struct PortID;

/// Base class for drivers that handle system clients and ports
class Driver
{
public:
  using EventSink = std::function<void(const Event&)>;

  explicit Driver(EventSink emit_event)
    : _emit_event{std::move(emit_event)}
  {}

  Driver(const Driver&) = delete;
  Driver& operator=(const Driver&) = delete;

  Driver(Driver&&) = delete;
  Driver& operator=(Driver&&) = delete;

  virtual ~Driver() = default;

  /// Connect to the underlying system API
  virtual void attach(bool launch_daemon) = 0;

  /// Disconnect from the underlying system API
  virtual void detach() = 0;

  /// Return true iff the driver is active and connected to the system
  virtual bool is_attached() const = 0;

  /// Send events to `sink` that describe the complete current system state
  virtual void refresh(const EventSink& sink) = 0;

  /// Make a connection between ports
  virtual bool connect(const PortID& tail_id, const PortID& head_id) = 0;

  /// Remove a connection between ports
  virtual bool disconnect(const PortID& tail_id, const PortID& head_id) = 0;

protected:
  EventSink _emit_event; ///< Sink for emitting "live" events
};

} // namespace patchage

#endif // PATCHAGE_DRIVER_HPP
