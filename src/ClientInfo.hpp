// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_CLIENTINFO_HPP
#define PATCHAGE_CLIENTINFO_HPP

#include <string>

namespace patchage {

/// Extra information about a client (program) not expressed in its ID
struct ClientInfo {
  std::string label; ///< Human-friendly label
};

} // namespace patchage

#endif // PATCHAGE_CLIENTINFO_HPP
