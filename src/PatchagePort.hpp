/* This file is part of Patchage.
 * Copyright 2007-2020 David Robillard <d@drobilla.net>
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

#include "patchage_config.h"

#include "Configuration.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageModule.hpp"
#include "PortID.hpp"
#include "warnings.hpp"

PATCHAGE_DISABLE_GANV_WARNINGS
#include "ganv/Module.hpp"
#include "ganv/Port.hpp"
PATCHAGE_RESTORE_WARNINGS

#include <gtkmm/menu.h>
#include <gtkmm/menushell.h>

#include <string>

/// A port on a PatchageModule
class PatchagePort : public Ganv::Port
{
public:
	PatchagePort(Ganv::Module&        module,
	             PortType             type,
	             PortID               id,
	             const std::string&   name,
	             const std::string&   human_name,
	             bool                 is_input,
	             uint32_t             color,
	             bool                 show_human_name,
	             boost::optional<int> order = boost::optional<int>())
	    : Port(module,
	           (show_human_name && !human_name.empty()) ? human_name : name,
	           is_input,
	           color)
	    , _type(type)
	    , _id(std::move(id))
	    , _name(name)
	    , _human_name(human_name)
	    , _order(order)
	{
		signal_event().connect(sigc::mem_fun(this, &PatchagePort::on_event));
	}

	PatchagePort(const PatchagePort&) = delete;
	PatchagePort& operator=(const PatchagePort&) = delete;

	PatchagePort(PatchagePort&&) = delete;
	PatchagePort& operator=(PatchagePort&&) = delete;

	~PatchagePort() override = default;

	/** Returns the name of the module/client this port is one */
	std::string module_name() const
	{
		auto* pmod = dynamic_cast<PatchageModule*>(get_module());
		return std::string(pmod->name());
	}

	/** Returns the full name of this port, as "modulename:portname" */
	std::string full_name() const
	{
		auto* pmod = dynamic_cast<PatchageModule*>(get_module());
		return std::string(pmod->name()) + ":" + _name;
	}

	void show_human_name(bool human)
	{
		if (human && !_human_name.empty()) {
			set_label(_human_name.c_str());
		} else {
			set_label(_name.c_str());
		}
	}

	bool on_event(GdkEvent* ev) override
	{
		if (ev->type != GDK_BUTTON_PRESS || ev->button.button != 3) {
			return false;
		}

		Gtk::Menu* menu = Gtk::manage(new Gtk::Menu());
		menu->items().push_back(Gtk::Menu_Helpers::MenuElem(
		    "Disconnect", sigc::mem_fun(this, &Port::disconnect)));

		menu->popup(ev->button.button, ev->button.time);
		return true;
	}

	PortType                    type() const { return _type; }
	PortID                      id() const { return _id; }
	const std::string&          name() const { return _name; }
	const std::string&          human_name() const { return _human_name; }
	const boost::optional<int>& order() const { return _order; }

private:
	PortType             _type;
	PortID               _id;
	std::string          _name;
	std::string          _human_name;
	boost::optional<int> _order;
};

#endif // PATCHAGE_PATCHAGEPORT_HPP
