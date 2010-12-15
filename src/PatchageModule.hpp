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

#ifndef PATCHAGE_PATCHAGEMODULE_HPP
#define PATCHAGE_PATCHAGEMODULE_HPP

#include <string>

#include "flowcanvas/Module.hpp"

#include "StateManager.hpp"

using namespace FlowCanvas;

class PatchageModule : public Module
{
public:
	PatchageModule(Patchage* app, const std::string& name, ModuleType type, double x=0, double y=0);
	~PatchageModule();

	void split();
	void join();

	void create_menu();
	void load_location();
	void menu_disconnect_all();
	void show_dialog() {}
	void store_location();

	ModuleType type() const { return _type; }

protected:
	Patchage*  _app;
	ModuleType _type;
};


#endif // PATCHAGE_PATCHAGEMODULE_HPP
