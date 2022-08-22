// Copyright 2007-2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_METADATA_HPP
#define PATCHAGE_METADATA_HPP

#include "ClientID.hpp"
#include "ClientInfo.hpp"
#include "PortID.hpp"
#include "PortInfo.hpp"

#include <map>
#include <optional>

namespace patchage {

/// Cache of metadata about clients and ports beyond their IDs
class Metadata
{
public:
  Metadata() = default;

  std::optional<ClientInfo> client(const ClientID& id) const;
  std::optional<PortInfo>   port(const PortID& id) const;

  void set_client(const ClientID& id, const ClientInfo& info);
  void set_port(const PortID& id, const PortInfo& info);

  void erase_client(const ClientID& id);
  void erase_port(const PortID& id);

private:
  using ClientData = std::map<ClientID, ClientInfo>;
  using PortData   = std::map<PortID, PortInfo>;

  ClientData _client_data;
  PortData   _port_data;
};

} // namespace patchage

#endif // PATCHAGE_METADATA_HPP
