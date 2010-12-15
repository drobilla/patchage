/* This file is part of Patchage.
 * Copyright (C) 2010 David Robillard <http://drobilla.net>
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

#include "Patchage.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageModule.hpp"
#include "PatchagePort.hpp"

PatchageModule::PatchageModule(
	Patchage* app, const std::string& name, ModuleType type, double x, double y)
	: Module(app->canvas(), name, x, y)
	, _app(app)
	, _type(type)
{
}

PatchageModule::~PatchageModule()
{
	delete _menu;
	_menu = NULL;
}

void
PatchageModule::create_menu()
{
	_menu = new Gtk::Menu();
	Gtk::Menu::MenuList& items = _menu->items();
	if (_type == InputOutput) {
		items.push_back(
			Gtk::Menu_Helpers::MenuElem("_Split", sigc::mem_fun(this, &PatchageModule::split)));
	} else {
		items.push_back(
			Gtk::Menu_Helpers::MenuElem("_Join", sigc::mem_fun(this, &PatchageModule::join)));
	}
	items.push_back(
		Gtk::Menu_Helpers::MenuElem("_Disconnect All",
		                            sigc::mem_fun(this, &PatchageModule::menu_disconnect_all)));
}

void
PatchageModule::load_location()
{
	boost::shared_ptr<Canvas> canvas = _canvas.lock();
	if (!canvas)
		return;

	Coord loc;

	if (_app->state_manager()->get_module_location(_name, _type, loc))
		move_to(loc.x, loc.y);
	else
		move_to((canvas->width()/2) - 100 + rand() % 400,
		        (canvas->height()/2) - 100 + rand() % 400);
}

void
PatchageModule::store_location()
{
	Coord loc(property_x().get_value(), property_y().get_value());
	_app->state_manager()->set_module_location(_name, _type, loc);
}

void
PatchageModule::split()
{
	assert(_type == InputOutput);
	_app->state_manager()->set_module_split(_name, true);
	_app->refresh();
}

void
PatchageModule::join()
{
	assert(_type != InputOutput);
	_app->state_manager()->set_module_split(_name, false);
	_app->refresh();
}

void
PatchageModule::menu_disconnect_all()
{
	for (PortVector::iterator p = _ports.begin(); p != _ports.end(); ++p)
		(*p)->disconnect_all();
}
