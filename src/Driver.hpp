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
