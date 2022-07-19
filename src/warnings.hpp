// Copyright 2020 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PATCHAGE_WARNINGS_HPP
#define PATCHAGE_WARNINGS_HPP

#if defined(__clang__)

#  define PATCHAGE_DISABLE_FMT_WARNINGS                                     \
    _Pragma("clang diagnostic push")                                        \
    _Pragma("clang diagnostic ignored \"-Wdocumentation-unknown-command\"") \
    _Pragma("clang diagnostic ignored \"-Wglobal-constructors\"")           \
    _Pragma("clang diagnostic ignored \"-Wsigned-enum-bitfield\"")

// clang-format off
#	define PATCHAGE_DISABLE_GANV_WARNINGS  \
		_Pragma("clang diagnostic push")    \
		_Pragma(                                                            \
			"clang diagnostic ignored \"-Wdocumentation-unknown-command\"")
// clang-format on

#  define PATCHAGE_RESTORE_WARNINGS _Pragma("clang diagnostic pop")

#elif defined(__GNUC__)

#  define PATCHAGE_DISABLE_FMT_WARNINGS _Pragma("GCC diagnostic push")

#  define PATCHAGE_DISABLE_GANV_WARNINGS

#  define PATCHAGE_RESTORE_WARNINGS _Pragma("GCC diagnostic pop")

#else

#  define PATCHAGE_DISABLE_GANV_WARNINGS
#  define PATCHAGE_RESTORE_WARNINGS

#endif

#if defined(__GNUC__)
#  define PATCHAGE_UNREACHABLE() __builtin_unreachable()
#else
#  define PATCHAGE_UNREACHABLE()
#endif

#endif // PATCHAGE_WARNINGS_HPP
