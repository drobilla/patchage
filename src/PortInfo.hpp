/* This file is part of Patchage.
 * Copyright 2007-2020 David Robillard <d@drobilla.net>
 *
 * Patchage is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Patchage is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Patchage.  If not, see <http://www.gnu.org/licenses/>.
 */

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
