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

#ifndef PATCHAGE_PATCHAGEEVENT_HPP
#define PATCHAGE_PATCHAGEEVENT_HPP

#include "ClientID.hpp"
#include "PortID.hpp"

#include <boost/variant/variant.hpp>

#include <string>

struct NoopEvent
{};

struct ClientCreationEvent
{
	ClientID id;
};

struct ClientDestructionEvent
{
	ClientID id;
};

struct PortCreationEvent
{
	PortID id;
};

struct PortDestructionEvent
{
	PortID id;
};

struct ConnectionEvent
{
	PortID tail;
	PortID head;
};

struct DisconnectionEvent
{
	PortID tail;
	PortID head;
};

/// An event from drivers that is processed by the GUI
using PatchageEvent = boost::variant<NoopEvent,
                                     ClientCreationEvent,
                                     ClientDestructionEvent,
                                     PortCreationEvent,
                                     PortDestructionEvent,
                                     ConnectionEvent,
                                     DisconnectionEvent>;

#endif // PATCHAGE_PATCHAGEEVENT_HPP
