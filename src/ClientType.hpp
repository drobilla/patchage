// Copyright 2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_CLIENTTYPE_HPP
#define PATCHAGE_CLIENTTYPE_HPP

#include "warnings.hpp"

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
#include <fmt/ostream.h>
PATCHAGE_RESTORE_WARNINGS

#include <ostream>

namespace patchage {

/// A type of client (program) with supported ports
enum class ClientType {
  jack,
  alsa,
};

inline std::ostream&
operator<<(std::ostream& os, const ClientType type)
{
  switch (type) {
  case ClientType::jack:
    return os << "JACK";
  case ClientType::alsa:
    return os << "ALSA";
  }

  return os;
}

} // namespace patchage

template<>
struct fmt::formatter<patchage::ClientType> : fmt::ostream_formatter {};

#endif // PATCHAGE_CLIENTTYPE_HPP
