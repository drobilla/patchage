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

#ifndef PATCHAGE_PATCHAGEEVENT_HPP
#define PATCHAGE_PATCHAGEEVENT_HPP

#include "patchage_config.h"

#include "PatchagePort.hpp"
#include "PortID.hpp"

#ifdef PATCHAGE_LIBJACK
#	include <jack/jack.h>
#endif
#ifdef HAVE_ALSA
#	include <alsa/asoundlib.h>
#endif

#include <cstring>

class Patchage;

/** A Driver event to be processed by the GUI thread.
 */
class PatchageEvent
{
public:
	enum class Type : uint8_t
	{
		noop,
		refresh,
		client_creation,
		client_destruction,
		port_creation,
		port_destruction,
		connection,
		disconnection,
	};

	explicit PatchageEvent(Type type = Type::noop)
	    : _str(nullptr)
	    , _type(type)
	{}

	PatchageEvent(Type type, const char* str)
	    : _str(g_strdup(str))
	    , _type(type)
	{}

	template<typename P>
	PatchageEvent(Type type, P port)
	    : _str(nullptr)
	    , _port_1(port)
	    , _type(type)
	{}

	template<typename P>
	PatchageEvent(Type type, P port_1, P port_2)
	    : _str(nullptr)
	    , _port_1(port_1, false)
	    , _port_2(port_2, true)
	    , _type(type)
	{}

	void execute(Patchage* patchage);

	inline Type type() const { return _type; }

private:
	char*  _str;
	PortID _port_1;
	PortID _port_2;
	Type   _type;
};

#endif // PATCHAGE_PATCHAGEEVENT_HPP
