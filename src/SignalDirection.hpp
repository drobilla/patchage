// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_SIGNALDIRECTION_HPP
#define PATCHAGE_SIGNALDIRECTION_HPP

namespace patchage {

enum class SignalDirection {
  input,
  output,
  duplex,
};

} // namespace patchage

#endif // PATCHAGE_SIGNALDIRECTION_HPP
