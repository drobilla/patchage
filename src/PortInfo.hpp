// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_PORTINFO_HPP
#define PATCHAGE_PORTINFO_HPP

#include "PortType.hpp"
#include "SignalDirection.hpp"

#include <boost/optional/optional.hpp>
#include <string>

namespace patchage {

/// Extra information about a port not expressed in its ID
struct PortInfo {
  std::string          label;       ///< Human-friendly label
  PortType             type;        ///< Detailed port type
  SignalDirection      direction;   ///< Signal direction
  boost::optional<int> order;       ///< Order key on client
  bool                 is_terminal; ///< True if this is a system port
};

} // namespace patchage

#endif // PATCHAGE_PORTINFO_HPP
