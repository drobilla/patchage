// Copyright 2021-2022 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: GPL-3.0-or-later

/*
  Configuration header that defines reasonable defaults at compile time.

  This allows compile-time configuration from the command line, while still
  allowing the source to be built "as-is" without any configuration.  The idea
  is to support an advanced build system with configuration checks, while still
  allowing the code to be simply "thrown at a compiler" with features
  determined from the compiler or system headers.  Everything can be
  overridden, so it should never be necessary to edit this file to build
  successfully.

  To ensure that all configure checks are performed, the build system can
  define PATCHAGE_NO_DEFAULT_CONFIG to disable defaults.  In this case, it must
  define all HAVE_FEATURE symbols below to 1 or 0 to enable or disable
  features.  Any missing definitions will generate a compiler warning.

  To ensure that this header is always included properly, all code that uses
  configuration variables includes this header and checks their value with #if
  (not #ifdef).  Variables like USE_FEATURE are internal and should never be
  defined on the command line.
*/

#ifndef PATCHAGE_CONFIG_H
#define PATCHAGE_CONFIG_H

// Define version unconditionally so a warning will catch a mismatch
#define PATCHAGE_VERSION "1.0.7"

#if !defined(PATCHAGE_NO_DEFAULT_CONFIG)

// Classic UNIX: dladdr()
#  ifndef HAVE_DLADDR
#    ifdef __has_include
#      if __has_include(<dlfcn.h>)
#        define HAVE_DLADDR 1
#      else
#        define HAVE_DLADDR 0
#      endif
#    elif defined(__unix__) || defined(__APPLE__)
#      define HAVE_DLADDR 1
#    else
#      define HAVE_DLADDR 0
#    endif
#  endif

// JACK metadata API
#  ifndef HAVE_JACK_METADATA
#    ifdef __has_include
#      if __has_include(<jack/metadata.h>)
#        define HAVE_JACK_METADATA 1
#      else
#        define HAVE_JACK_METADATA 0
#      endif
#    else
#      define HAVE_JACK_METADATA 0
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

#if HAVE_DLADDR
#  define USE_DLADDR 1
#else
#  define USE_DLADDR 0
#endif

#if HAVE_JACK_METADATA
#  define USE_JACK_METADATA 1
#else
#  define USE_JACK_METADATA 0
#endif

#ifndef PATCHAGE_USE_LIGHT_THEME
#  define PATCHAGE_USE_LIGHT_THEME 0
#endif

#endif // PATCHAGE_CONFIG_H
