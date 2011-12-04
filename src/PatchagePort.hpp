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

#ifndef PATCHAGE_PATCHAGEPORT_HPP
#define PATCHAGE_PATCHAGEPORT_HPP

#include <string>

#include <boost/shared_ptr.hpp>

#include <gtkmm.h>

#include "flowcanvas/Port.hpp"
#include "flowcanvas/Module.hpp"

#include "patchage-config.h"
#include "PatchageCanvas.hpp"
#include "PortID.hpp"
#include "StateManager.hpp"

/** A Port on a PatchageModule
 */
class PatchagePort : public FlowCanvas::Port
{
public:
	PatchagePort(FlowCanvas::Module& module,
	             PortType            type,
	             const std::string&  name,
	             bool                is_input,
	             uint32_t            color)
		: Port(module, name, is_input, color)
		, _type(type)
	{
	}

	virtual ~PatchagePort() {}

	/** Returns the full name of this port, as "modulename:portname" */
	std::string full_name() const {
		return std::string(get_module()->get_label()) + ":" + get_label();
	}

	bool on_click(GdkEventButton* ev) {
		if (ev->button != 3) {
			return FlowCanvas::Port::on_click(ev);
		}

		Gtk::Menu* menu = Gtk::manage(new Gtk::Menu());
		menu->items().push_back(
			Gtk::Menu_Helpers::MenuElem(
				"Disconnect All", sigc::mem_fun(this, &Port::disconnect_all)));

		menu->popup(ev->button, ev->time);
		return true;
	}

	PortType type() const { return _type; }

private:
	PortType _type;
};

#endif // PATCHAGE_PATCHAGEPORT_HPP
