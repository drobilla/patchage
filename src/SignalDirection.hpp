// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_SIGNALDIRECTION_HPP
#define PATCHAGE_SIGNALDIRECTION_HPP

#include "warnings.hpp"

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
#include <fmt/ostream.h>
PATCHAGE_RESTORE_WARNINGS

#include <ostream>

namespace patchage {

enum class SignalDirection {
  input,
  output,
  duplex,
};

inline std::ostream&
operator<<(std::ostream& os, const SignalDirection direction)
{
  switch (direction) {
  case SignalDirection::input:
    return os << "input";
  case SignalDirection::output:
    return os << "output";
  case SignalDirection::duplex:
    return os << "duplex";
  }

  PATCHAGE_UNREACHABLE();
}

} // namespace patchage

template<>
struct fmt::formatter<patchage::SignalDirection> : fmt::ostream_formatter {};

#endif // PATCHAGE_SIGNALDIRECTION_HPP
