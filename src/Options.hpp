// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_OPTIONS_HPP
#define PATCHAGE_OPTIONS_HPP

namespace patchage {

struct Options {
  bool alsa_driver_autoattach = true;
  bool jack_driver_autoattach = true;
};

} // namespace patchage

#endif // PATCHAGE_OPTIONS_HPP
