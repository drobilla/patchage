/* This file is part of Patchage.
 * Copyright (C) 2008-2010 David Robillard <http://drobilla.net>
 * Copyright (C) 2008 Nedko Arnaudov <nedko@arnaudov.name>
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

#include "Client.hpp"
#include "Patchage.hpp"

using namespace std;

struct ClientImpl {
	Project* project;
	string   id;
	string   name;
};

Client::Client(
    Project*      project,
    const string& id,
    const string& name)
{
	_impl          = new ClientImpl();
	_impl->project = project;
	_impl->id      = id;
	_impl->name    = name;
}

Client::~Client()
{
	delete _impl;
}

Project*
Client::get_project()
{
	return _impl->project;
}

const string&
Client::get_id() const
{
	return _impl->id;
}

const string&
Client::get_name() const
{
	return _impl->name;
}

void
Client::set_name(const string& name)
{
	if (_impl->name != name) {
		_impl->name = name;
		_signal_renamed.emit();
	}
}
