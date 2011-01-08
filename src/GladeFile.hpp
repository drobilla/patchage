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

#ifndef PATCHAGE_GLADEFILE_HPP
#define PATCHAGE_GLADEFILE_HPP

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <libglademm/xml.h>

#include "raul/log.hpp"

#include "patchage-config.h"
#ifdef PATCHAGE_BINLOC
#include "binary_location.h"
#endif

class GladeFile {
public:
	inline static bool is_readable(const std::string& filename) {
		std::ifstream fs(filename.c_str());
		const bool fail = fs.fail();
		fs.close();
		return !fail;
	}
		
	static Glib::RefPtr<Gnome::Glade::Xml> open(const std::string& base_name) {
		std::string glade_filename;
		char* loc = NULL;
#ifdef PATCHAGE_BINLOC
		loc = binary_location();
		if (loc) {
			std::string bundle = loc;
			bundle = bundle.substr(0, bundle.find_last_of("/"));
			glade_filename = bundle + "/" + base_name + ".glade";
			free(loc);
			if (is_readable(glade_filename)) {
				Raul::info << "Loading glade file " << glade_filename << std::endl;
				return Gnome::Glade::Xml::create(glade_filename);
			}
		}
#endif
		glade_filename = std::string(PATCHAGE_DATA_DIR) + "/" + base_name + ".glade";
		if (is_readable(glade_filename)) {
			Raul::info << "Loading glade file " << glade_filename << std::endl;
			return Gnome::Glade::Xml::create(glade_filename);
		}

		std::stringstream ss;
		ss << "Unable to find " << base_name << ".glade in " << loc
		   << " or " << PATCHAGE_DATA_DIR << std::endl;
		throw std::runtime_error(ss.str());
		return Glib::RefPtr<Gnome::Glade::Xml>();
		//return Gnome::Glade::Xml::create(glade_filename);
	}
};

#endif // PATCHAGE_GLADEFILE_HPP
