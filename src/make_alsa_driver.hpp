// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_MAKE_ALSA_DRIVER_HPP
#define PATCHAGE_MAKE_ALSA_DRIVER_HPP

#include "Driver.hpp"

#include <memory>

namespace patchage {

class ILog;

std::unique_ptr<Driver>
make_alsa_driver(ILog& log, Driver::EventSink emit_event);

} // namespace patchage

#endif // PATCHAGE_MAKE_ALSA_DRIVER_HPP
