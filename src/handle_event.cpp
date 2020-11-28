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

#include "handle_event.hpp"

#include "PatchageEvent.hpp"

#include "patchage_config.h"

#include "Driver.hpp"
#include "Patchage.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageModule.hpp"
#include "PatchagePort.hpp"

#if defined(HAVE_JACK_DBUS)
#	include "JackDbusDriver.hpp"
#elif defined(PATCHAGE_LIBJACK)
#	include "JackDriver.hpp"
#endif
#ifdef HAVE_ALSA
#	include "AlsaDriver.hpp"
#endif

PATCHAGE_DISABLE_FMT_WARNINGS
#include <fmt/core.h>
#include <fmt/ostream.h>
PATCHAGE_RESTORE_WARNINGS

namespace {

class EventHandler
{
public:
	using result_type = void; ///< For boost::apply_visitor

	explicit EventHandler(Patchage& patchage)
	    : _patchage{patchage}
	{}

	void operator()(const ClientCreationEvent&)
	{
		// Don't create empty modules, they will be created when ports are added
	}

	void operator()(const ClientDestructionEvent& event)
	{
		_patchage.canvas()->remove_module(event.id);
	}

	void operator()(const PortCreationEvent& event)
	{
		Driver* driver = nullptr;
		if (event.id.type() == PortID::Type::jack) {
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
			driver = _patchage.jack_driver();
#endif
#ifdef HAVE_ALSA
		} else if (event.id.type() == PortID::Type::alsa) {
			driver = _patchage.alsa_driver();
#endif
		}

		if (driver) {
			PatchagePort* port = driver->create_port_view(&_patchage, event.id);
			if (!port) {
				_patchage.log().error(fmt::format(
				    "Unable to create view for port \"{}\"", event.id));
			}
		} else {
			_patchage.log().error(
			    fmt::format("Unknown type for port \"{}\"", event.id));
		}
	}

	void operator()(const PortDestructionEvent& event)
	{
		_patchage.canvas()->remove_port(event.id);
	}

	void operator()(const ConnectionEvent& event)
	{
		PatchagePort* port_1 = _patchage.canvas()->find_port(event.tail);
		PatchagePort* port_2 = _patchage.canvas()->find_port(event.head);

		if (!port_1) {
			_patchage.log().error(fmt::format(
			    "Unable to find port \"{}\" to connect", event.tail));
		} else if (!port_2) {
			_patchage.log().error(fmt::format(
			    "Unable to find port \"{}\" to connect", event.head));
		} else {
			_patchage.canvas()->make_connection(port_1, port_2);
		}
	}

	void operator()(const DisconnectionEvent& event)
	{
		PatchagePort* port_1 = _patchage.canvas()->find_port(event.tail);
		PatchagePort* port_2 = _patchage.canvas()->find_port(event.head);

		if (!port_1) {
			_patchage.log().error(fmt::format(
			    "Unable to find port \"{}\" to disconnect", event.tail));
		} else if (!port_2) {
			_patchage.log().error(fmt::format(
			    "Unable to find port \"{}\" to disconnect", event.head));
		} else {
			_patchage.canvas()->remove_edge_between(port_1, port_2);
		}
	}

private:
	Patchage& _patchage;
};

} // namespace

void
handle_event(Patchage& patchage, const PatchageEvent& event)
{
	EventHandler handler{patchage};
	boost::apply_visitor(handler, event);
}
