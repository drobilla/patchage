/* This file is part of Patchage.
 * Copyright 2007-2011 David Robillard <http://drobilla.net>
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

#ifndef PATCHAGE_PATCHAGEMODULE_HPP
#define PATCHAGE_PATCHAGEMODULE_HPP

#include <string>

#include "flowcanvas/Module.hpp"
#include "flowcanvas/Port.hpp"

#include "StateManager.hpp"

class Patchage;

class PatchageModule : public FlowCanvas::Module
{
public:
	PatchageModule(Patchage* app, const std::string& name, ModuleType type, double x=0, double y=0);
	~PatchageModule();

	void split();
	void join();

	bool show_menu(GdkEventButton* ev);
	void update_menu();

	void load_location();
	void menu_disconnect_all();
	void show_dialog() {}
	void store_location();

	ModuleType type() const { return _type; }

protected:
	void add_port(FlowCanvas::Port* port);
	void remove_port(FlowCanvas::Port* port);

	Patchage*  _app;
	Gtk::Menu* _menu;
	ModuleType _type;
};

#endif // PATCHAGE_PATCHAGEMODULE_HPP
