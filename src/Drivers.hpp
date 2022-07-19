// Copyright 2007-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_DRIVERS_HPP
#define PATCHAGE_DRIVERS_HPP

#include "AudioDriver.hpp"
#include "ClientType.hpp"
#include "Driver.hpp"

#include <memory>

namespace patchage {

class ILog;

/// Manager for all drivers
class Drivers
{
public:
  Drivers(ILog& log, Driver::EventSink emit_event);

  Drivers(const Drivers&) = delete;
  Drivers& operator=(const Drivers&) = delete;

  Drivers(Drivers&&) = delete;
  Drivers& operator=(Drivers&&) = delete;

  ~Drivers() = default;

  /// Refresh all drivers and emit results to the event sink
  void refresh();

  /// Return a pointer to the driver for the given client type (or null)
  Driver* driver(ClientType type);

  /// Return a pointer to the ALSA driver (or null)
  const std::unique_ptr<Driver>& alsa() { return _alsa_driver; }

  /// Return a pointer to the JACK driver (or null)
  const std::unique_ptr<AudioDriver>& jack() { return _jack_driver; }

protected:
  ILog&                        _log;
  Driver::EventSink            _emit_event;
  std::unique_ptr<Driver>      _alsa_driver;
  std::unique_ptr<AudioDriver> _jack_driver;
};

} // namespace patchage

#endif // PATCHAGE_DRIVER_HPP
