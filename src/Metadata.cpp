// Copyright 2014-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Metadata.hpp"

#include "ClientID.hpp"
#include "ClientInfo.hpp"
#include "PortID.hpp"
#include "PortInfo.hpp"

#include <boost/optional/optional.hpp>

#include <utility>

namespace patchage {

boost::optional<ClientInfo>
Metadata::client(const ClientID& id) const
{
  const auto i = _client_data.find(id);
  if (i == _client_data.end()) {
    return {};
  }

  return i->second;
}

boost::optional<PortInfo>
Metadata::port(const PortID& id) const
{
  const auto i = _port_data.find(id);
  if (i == _port_data.end()) {
    return {};
  }

  return i->second;
}

void
Metadata::set_client(const ClientID& id, const ClientInfo& info)
{
  const auto i = _client_data.find(id);
  if (i == _client_data.end()) {
    _client_data.emplace(id, info);
  } else {
    i->second = info;
  }
}

void
Metadata::set_port(const PortID& id, const PortInfo& info)
{
  const auto i = _port_data.find(id);
  if (i == _port_data.end()) {
    _port_data.emplace(id, info);
  } else {
    i->second = info;
  }
}

void
Metadata::erase_client(const ClientID& id)
{
  _client_data.erase(id);
}

void
Metadata::erase_port(const PortID& id)
{
  _port_data.erase(id);
}

} // namespace patchage
