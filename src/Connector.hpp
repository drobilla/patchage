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

#ifndef PATCHAGE_CONNECTOR_HPP
#define PATCHAGE_CONNECTOR_HPP

#include "PortID.hpp"

#include <unordered_map>

namespace patchage {

class Driver;
class ILog;

/// Controller that makes and breaks connections on the system
class Connector
{
public:
	explicit Connector(ILog& log);

	void add_driver(PortID::Type type, Driver* driver);

	void connect(const PortID& tail, const PortID& head);
	void disconnect(const PortID& tail, const PortID& head);

private:
	ILog&                                     _log;
	std::unordered_map<PortID::Type, Driver*> _drivers;
};

} // namespace patchage

#endif // PATCHAGE_CONNECTOR_HPP
