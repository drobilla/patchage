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

#ifndef PATCHAGE_PATCHAGECANVAS_HPP
#define PATCHAGE_PATCHAGECANVAS_HPP

#include "patchage_config.h"

#include "CanvasModule.hpp"
#include "Event.hpp"
#include "PortID.hpp"
#include "warnings.hpp"

PATCHAGE_DISABLE_GANV_WARNINGS
#include "ganv/Canvas.hpp"
PATCHAGE_RESTORE_WARNINGS

#include <map>
#include <string>
#include <utility>

namespace patchage {

class Patchage;
class CanvasModule;
class CanvasPort;
class Connector;

class Canvas : public Ganv::Canvas
{
public:
	Canvas(Connector& connector, int width, int height);

	CanvasModule* create_module(Patchage&         patchage,
	                            const ClientID&   id,
	                            const ClientInfo& info);

	CanvasPort*
	create_port(Patchage& patchage, const PortID& id, const PortInfo& info);

	CanvasModule* find_module(const ClientID& id, SignalDirection type);
	CanvasPort*   find_port(const PortID& id);

	void remove_module(const ClientID& id);
	void remove_module(CanvasModule* module);

	void index_port(const PortID& id, CanvasPort* port)
	{
		_port_index.insert(std::make_pair(id, port));
	}

	void remove_ports(bool (*pred)(const CanvasPort*));

	void add_module(const ClientID& id, CanvasModule* module);

	bool make_connection(Ganv::Node* tail, Ganv::Node* head);

	void remove_port(const PortID& id);

	void clear() override;

private:
	using PortIndex   = std::map<const PortID, CanvasPort*>;
	using ModuleIndex = std::multimap<const ClientID, CanvasModule*>;

	friend void disconnect_edge(GanvEdge*, void*);

	bool on_event(GdkEvent* ev);
	bool on_connection_event(Ganv::Edge* c, GdkEvent* ev);

	void on_connect(Ganv::Node* port1, Ganv::Node* port2);
	void on_disconnect(Ganv::Node* port1, Ganv::Node* port2);

	Connector&  _connector;
	PortIndex   _port_index;
	ModuleIndex _module_index;
};

} // namespace patchage

#endif // PATCHAGE_PATCHAGECANVAS_HPP
