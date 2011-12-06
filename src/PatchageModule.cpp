/* This file is part of Patchage.
 * Copyright 2010-2011 David Robillard <http://drobilla.net>
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

#include "Patchage.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageModule.hpp"
#include "PatchagePort.hpp"

PatchageModule::PatchageModule(
	Patchage* app, const std::string& name, ModuleType type, double x, double y)
	: Module(*app->canvas().get(), name, x, y)
	, _app(app)
	, _menu(NULL)
	, _name(name)
	, _type(type)
{
	signal_moved.connect(
		sigc::mem_fun(this, &PatchageModule::store_location));
}

PatchageModule::~PatchageModule()
{
	_app->canvas()->remove_module(this);
	delete _menu;
	_menu = NULL;
}

void
PatchageModule::update_menu()
{
	if (!_menu)
		return;

	if (_type == InputOutput) {
		bool has_in  = false;
		bool has_out = false;
		for (const_iterator p = begin(); p != end(); ++p) {
			if ((*p)->is_input()) {
				has_in = true;
			} else {
				has_out = true;
			}
			if (has_in && has_out) {
				_menu->items()[0].show(); // Show "Split" menu item
				return;
			}
		}
		_menu->items()[0].hide(); // Hide "Split" menu item
	}
}

bool
PatchageModule::show_menu(GdkEventButton* ev)
{
	_menu = new Gtk::Menu();
	Gtk::Menu::MenuList& items = _menu->items();
	if (_type == InputOutput) {
		items.push_back(
			Gtk::Menu_Helpers::MenuElem(
				"_Split", sigc::mem_fun(this, &PatchageModule::split)));
		update_menu();
	} else {
		items.push_back(
			Gtk::Menu_Helpers::MenuElem(
				"_Join", sigc::mem_fun(this, &PatchageModule::join)));
	}
	items.push_back(
		Gtk::Menu_Helpers::MenuElem(
			"_Disconnect All",
			sigc::mem_fun(this, &PatchageModule::menu_disconnect_all)));

	_menu->popup(ev->button, ev->time);
	return true;
}

bool
PatchageModule::on_click(GdkEventButton* ev)
{
	if (ev->button == 3) {
		return show_menu(ev);
	}
	return false;
}

void
PatchageModule::load_location()
{
	Coord loc;

	if (_app->state_manager()->get_module_location(_name, _type, loc))
		move_to(loc.x, loc.y);
	else
		move_to(20 + rand() % 640,
		        20 + rand() % 480);
}

void
PatchageModule::store_location()
{
	Coord loc(get_x(), get_y());
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
PatchageModule::add_port(Ganv::Port* port)
{
	std::cerr << "FIXME: add port" << std::endl;
	//Ganv::Module::add_port(port);
	update_menu();
}

void
PatchageModule::remove_port(Ganv::Port* port)
{
	std::cerr << "FIXME: remove port" << std::endl;
	//Ganv::Module::remove_port(port);
	update_menu();
}

void
PatchageModule::menu_disconnect_all()
{
	for (iterator p = begin(); p != end(); ++p)
		(*p)->disconnect();
}

PatchagePort*
PatchageModule::get_port(const std::string& name)
{
	for (iterator p = begin(); p != end(); ++p) {
		if ((*p)->get_label() == name) {
			return dynamic_cast<PatchagePort*>(*p);
		}
	}
	
	return NULL;
}
