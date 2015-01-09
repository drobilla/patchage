/* This file is part of Patchage.
 * Copyright 2007-2014 David Robillard <http://drobilla.net>
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

#ifdef __APPLE__
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <gtk/gtkrc.h>
#include "binary_location.h"
#endif

#include <iostream>

#include <glibmm/exception.h>

#include "Patchage.hpp"

int
main(int argc, char** argv)
{
#ifdef __APPLE__
	const std::string binary     = binary_location();
	const std::string bundle     = binary.substr(0, binary.find_last_of('/'));
	const std::string gtkrc_path = bundle + "/Resources/gtkrc";
	if (Glib::file_test(gtkrc_path, Glib::FILE_TEST_EXISTS)) {
		gtk_rc_parse(gtkrc_path.c_str());
	}
#endif

	try {

	Glib::thread_init();

	Gtk::Main app(argc, argv);

	Patchage patchage(argc, argv);
	app.run(*patchage.window());

	} catch (std::exception& e) {
		std::cerr << "patchage: error: " << e.what() << std::endl;
		return 1;
	} catch (Glib::Exception& e) {
		std::cerr << "patchage: error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}

