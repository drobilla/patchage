/* This file is part of Patchage.
 * Copyright 2008-2020 David Robillard <d@drobilla.net>
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

#ifndef PATCHAGE_PORTID_HPP
#define PATCHAGE_PORTID_HPP

#include "patchage_config.h"

#include "PatchagePort.hpp"

#ifdef PATCHAGE_LIBJACK
#	include <jack/jack.h>
#endif
#ifdef HAVE_ALSA
#	include <alsa/asoundlib.h>
#endif

#include <cstring>
#include <iostream>

struct PortID
{
	enum class Type
	{
		nothing,
		jack_id,
		alsa_addr,
	};

	PortID() = default;

	PortID(const PortID& copy)
	    : type(copy.type)
	{
		memcpy(&id, &copy.id, sizeof(id));
	}

#ifdef PATCHAGE_LIBJACK
	explicit PortID(jack_port_id_t jack_id, bool ign = false)
	    : type(Type::jack_id)
	{
		id.jack_id = jack_id;
	}
#endif

#ifdef HAVE_ALSA
	PortID(snd_seq_addr_t addr, bool in)
	    : type(Type::alsa_addr)
	{
		id.alsa_addr = addr;
		id.is_input  = in;
	}
#endif

	Type type = Type::nothing;

	union
	{
#ifdef PATCHAGE_LIBJACK
		jack_port_id_t jack_id;
#endif
#ifdef HAVE_ALSA
		struct
		{
			snd_seq_addr_t alsa_addr;
			bool           is_input : 1;
		};
#endif
	} id = {};
};

static inline std::ostream&
operator<<(std::ostream& os, const PortID& id)
{
	switch (id.type) {
	case PortID::Type::nothing:
		return os << "(null)";
	case PortID::Type::jack_id:
#ifdef PATCHAGE_LIBJACK
		return os << "jack:" << id.id.jack_id;
#endif
		break;
	case PortID::Type::alsa_addr:
#ifdef HAVE_ALSA
		return os << "alsa:" << int(id.id.alsa_addr.client) << ":"
		          << int(id.id.alsa_addr.port) << ":"
		          << (id.id.is_input ? "in" : "out");
#endif
		break;
	}
	assert(false);
	return os;
}

static inline bool
operator<(const PortID& a, const PortID& b)
{
	if (a.type != b.type) {
		return a.type < b.type;
	}

	switch (a.type) {
	case PortID::Type::nothing:
		return true;
	case PortID::Type::jack_id:
#ifdef PATCHAGE_LIBJACK
		return a.id.jack_id < b.id.jack_id;
#endif
		break;
	case PortID::Type::alsa_addr:
#ifdef HAVE_ALSA
		if ((a.id.alsa_addr.client < b.id.alsa_addr.client) ||
		    ((a.id.alsa_addr.client == b.id.alsa_addr.client) &&
		     a.id.alsa_addr.port < b.id.alsa_addr.port)) {
			return true;
		} else if (a.id.alsa_addr.client == b.id.alsa_addr.client &&
		           a.id.alsa_addr.port == b.id.alsa_addr.port) {
			return (a.id.is_input < b.id.is_input);
		} else {
			return false;
		}
#endif
		break;
	}
	assert(false);
	return false;
}

#endif // PATCHAGE_PORTID_HPP
