/* This file is part of Patchage.
 * Copyright 2008-2014 David Robillard <http://drobilla.net>
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

#ifndef _GNU_SOURCE
#	define _GNU_SOURCE
#endif

#include <dlfcn.h>

#include <cassert>
#include <climits>
#include <cstdlib>
#include <string>

/** Return the absolute path of the binary. */
inline std::string
binary_location()
{
	Dl_info     dli;
	std::string loc;
	const int   ret = dladdr((void*)&binary_location, &dli);
	if (ret) {
		if (char* const bin_loc = realpath(dli.dli_fname, nullptr)) {
			loc = bin_loc;
			free(bin_loc);
		}
	}
	return loc;
}

/** Return the absolute path of the bundle (binary parent directory). */
inline std::string
bundle_location()
{
	const std::string binary = binary_location();
	if (binary.empty()) {
		return "";
	}
	return binary.substr(0, binary.find_last_of('/'));
}
