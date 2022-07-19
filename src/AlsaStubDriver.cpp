// Copyright 2007-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AudioDriver.hpp"
#include "Driver.hpp"
#include "make_alsa_driver.hpp"

#include <memory>

namespace patchage {

std::unique_ptr<Driver>
make_alsa_driver(ILog&, Driver::EventSink)
{
  return nullptr;
}

} // namespace patchage
