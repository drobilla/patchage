// Copyright 2021-2023 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

/*
  Configuration header that defines reasonable defaults at compile-time.

  This allows configuration from the command-line (usually by the build system)
  while still allowing the code to compile "as-is" with reasonable default
  features on supported platforms.

  This system is designed so that, ideally, no command-line or build-system
  configuration is needed, but automatic feature detection can be disabled or
  overridden for maximum control.  It should never be necessary to edit the
  source code to achieve a given configuration.

  Usage:

  - By default, features are enabled if they can be detected or assumed to be
    available from the build environment, unless `PATCHAGE_NO_DEFAULT_CONFIG`
    is defined, which disables everything by default.

  - If a symbol like `HAVE_SOMETHING` is defined to non-zero, then the
    "something" feature is assumed to be available.

  Code rules:

  - To check for a feature, this header must be included, and the symbol
    `USE_SOMETHING` used as a boolean in an `#if` expression.

  - None of the other configuration symbols described here may be used
    directly.  In particular, this header should be the only place in the
    source code that touches `HAVE` symbols.
*/

#ifndef PATCHAGE_CONFIG_H
#define PATCHAGE_CONFIG_H

// Define version unconditionally so a warning will catch a mismatch
#define PATCHAGE_VERSION "1.0.11"

#if !defined(PATCHAGE_NO_DEFAULT_CONFIG)

// Classic UNIX: dladdr()
#  ifndef HAVE_DLADDR
#    ifdef __has_include
#      if __has_include(<dlfcn.h>)
#        define HAVE_DLADDR 1
#      endif
#    elif defined(__unix__) || defined(__APPLE__)
#      define HAVE_DLADDR 1
#    endif
#  endif

// GNU gettext()
#  ifndef HAVE_GETTEXT
#    ifdef __has_include
#      if __has_include(<libintl.h>)
#        define HAVE_GETTEXT 1
#      endif
#    endif
#  endif

// JACK metadata API
#  ifndef HAVE_JACK_METADATA
#    ifdef __has_include
#      if __has_include(<jack/metadata.h>)
#        define HAVE_JACK_METADATA 1
#      endif
#    endif
#  endif

#endif // !defined(PATCHAGE_NO_DEFAULT_CONFIG)

/*
  Make corresponding USE_FEATURE defines based on the HAVE_FEATURE defines from
  above or the command line.  The code checks for these using #if (not #ifdef),
  so there will be an undefined warning if it checks for an unknown feature,
  and this header is always required by any code that checks for features, even
  if the build system defines them all.
*/

#if defined(HAVE_DLADDR) && HAVE_DLADDR
#  define USE_DLADDR 1
#else
#  define USE_DLADDR 0
#endif

#if defined(HAVE_GETTEXT) && HAVE_GETTEXT
#  define USE_GETTEXT 1
#else
#  define USE_GETTEXT 0
#endif

#if defined(HAVE_JACK_METADATA) && HAVE_JACK_METADATA
#  define USE_JACK_METADATA 1
#else
#  define USE_JACK_METADATA 0
#endif

#if !defined(PATCHAGE_USE_LIGHT_THEME)
#  define PATCHAGE_USE_LIGHT_THEME 0
#endif

#ifndef PATCHAGE_BUNDLED
#  ifdef __APPLE__
#    define PATCHAGE_BUNDLED 1
#  else
#    define PATCHAGE_BUNDLED 0
#  endif
#endif

#endif // PATCHAGE_CONFIG_H
