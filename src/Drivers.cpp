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

#include "Drivers.hpp"

#include "AudioDriver.hpp"
#include "Driver.hpp"
#include "Event.hpp"
#include "make_alsa_driver.hpp"
#include "make_jack_driver.hpp"

#include <functional>
#include <utility>

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
