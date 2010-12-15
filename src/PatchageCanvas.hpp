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

#include <string>

#include "patchage-config.h"

#ifdef HAVE_ALSA
  #include <alsa/asoundlib.h>
#endif

#include "flowcanvas/Canvas.hpp"

#include "PatchageEvent.hpp"
#include "StateManager.hpp"

class Patchage;
class PatchageModule;
class PatchagePort;

using std::string;
using namespace FlowCanvas;

class PatchageCanvas : public Canvas {
public:
	PatchageCanvas(Patchage* _app, int width, int height);

	boost::shared_ptr<PatchageModule> find_module(const string& name, ModuleType type);
	boost::shared_ptr<PatchagePort>   find_port(const PortID& id);

	void connect(boost::shared_ptr<Connectable> port1, boost::shared_ptr<Connectable> port2);
	void disconnect(boost::shared_ptr<Connectable> port1, boost::shared_ptr<Connectable> port2);

	void status_message(const string& msg);

private:
	Patchage* _app;
};


#endif // PATCHAGE_PATCHAGECANVAS_HPP
