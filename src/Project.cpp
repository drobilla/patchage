/* This file is part of Patchage.
 * Copyright 2008-2011 David Robillard <http://drobilla.net>
 * Copyright 2008 Nedko Arnaudov <nedko@arnaudov.name>
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

#include "Project.hpp"
#include "Client.hpp"

using boost::shared_ptr;
using std::string;
using std::list;

struct ProjectImpl {
	string     name;
	string     description;
	string     notes;
	bool       modified_status;
	list< shared_ptr<Client> > clients;
};

Project::Project(const string& name, const LoadedProjectProperties& properties)
{
	_impl                  = new ProjectImpl();
	_impl->name            = name;
	_impl->description     = properties.description;
	_impl->notes           = properties.notes;
	_impl->modified_status = properties.modified_status;
}

Project::~Project()
{
	delete _impl;
}

void
Project::clear()
{
	shared_ptr<Client> client;

	while (!_impl->clients.empty()) {
		client = _impl->clients.front();
		_impl->clients.pop_front();
		_signal_client_removed.emit(client);
	}
}

const string&
Project::get_name() const
{
	return _impl->name;
}

void
Project::on_name_changed(const string& name)
{
	_impl->name = name;
	_signal_renamed.emit();
}

const string&
Project::get_description() const
{
	return _impl->description;
}

const string&
Project::get_notes() const
{
	return _impl->notes;
}

bool
Project::get_modified_status() const
{
	return _impl->modified_status;
}

const Project::Clients&
Project::get_clients() const
{
	return _impl->clients;
}

void
Project::on_client_added(shared_ptr<Client> client)
{
	_impl->clients.push_back(client);
	_signal_client_added.emit(client);
}

void
Project::on_client_removed(const string& id)
{
	shared_ptr<Client> client;

	for (list< shared_ptr<Client> >::iterator iter = _impl->clients.begin();
	     iter != _impl->clients.end(); iter++) {
		client = *iter;

		if (client->get_id() == id) {
			_signal_client_removed.emit(client);
			_impl->clients.erase(iter);
			return;
		}
	}
}

void
Project::on_modified_status_changed(bool modified_status)
{
	_impl->modified_status = modified_status;
	_signal_modified_status_changed.emit();
}

void
Project::on_description_changed(const string& description)
{
	_impl->description = description;
	_signal_description_changed.emit();
}

void
Project::on_notes_changed(const string& notes)
{
	_impl->notes = notes;
	_signal_notes_changed.emit();
}

