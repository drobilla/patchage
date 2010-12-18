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

#include "raul/log.hpp"
#include "raul/SharedPtr.hpp"

#include "patchage-config.h"
#include "Patchage.hpp"
#include "PatchageCanvas.hpp"
#include "PatchageModule.hpp"
#include "PatchageEvent.hpp"
#include "Driver.hpp"
#if defined(HAVE_JACK_DBUS)
  #include "JackDbusDriver.hpp"
#elif defined(USE_LIBJACK)
  #include "JackDriver.hpp"
#endif
#ifdef HAVE_ALSA
  #include "AlsaDriver.hpp"
#endif

using namespace std;

void
PatchageEvent::execute(Patchage* patchage)
{
	if (_type == REFRESH) {
		patchage->refresh();

	} else if (_type == CLIENT_CREATION) {
		// No empty modules (for now)
		free(_str);
		_str = NULL;

	} else if (_type == CLIENT_DESTRUCTION) {
		SharedPtr<PatchageModule> module = PtrCast<PatchageModule>(
				patchage->canvas()->find_module(_str, InputOutput));

		if (module) {
			patchage->canvas()->remove_item(module);
			module.reset();
		} else {
			Raul::error << "Unable to find client `" << _str << "' to remove" << endl;
		}

		free(_str);
		_str = NULL;

	} else if (_type == PORT_CREATION) {

		Driver* driver = NULL;
		if (_port_1.type == PortID::JACK_ID) {
#if defined(USE_LIBJACK) || defined(HAVE_JACK_DBUS)
			driver = patchage->jack_driver();
#endif
#ifdef HAVE_ALSA
		} else if (_port_1.type == PortID::ALSA_ADDR) {
			driver = patchage->alsa_driver();
#endif
		}

		if (driver) {
			SharedPtr<PatchagePort> port = driver->create_port_view(patchage, _port_1);
			if (port)
				patchage->enqueue_resize(port->module().lock());
			else
				Raul::error << "Unable to create port view: " << _port_1 << endl;
		} else {
			Raul::error << "Attempt to create port with unknown type: " << _port_1 << endl;
		}

	} else if (_type == PORT_DESTRUCTION) {

		SharedPtr<PatchagePort> port = patchage->canvas()->find_port(_port_1);

		if (port) {
			SharedPtr<PatchageModule> module = PtrCast<PatchageModule>(port->module().lock());
			assert(module);

			module->remove_port(port);
			port.reset();

			// No empty modules (for now)
			if (module->num_ports() == 0) {
				patchage->canvas()->remove_item(module);
				module.reset();
			} else {
				patchage->enqueue_resize(module);
			}

		} else {
			Raul::error << "Unable to find port `" << _port_1 << "' to destroy" << endl;
		}

	} else if (_type == CONNECTION) {

		SharedPtr<PatchagePort> port_1 = patchage->canvas()->find_port(_port_1);
		SharedPtr<PatchagePort> port_2 = patchage->canvas()->find_port(_port_2);

		if (!port_1)
			Raul::error << "Unable to find port `" << _port_1 << "' to connect" << endl;
		else if (!port_2)
			Raul::error << "Unable to find port `" << _port_2 << "' to connect" << endl;
		else
			patchage->canvas()->add_connection(port_1, port_2, port_1->color() + 0x22222200);

	} else if (_type == DISCONNECTION) {

		SharedPtr<PatchagePort> port_1 = patchage->canvas()->find_port(_port_1);
		SharedPtr<PatchagePort> port_2 = patchage->canvas()->find_port(_port_2);

		if (!port_1)
			Raul::error << "Unable to find port `" << _port_1 << "' to disconnect" << endl;
		else if (!port_2)
			Raul::error << "Unable to find port `" << _port_2 << "' to disconnect" << endl;
		else
			patchage->canvas()->remove_connection(port_1, port_2);
	}
}

