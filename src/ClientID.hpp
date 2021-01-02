/* This file is part of Patchage.
 * Copyright 2020 David Robillard <d@drobilla.net>
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

#ifndef PATCHAGE_CLIENTID_HPP
#define PATCHAGE_CLIENTID_HPP

#include "ClientType.hpp"

#include <cassert>
#include <cstdint>
#include <ostream>
#include <string>
#include <utility>

namespace patchage {

/// An ID for some client (program) that has ports
struct ClientID {
  using Type = ClientType;

  ClientID(const ClientID& copy) = default;
  ClientID& operator=(const ClientID& copy) = default;

  ClientID(ClientID&& id) = default;
  ClientID& operator=(ClientID&& id) = default;

  ~ClientID() = default;

  /// Return an ID for a JACK client by name
  static ClientID jack(std::string name)
  {
    return ClientID{Type::jack, std::move(name)};
  }

  /// Return an ID for an ALSA Sequencer client by ID
  static ClientID alsa(const uint8_t id) { return ClientID{Type::alsa, id}; }

  Type               type() const { return _type; }
  const std::string& jack_name() const { return _jack_name; }
  uint8_t            alsa_id() const { return _alsa_id; }

private:
  ClientID(const Type type, std::string jack_name)
    : _type{type}
    , _jack_name{std::move(jack_name)}
  {
    assert(_type == Type::jack);
  }

  ClientID(const Type type, const uint8_t alsa_id)
    : _type{type}
    , _alsa_id{alsa_id}
  {
    assert(_type == Type::alsa);
  }

  Type        _type;        ///< Determines which field is active
  std::string _jack_name{}; ///< Client name for Type::jack
  uint8_t     _alsa_id{};   ///< Client ID for Type::alsa
};

static inline std::ostream&
operator<<(std::ostream& os, const ClientID& id)
{
  switch (id.type()) {
  case ClientID::Type::jack:
    return os << "jack:" << id.jack_name();
  case ClientID::Type::alsa:
    return os << "alsa:" << int(id.alsa_id());
  }

  assert(false);
  return os;
}

static inline bool
operator==(const ClientID& lhs, const ClientID& rhs)
{
  if (lhs.type() != rhs.type()) {
    return false;
  }

  switch (lhs.type()) {
  case ClientID::Type::jack:
    return lhs.jack_name() == rhs.jack_name();
  case ClientID::Type::alsa:
    return lhs.alsa_id() == rhs.alsa_id();
  }

  assert(false);
  return false;
}

static inline bool
operator<(const ClientID& lhs, const ClientID& rhs)
{
  if (lhs.type() != rhs.type()) {
    return lhs.type() < rhs.type();
  }

  switch (lhs.type()) {
  case ClientID::Type::jack:
    return lhs.jack_name() < rhs.jack_name();
  case ClientID::Type::alsa:
    return lhs.alsa_id() < rhs.alsa_id();
  }

  assert(false);
  return false;
}

} // namespace patchage

#endif // PATCHAGE_CLIENTID_HPP
