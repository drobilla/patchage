# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: CC0-1.0 OR GPL-3.0-or-later

# Project-specific warning suppressions.
#
# This should be used in conjunction with the generic "warnings" sibling that
# enables all reasonable warnings for the compiler.  It lives here just to keep
# the top-level meson.build more readable.

#######
# C++ #
#######

if is_variable('cpp')
  cpp_suppressions = []

  if get_option('strict')
    if cpp.get_id() == 'clang'
      cpp_suppressions += [
        '-Wno-alloca',
        '-Wno-cast-qual',
        '-Wno-double-promotion',
        '-Wno-float-conversion',
        '-Wno-float-equal',
        '-Wno-implicit-float-conversion',
        '-Wno-padded',
        '-Wno-pedantic',
        '-Wno-shorten-64-to-32',
        '-Wno-sign-conversion',
        '-Wno-weak-vtables',
      ]

      if host_machine.system() == 'darwin'
        cpp_suppressions += [
          '-Wno-documentation', # JACK
          '-Wno-documentation-deprecated-sync', # JACK
          '-Wno-documentation-unknown-command', # JACK
          '-Wno-reserved-id-macro', # JACK
        ]
      endif

    elif cpp.get_id() == 'gcc'
      cpp_suppressions += [
        '-Wno-abi-tag',
        '-Wno-alloca',
        '-Wno-conditionally-supported',
        '-Wno-conversion',
        '-Wno-effc++',
        '-Wno-float-equal',
        '-Wno-inline',
        '-Wno-null-dereference',
        '-Wno-padded',
        '-Wno-pedantic',
        '-Wno-stack-protector',
        '-Wno-strict-overflow',
        '-Wno-suggest-attribute=const',
        '-Wno-suggest-attribute=noreturn',
        '-Wno-suggest-attribute=pure',
        '-Wno-suggest-final-methods',
        '-Wno-suggest-final-types',
        '-Wno-switch-default',
      ]
    endif
  endif

  cpp_suppressions = cpp.get_supported_arguments(cpp_suppressions)
endif
