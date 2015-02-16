/* This file is part of Patchage.
 * Copyright 2007-2014 David Robillard <http://drobilla.net>
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

#include <map>
#include <string>

#include "patchage_config.h"

#ifdef HAVE_ALSA
  #include <alsa/asoundlib.h>
#endif

#include "ganv/Canvas.hpp"

#include "PatchageEvent.hpp"
#include "PatchageModule.hpp"
#include "PortID.hpp"

class Patchage;
class PatchageModule;
class PatchagePort;

class PatchageCanvas : public Ganv::Canvas {
public:
	PatchageCanvas(Patchage* _app, int width, int height);

	PatchageModule* find_module(const std::string& name, ModuleType type);
	PatchagePort*   find_port(const PortID& id);

	void remove_module(const std::string& name);
	void remove_module(PatchageModule* module);

	PatchagePort* find_port_by_name(const std::string& client_name,
	                                const std::string& port_name);

	void connect(Ganv::Node* port1,
	             Ganv::Node* port2);

	void disconnect(Ganv::Node* port1,
	                Ganv::Node* port2);

	void index_port(const PortID& id, PatchagePort* port) {
		_port_index.insert(std::make_pair(id, port));
	}

	void remove_ports(bool (*pred)(const PatchagePort*));

	void add_module(const std::string& name, PatchageModule* module);

	bool make_connection(Ganv::Node* tail, Ganv::Node* head);

	void remove_port(const PortID& id);

	void clear();

private:
	Patchage* _app;

	bool on_event(GdkEvent* ev);
	bool on_connection_event(Ganv::Edge* c, GdkEvent* ev);

	typedef std::map<const PortID, PatchagePort*> PortIndex;
	PortIndex _port_index;

	typedef std::multimap<const std::string, PatchageModule*> ModuleIndex;
	ModuleIndex _module_index;
};

#endif // PATCHAGE_PATCHAGECANVAS_HPP
