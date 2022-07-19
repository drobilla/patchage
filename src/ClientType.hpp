// Copyright 2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_CLIENTTYPE_HPP
#define PATCHAGE_CLIENTTYPE_HPP

namespace patchage {

/// A type of client (program) with supported ports
enum class ClientType {
  jack,
  alsa,
};

} // namespace patchage

#endif // PATCHAGE_CLIENTTYPE_HPP
