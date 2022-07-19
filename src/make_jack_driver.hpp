// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_MAKE_JACK_DRIVER_HPP
#define PATCHAGE_MAKE_JACK_DRIVER_HPP

#include "Driver.hpp"

#include <memory>

namespace patchage {

class AudioDriver;
class ILog;

std::unique_ptr<AudioDriver>
make_jack_driver(ILog& log, Driver::EventSink emit_event);

} // namespace patchage

#endif // PATCHAGE_MAKE_JACK_DRIVER_HPP
