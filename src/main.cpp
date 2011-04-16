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

#ifdef __APPLE__
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <gtk/gtk.h>
#include "binary_location.h"
#endif

#include <iostream>

#include <libgnomecanvasmm.h>
#include <glibmm/exception.h>

#include "raul/log.hpp"

#include "Patchage.hpp"

#ifdef __APPLE__
void
set_bundle_environment()
{
	char* binloc_c = binary_location();
	std::string binloc(binloc_c);
	free(binloc_c);

	const std::string bundle_path = binloc.substr(0, binloc.find_last_of('/'));

	const std::string gtk_path(bundle_path + "/lib");
	setenv("GTK_PATH", gtk_path.c_str(), 1);
	std::cout << "GTK PATH " << gtk_path << std::endl;

	chdir(bundle_path.c_str());
	const std::string pangorc_path(bundle_path + "/Resources/pangorc");
	setenv("PANGO_RC_FILE", pangorc_path.c_str(), 1);

	const char* path_c = getenv("PATH");
	std::string path = "/opt/local/bin";
	if (path_c)
		path += std::string(":") + path_c;
	setenv("PATH", path.c_str(), 1);

	gtk_rc_parse((bundle_path + "/Resources/gtkrc").c_str());
}
#endif

int
main(int argc, char** argv)
{
#ifdef __APPLE__
	set_bundle_environment();
#endif

	try {

	Glib::thread_init();

	Gnome::Canvas::init();
	Gtk::Main app(argc, argv);

	Patchage patchage(argc, argv);
	app.run(*patchage.window());

	} catch (std::exception& e) {
		Raul::error << "patchage: error: " << e.what() << std::endl;
		return 1;
	} catch (Glib::Exception& e) {
		Raul::error << "patchage: error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}

