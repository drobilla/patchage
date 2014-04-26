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
	#define _GNU_SOURCE
#endif

#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <dlfcn.h>

/** Return the absolute path of the binary.
 * Returned value must be freed by caller.
 */
static char*
binary_location()
{
	Dl_info dli;
	const int ret = dladdr((void*)&binary_location, &dli);
	if (ret) {
		char* const bin_loc = (char*)calloc(PATH_MAX, 1);
		if (!realpath(dli.dli_fname, bin_loc)) {
			return NULL;
		}
		return bin_loc;
	}
	return NULL;
}
