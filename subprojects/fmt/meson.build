# Copyright 2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR GPL-3.0-or-later

project(
  'fmt',
  ['cpp'],
  default_options: [
    'b_ndebug=if-release',
    'buildtype=release',
    'cpp_std=c++17',
  ],
  license: 'MIT',
  meson_version: '>= 0.56.0',
  version: '9.0.0',
)

include_dirs = include_directories('include')

fmt_dep = declare_dependency(include_directories: include_dirs)
