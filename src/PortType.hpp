// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_PORTTYPE_HPP
#define PATCHAGE_PORTTYPE_HPP

#include "warnings.hpp"

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
#include <fmt/ostream.h>
PATCHAGE_RESTORE_WARNINGS

#include <ostream>

namespace patchage {

enum class PortType {
  jack_audio,
  jack_midi,
  alsa_midi,
  jack_osc,
  jack_cv,
};

inline std::ostream&
operator<<(std::ostream& os, const PortType port_type)
{
  switch (port_type) {
  case PortType::jack_audio:
    return os << "JACK audio";
  case PortType::jack_midi:
    return os << "JACK MIDI";
  case PortType::alsa_midi:
    return os << "ALSA MIDI";
  case PortType::jack_osc:
    return os << "JACK OSC";
  case PortType::jack_cv:
    return os << "JACK CV";
  }

  PATCHAGE_UNREACHABLE();
}

} // namespace patchage

template<>
struct fmt::formatter<patchage::PortType> : fmt::ostream_formatter {};

#endif // PATCHAGE_PORTTYPE_HPP
