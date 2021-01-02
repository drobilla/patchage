/* This file is part of Patchage.
 * Copyright 2008-2020 David Robillard <d@drobilla.net>
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

#ifndef PATCHAGE_PORTNAMES_HPP
#define PATCHAGE_PORTNAMES_HPP

#include "PortID.hpp"

#include <cassert>
#include <string>

namespace patchage {

/// Utility class that splits a Jack port ID into client and client names
class PortNames
{
public:
  explicit PortNames(const std::string& jack_name)
  {
    const auto colon = jack_name.find(':');

    if (colon != std::string::npos) {
      _client_name = jack_name.substr(0, colon);
      _port_name   = jack_name.substr(colon + 1);
    }
  }

  explicit PortNames(const PortID& id)
    : PortNames(id.jack_name())
  {
    assert(id.type() == PortID::Type::jack);
  }

  const std::string& client() const { return _client_name; }
  const std::string& port() const { return _port_name; }

private:
  std::string _client_name;
  std::string _port_name;
};

} // namespace patchage

#endif // PATCHAGE_PORTNAMES_HPP
