/* This file is part of Patchage.
 * Copyright 2007-2011 David Robillard <http://drobilla.net>
 *
 * Patchage is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * Patchage is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef PATCHAGE_PATCHAGECANVAS_HPP
#define PATCHAGE_PATCHAGECANVAS_HPP

#include <map>
#include <string>

#include "patchage-config.h"

#ifdef HAVE_ALSA
  #include <alsa/asoundlib.h>
#endif

#include "flowcanvas/Canvas.hpp"

#include "PatchageEvent.hpp"
#include "PatchageModule.hpp"
#include "PortID.hpp"
#include "StateManager.hpp"

class Patchage;
class PatchageModule;
class PatchagePort;

class PatchageCanvas : public FlowCanvas::Canvas {
public:
	PatchageCanvas(Patchage* _app, int width, int height);

	PatchageModule* find_module(const std::string& name, ModuleType type);
	PatchagePort*   find_port(const PortID& id);

	PatchagePort* find_port_by_name(const std::string& client_name,
	                                const std::string& port_name);

	void connect(FlowCanvas::Connectable* port1,
	             FlowCanvas::Connectable* port2);

	void disconnect(FlowCanvas::Connectable* port1,
	                FlowCanvas::Connectable* port2);

	void status_message(const std::string& msg);

	void index_port(const PortID& id, PatchagePort* port) {
		_port_index.insert(std::make_pair(id, port));
	}

	void remove_ports(bool (*pred)(const PatchagePort*));

	void add_module(const std::string& name, PatchageModule* module);
	bool remove_item(FlowCanvas::Item* i);

	PatchagePort* remove_port(const PortID& id);

	void destroy();

private:
	Patchage* _app;

	typedef std::map<const PortID, PatchagePort*> PortIndex;
	PortIndex _port_index;

	typedef std::multimap<const std::string, PatchageModule*> ModuleIndex;
	ModuleIndex _module_index;
};

#endif // PATCHAGE_PATCHAGECANVAS_HPP
