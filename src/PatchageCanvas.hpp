/* This file is part of Patchage.
 * Copyright (C) 2007-2009 David Robillard <http://drobilla.net>
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

	boost::shared_ptr<PatchageModule> find_module(const std::string& name, ModuleType type);
	boost::shared_ptr<PatchagePort>   find_port(const PortID& id);
	
	boost::shared_ptr<PatchagePort> find_port_by_name(const std::string& client_name,
	                                                  const std::string& port_name);

	void connect(boost::shared_ptr<FlowCanvas::Connectable> port1,
	             boost::shared_ptr<FlowCanvas::Connectable> port2);
	
	void disconnect(boost::shared_ptr<FlowCanvas::Connectable> port1,
	                boost::shared_ptr<FlowCanvas::Connectable> port2);

	void status_message(const std::string& msg);

	void index_port(const PortID& id, boost::shared_ptr<PatchagePort> port) {
		_port_index.insert(std::make_pair(id, port));
	}

	void add_module(const std::string& name, boost::shared_ptr<PatchageModule> module) {
		_module_index.insert(std::make_pair(name, module));
		add_item(module);
	}

	void destroy();

private:
	Patchage* _app;

	typedef std::map< const PortID, boost::shared_ptr<PatchagePort> > PortIndex;
	PortIndex _port_index;

	typedef std::multimap< const std::string, boost::shared_ptr<PatchageModule> > ModuleIndex;
	ModuleIndex _module_index;
};


#endif // PATCHAGE_PATCHAGECANVAS_HPP
