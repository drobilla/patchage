/* This file is part of Patchage.
 * Copyright 2020 David Robillard <d@drobilla.net>
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

#ifndef PATCHAGE_WARNINGS_HPP
#define PATCHAGE_WARNINGS_HPP

#if defined(__clang__)

#	define PATCHAGE_DISABLE_FMT_WARNINGS                                   \
		_Pragma("clang diagnostic push")                                    \
		_Pragma(                                                            \
		    "clang diagnostic ignored \"-Wdocumentation-unknown-command\"") \
		_Pragma("clang diagnostic ignored \"-Wglobal-constructors\"")       \
		_Pragma("clang diagnostic ignored \"-Wsigned-enum-bitfield\"")

#	define PATCHAGE_DISABLE_GANV_WARNINGS                                    \
		_Pragma("clang diagnostic push")                                      \
		_Pragma(                                                              \
		    "clang diagnostic ignored \"-Wdocumentation-unknown-command\"")   \
		_Pragma("clang diagnostic ignored \"-Wsuggest-destructor-override\"") \
		_Pragma("clang diagnostic ignored \"-Wsuggest-override\"")            \
		_Pragma("clang diagnostic ignored \"-Wunused-parameter\"")

#	define PATCHAGE_RESTORE_WARNINGS _Pragma("clang diagnostic pop")

#elif defined(__GNUC__)

#	define PATCHAGE_DISABLE_FMT_WARNINGS _Pragma("GCC diagnostic push")

#	define PATCHAGE_DISABLE_GANV_WARNINGS                       \
		_Pragma("GCC diagnostic push")                           \
		_Pragma("GCC diagnostic ignored \"-Wsuggest-override\"") \
		_Pragma("GCC diagnostic ignored \"-Wunused-parameter\"")

#	define PATCHAGE_RESTORE_WARNINGS _Pragma("GCC diagnostic pop")

#else

#	define PATCHAGE_DISABLE_GANV_WARNINGS
#	define PATCHAGE_RESTORE_WARNINGS

#endif

#endif // PATCHAGE_WARNINGS_HPP
