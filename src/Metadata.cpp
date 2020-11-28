/* This file is part of Patchage.
 * Copyright 2014-2020 David Robillard <d@drobilla.net>
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

#include "Metadata.hpp"

boost::optional<ClientInfo>
Metadata::client(const ClientID& id)
{
	const auto i = _client_data.find(id);
	if (i == _client_data.end()) {
		return {};
	}

	return i->second;
}

boost::optional<PortInfo>
Metadata::port(const PortID& id)
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
