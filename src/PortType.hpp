// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_PORTTYPE_HPP
#define PATCHAGE_PORTTYPE_HPP

namespace patchage {

enum class PortType {
  jack_audio,
  jack_midi,
  alsa_midi,
  jack_osc,
  jack_cv,
};

} // namespace patchage

#endif // PATCHAGE_PORTTYPE_HPP
