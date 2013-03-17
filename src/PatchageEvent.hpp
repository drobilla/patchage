/* This file is part of Patchage.
 * Copyright 2007-2013 David Robillard <http://drobilla.net>
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

#ifndef PATCHAGE_PATCHAGEEVENT_HPP
#define PATCHAGE_PATCHAGEEVENT_HPP

#include <cstring>

#include "patchage_config.h"

#ifdef PATCHAGE_LIBJACK
  #include <jack/jack.h>
#endif
#ifdef HAVE_ALSA
  #include <alsa/asoundlib.h>
#endif

#include "PatchagePort.hpp"
#include "PortID.hpp"

class Patchage;

/** A Driver event to be processed by the GUI thread.
 */
class PatchageEvent {
public:
	enum Type {
		NULL_EVENT = 0,
		REFRESH,
		CLIENT_CREATION,
		CLIENT_DESTRUCTION,
		PORT_CREATION,
		PORT_DESTRUCTION,
		CONNECTION,
		DISCONNECTION
	};

	explicit PatchageEvent(Type type=NULL_EVENT)
		: _str(NULL)
		, _type(type)
	{}

	PatchageEvent(Type type, const char* str)
		: _str(g_strdup(str))
		, _type(type)
	{}

	template <typename P>
	PatchageEvent(Type type, P port)
		: _str(NULL)
		, _port_1(port)
		, _type(type)
	{}

	template <typename P>
	PatchageEvent(Type type, P port_1, P port_2)
		: _str(NULL)
		, _port_1(port_1, false)
		, _port_2(port_2, true)
		, _type(type)
	{}

	void execute(Patchage* patchage);

	inline Type type() const { return (Type)_type; }

private:
	char*   _str;
	PortID  _port_1;
	PortID  _port_2;
	uint8_t _type;
};

#endif // PATCHAGE_PATCHAGEEVENT_HPP

