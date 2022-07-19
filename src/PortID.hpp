// Copyright 2008-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_PORTID_HPP
#define PATCHAGE_PORTID_HPP

#include "ClientID.hpp"
#include "ClientType.hpp"
#include "warnings.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <utility>

namespace patchage {

/// An ID for some port on a client (program)
struct PortID {
  using Type = ClientType;

  PortID(const PortID& copy) = default;
  PortID& operator=(const PortID& copy) = default;

  PortID(PortID&& id) = default;
  PortID& operator=(PortID&& id) = default;

  ~PortID() = default;

  /// Return an ID for a JACK port by full name (like "client:port")
  static PortID jack(std::string name)
  {
    return PortID{Type::jack, std::move(name)};
  }

  /// Return an ID for a JACK port by separate client and port name
  static PortID jack(const std::string& client_name,
                     const std::string& port_name)
  {
    return PortID{Type::jack, client_name + ":" + port_name};
  }

  /// Return an ID for an ALSA Sequencer port by ID
  static PortID alsa(const uint8_t client_id,
                     const uint8_t port,
                     const bool    is_input)
  {
    return PortID{Type::alsa, client_id, port, is_input};
  }

  /// Return the ID of the client that hosts this port
  ClientID client() const
  {
    switch (_type) {
    case Type::jack:
      return ClientID::jack(_jack_name.substr(0, _jack_name.find(':')));
    case Type::alsa:
      return ClientID::alsa(_alsa_client);
    }

    PATCHAGE_UNREACHABLE();
  }

  Type               type() const { return _type; }
  const std::string& jack_name() const { return _jack_name; }
  uint8_t            alsa_client() const { return _alsa_client; }
  uint8_t            alsa_port() const { return _alsa_port; }
  bool               alsa_is_input() const { return _alsa_is_input; }

private:
  PortID(const Type type, std::string jack_name)
    : _type{type}
    , _jack_name{std::move(jack_name)}
  {
    assert(_type == Type::jack);
    assert(_jack_name.find(':') != std::string::npos);
    assert(_jack_name.find(':') > 0);
    assert(_jack_name.find(':') < _jack_name.length() - 1);
  }

  PortID(const Type    type,
         const uint8_t alsa_client,
         const uint8_t alsa_port,
         const bool    is_input)
    : _type{type}
    , _alsa_client{alsa_client}
    , _alsa_port{alsa_port}
    , _alsa_is_input{is_input}
  {
    assert(_type == Type::alsa);
  }

  Type        _type;            ///< Determines which field is active
  std::string _jack_name;       ///< Full port name for Type::jack
  uint8_t     _alsa_client{};   ///< Client ID for Type::alsa
  uint8_t     _alsa_port{};     ///< Port ID for Type::alsa
  bool        _alsa_is_input{}; ///< Input flag for Type::alsa
};

inline std::ostream&
operator<<(std::ostream& os, const PortID& id)
{
  switch (id.type()) {
  case PortID::Type::jack:
    return os << "jack:" << id.jack_name();
  case PortID::Type::alsa:
    return os << "alsa:" << int(id.alsa_client()) << ":" << int(id.alsa_port())
              << ":" << (id.alsa_is_input() ? "in" : "out");
  }

  assert(false);
  return os;
}

inline bool
operator==(const PortID& lhs, const PortID& rhs)
{
  if (lhs.type() != rhs.type()) {
    return false;
  }

  switch (lhs.type()) {
  case PortID::Type::jack:
    return lhs.jack_name() == rhs.jack_name();
  case PortID::Type::alsa:
    return std::make_tuple(
             lhs.alsa_client(), lhs.alsa_port(), lhs.alsa_is_input()) ==
           std::make_tuple(
             rhs.alsa_client(), rhs.alsa_port(), rhs.alsa_is_input());
  }

  assert(false);
  return false;
}

inline bool
operator<(const PortID& lhs, const PortID& rhs)
{
  if (lhs.type() != rhs.type()) {
    return lhs.type() < rhs.type();
  }

  switch (lhs.type()) {
  case PortID::Type::jack:
    return lhs.jack_name() < rhs.jack_name();
  case PortID::Type::alsa:
    return std::make_tuple(
             lhs.alsa_client(), lhs.alsa_port(), lhs.alsa_is_input()) <
           std::make_tuple(
             rhs.alsa_client(), rhs.alsa_port(), rhs.alsa_is_input());
  }

  assert(false);
  return false;
}

} // namespace patchage

namespace std {

template<>
struct hash<patchage::PortID::Type> {
  size_t operator()(const patchage::PortID::Type& v) const noexcept
  {
    return hash<unsigned>()(static_cast<unsigned>(v));
  }
};

} // namespace std

#endif // PATCHAGE_PORTID_HPP
