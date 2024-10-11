// Copyright 2007-2021 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Drivers.hpp"

#include "AudioDriver.hpp"
#include "ClientType.hpp"
#include "Driver.hpp"
#include "Event.hpp"
#include "make_alsa_driver.hpp"
#include "make_jack_driver.hpp"

#include <functional>
#include <utility>
#include <variant>

namespace patchage {

Drivers::Drivers(ILog& log, Driver::EventSink emit_event)
  : _log{log}
  , _emit_event{std::move(emit_event)}
  , _alsa_driver{make_alsa_driver(
      log,
      [this](const Event& event) { _emit_event(event); })}
  , _jack_driver{make_jack_driver(_log, [this](const Event& event) {
    _emit_event(event);
  })}
{}

Drivers::~Drivers()
{
  if (_alsa_driver) {
    _alsa_driver->detach();
  }

  if (_jack_driver) {
    _jack_driver->detach();
  }
}

void
Drivers::refresh()
{
  _emit_event(event::Cleared{});

  if (_alsa_driver) {
    _alsa_driver->refresh(_emit_event);
  }

  if (_jack_driver) {
    _jack_driver->refresh(_emit_event);
  }
}

Driver*
Drivers::driver(const ClientType type)
{
  switch (type) {
  case ClientType::jack:
    return _jack_driver.get();
  case ClientType::alsa:
    return _alsa_driver.get();
  }

  return nullptr;
}

} // namespace patchage
