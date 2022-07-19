// Copyright 2020-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "AudioDriver.hpp"
#include "Driver.hpp"
#include "make_jack_driver.hpp"

#include <memory>

namespace patchage {

std::unique_ptr<AudioDriver>
make_jack_driver(ILog&, Driver::EventSink)
{
  return nullptr;
}

} // namespace patchage
