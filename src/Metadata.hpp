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

#ifndef PATCHAGE_METADATA_HPP
#define PATCHAGE_METADATA_HPP

#include "ClientID.hpp"
#include "ClientInfo.hpp"
#include "PortID.hpp"
#include "PortInfo.hpp"

#include <boost/optional.hpp>

#include <map>

namespace patchage {

/// Cache of metadata about clients and ports beyond their IDs
class Metadata
{
public:
	Metadata() = default;

	boost::optional<ClientInfo> client(const ClientID& id);
	boost::optional<PortInfo>   port(const PortID& id);

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
