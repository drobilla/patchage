/* This file is part of Patchage.
 * Copyright 2007-2011 David Robillard <http://drobilla.net>
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

#ifndef PATCHAGE_PATCHAGEPORT_HPP
#define PATCHAGE_PATCHAGEPORT_HPP

#include <string>

#include <boost/shared_ptr.hpp>

#include "flowcanvas/Port.hpp"
#include "flowcanvas/Module.hpp"

#include "patchage-config.h"
#include "PatchageCanvas.hpp"
#include "PortID.hpp"
#include "StateManager.hpp"

#ifdef HAVE_ALSA
  #include <alsa/asoundlib.h>
#endif

/** A Port on a PatchageModule
 */
class PatchagePort : public FlowCanvas::Port
{
public:
	PatchagePort(boost::shared_ptr<FlowCanvas::Module> module,
	             PortType                              type,
	             const std::string&                    name,
	             bool                                  is_input,
	             uint32_t                              color)
		: Port(module, name, is_input, color)
		, _type(type)
	{
#ifdef HAVE_ALSA
		_alsa_addr.client = '\0';
		_alsa_addr.port   = '\0';
#endif
	}

	virtual ~PatchagePort() {}

#ifdef HAVE_ALSA
	// FIXME: This driver specific crap really needs to go
	void                  alsa_addr(const snd_seq_addr_t addr) { _alsa_addr = addr; }
	const snd_seq_addr_t* alsa_addr() const { return (_type == ALSA_MIDI) ? &_alsa_addr : NULL; }
#endif

	/** Returns the full name of this port, as "modulename:portname" */
	std::string full_name() const { return _module->name() + ":" + _name; }

	PortType type() const { return _type; }

private:
#ifdef HAVE_ALSA
	snd_seq_addr_t _alsa_addr;
#endif
	PortType       _type;
};

#endif // PATCHAGE_PATCHAGEPORT_HPP
