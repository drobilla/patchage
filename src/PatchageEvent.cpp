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

#include "PatchageEvent.hpp"

#include "patchage_config.h"

#include "Driver.hpp"
#include "Patchage.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageModule.hpp"

#if defined(HAVE_JACK_DBUS)
#	include "JackDbusDriver.hpp"
#elif defined(PATCHAGE_LIBJACK)
#	include "JackDriver.hpp"
#endif
#ifdef HAVE_ALSA
#	include "AlsaDriver.hpp"
#endif

#include <boost/format.hpp>

using boost::format;

void
PatchageEvent::execute(Patchage* patchage)
{
	if (_type == REFRESH) {
		patchage->refresh();

	} else if (_type == CLIENT_CREATION) {
		// No empty modules (for now)
		g_free(_str);
		_str = nullptr;

	} else if (_type == CLIENT_DESTRUCTION) {
		patchage->canvas()->remove_module(_str);
		g_free(_str);
		_str = nullptr;

	} else if (_type == PORT_CREATION) {

		Driver* driver = nullptr;
		if (_port_1.type == PortID::JACK_ID) {
#if defined(PATCHAGE_LIBJACK) || defined(HAVE_JACK_DBUS)
			driver = patchage->jack_driver();
#endif
#ifdef HAVE_ALSA
		} else if (_port_1.type == PortID::ALSA_ADDR) {
			driver = patchage->alsa_driver();
#endif
		}

		if (driver) {
			PatchagePort* port = driver->create_port_view(patchage, _port_1);
			if (!port) {
				patchage->error_msg(
				    (format("Unable to create view for port `%1%'") % _port_1)
				        .str());
			}
		} else {
			patchage->error_msg(
			    (format("Unknown type for port `%1%'") % _port_1).str());
		}

	} else if (_type == PORT_DESTRUCTION) {

		patchage->canvas()->remove_port(_port_1);

	} else if (_type == CONNECTION) {

		PatchagePort* port_1 = patchage->canvas()->find_port(_port_1);
		PatchagePort* port_2 = patchage->canvas()->find_port(_port_2);

		if (!port_1) {
			patchage->error_msg(
			    (format("Unable to find port `%1%' to connect") % _port_1)
			        .str());
		} else if (!port_2) {
			patchage->error_msg(
			    (format("Unable to find port `%1%' to connect") % _port_2)
			        .str());
		} else {
			patchage->canvas()->make_connection(port_1, port_2);
		}

	} else if (_type == DISCONNECTION) {

		PatchagePort* port_1 = patchage->canvas()->find_port(_port_1);
		PatchagePort* port_2 = patchage->canvas()->find_port(_port_2);

		if (!port_1) {
			patchage->error_msg(
			    (format("Unable to find port `%1%' to disconnect") % _port_1)
			        .str());
		} else if (!port_2) {
			patchage->error_msg(
			    (format("Unable to find port `%1%' to disconnect") % _port_2)
			        .str());
		} else {
			patchage->canvas()->remove_edge_between(port_1, port_2);
		}
	}
}
