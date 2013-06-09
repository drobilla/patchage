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

#ifndef PATCHAGE_DRIVER_HPP
#define PATCHAGE_DRIVER_HPP

#include <boost/shared_ptr.hpp>
#include <sigc++/sigc++.h>

#include "PatchageEvent.hpp"

class PatchagePort;
class PatchageCanvas;

/** Trival driver base class */
class Driver {
public:
	virtual ~Driver() {}

	virtual void process_events(Patchage* app) = 0;

	virtual void attach(bool launch_daemon) = 0;
	virtual void detach()                   = 0;
	virtual bool is_attached() const        = 0;

	virtual void refresh() = 0;
	virtual void destroy_all() {}

	virtual PatchagePort* create_port_view(Patchage*     patchage,
	                                       const PortID& id) = 0;

	virtual bool connect(PatchagePort* src_port,
	                     PatchagePort* dst_port) = 0;

	virtual bool disconnect(PatchagePort* src_port,
	                        PatchagePort* dst_port) = 0;

	sigc::signal<void> signal_attached;
	sigc::signal<void> signal_detached;
};

#endif // PATCHAGE_DRIVER_HPP

