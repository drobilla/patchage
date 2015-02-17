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

#ifndef PATCHAGE_PATCHAGEPORT_HPP
#define PATCHAGE_PATCHAGEPORT_HPP

#include <string>

#include <boost/shared_ptr.hpp>

#include <gtkmm/menu.h>
#include <gtkmm/menushell.h>

#include "ganv/Port.hpp"
#include "ganv/Module.hpp"

#include "Configuration.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageModule.hpp"
#include "PortID.hpp"
#include "patchage_config.h"

/** A Port on a PatchageModule
 */
class PatchagePort : public Ganv::Port
{
public:
	PatchagePort(Ganv::Module&      module,
	             PortType           type,
	             const std::string& name,
	             const std::string& human_name,
	             bool               is_input,
	             uint32_t           color,
	             bool               show_human_name)
		: Port(module,
		       (show_human_name && !human_name.empty()) ? human_name : name,
		       is_input,
		       color)
		, _type(type)
		, _name(name)
		, _human_name(human_name)
	{
		signal_event().connect(
			sigc::mem_fun(this, &PatchagePort::on_event));
	}

	virtual ~PatchagePort() {}

	/** Returns the full name of this port, as "modulename:portname" */
	std::string full_name() const {
		PatchageModule* pmod = dynamic_cast<PatchageModule*>(get_module());
		return std::string(pmod->name()) + ":" + _name;
	}

	void show_human_name(bool human) {
		if (human && !_human_name.empty()) {
			set_label(_human_name.c_str());
		} else {
			set_label(_name.c_str());
		}
	}

	bool on_event(GdkEvent* ev) {
		if (ev->type != GDK_BUTTON_PRESS || ev->button.button != 3) {
			return false;
		}

		Gtk::Menu* menu = Gtk::manage(new Gtk::Menu());
		menu->items().push_back(
			Gtk::Menu_Helpers::MenuElem(
				"Disconnect", sigc::mem_fun(this, &Port::disconnect)));

		menu->popup(ev->button.button, ev->button.time);
		return true;
	}

	PortType           type()       const { return _type; }
	const std::string& name()       const { return _name; }
	const std::string& human_name() const { return _human_name; }

private:
	PortType    _type;
	std::string _name;
	std::string _human_name;
};

#endif // PATCHAGE_PATCHAGEPORT_HPP
