// Copyright 2008-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

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
