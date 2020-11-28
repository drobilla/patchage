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

#include "Connector.hpp"

#include "Driver.hpp"
#include "ILog.hpp"
#include "PortID.hpp"

#include <unordered_map>

Connector::Connector(ILog& log)
    : _log(log)
{}

void
Connector::add_driver(PortID::Type type, Driver* driver)
{
	_drivers.emplace(type, driver);
}

void
Connector::connect(const PortID& tail, const PortID& head)
{
	if (tail.type() != head.type()) {
		_log.warning("Unable to connect incompatible port types");
		return;
	}

	auto d = _drivers.find(tail.type());
	if (d == _drivers.end()) {
		_log.error("No driver for port type");
		return;
	}

	d->second->connect(tail, head);
}

void
Connector::disconnect(const PortID& tail, const PortID& head)
{
	if (tail.type() != head.type()) {
		_log.error("Unable to disconnect incompatible port types");
		return;
	}

	auto d = _drivers.find(tail.type());
	if (d == _drivers.end()) {
		_log.error("No driver for port type");
		return;
	}

	d->second->disconnect(tail, head);
}
