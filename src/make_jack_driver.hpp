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

#ifndef PATCHAGE_MAKE_JACK_DRIVER_HPP
#define PATCHAGE_MAKE_JACK_DRIVER_HPP

#include "AudioDriver.hpp"
#include "patchage_config.h"

#include <memory>

class ILog;

#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)

std::unique_ptr<AudioDriver>
make_jack_driver(ILog& log, Driver::EventSink emit_event);

#else

inline std::unique_ptr<AudioDriver>
make_jack_driver(ILog&, Driver::EventSink)
{
	return nullptr;
}

#endif

#endif // PATCHAGE_MAKE_JACK_DRIVER_HPP
