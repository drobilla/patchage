# Copyright 2020-2023 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR GPL-3.0-or-later

project(
  'patchage',
  ['c', 'cpp'],
  default_options: [
    'b_ndebug=if-release',
    'buildtype=release',
    'cpp_std=c++17',
  ],
  license: 'GPLv3+',
  meson_version: '>= 0.56.0',
  version: '1.0.11',
)

patchage_src_root = meson.current_source_dir()
major_version = meson.project_version().split('.')[0]
version_suffix = '@0@-@1@'.format(meson.project_name(), major_version)

#######################
# Compilers and Flags #
#######################

# Required tools
cpp = meson.get_compiler('cpp')

# Set global warning suppressions
subdir('meson/suppressions')
add_project_arguments(cpp_suppressions, language: ['cpp'])
add_project_arguments(['-DFMT_HEADER_ONLY'], language: ['cpp'])

##########################
# Platform Configuration #
##########################

patchage_datadir = get_option('prefix') / get_option('datadir') / 'patchage'
patchage_localedir = get_option('prefix') / get_option('localedir')

platform_defines = [
  '-DPATCHAGE_VERSION="@0@"'.format(meson.project_version()),
  '-DPATCHAGE_DATA_DIR="@0@"'.format(patchage_datadir),
  '-DPATCHAGE_LOCALE_DIR="@0@"'.format(patchage_localedir),
]

if host_machine.system() in ['gnu', 'linux']
  platform_defines += ['-D_GNU_SOURCE']
endif

if get_option('checks').disabled()
  # Generic build without platform-specific features
  platform_defines += ['-DPATCHAGE_NO_DEFAULT_CONFIG']
elif get_option('checks').enabled()
  # Only use the features detected by the build system
  platform_defines += ['-DPATCHAGE_NO_DEFAULT_CONFIG']

  dladdr_code = '''#include <dlfcn.h>
int main(void) { Dl_info info; return dladdr(&info, &info); }'''

  gettext_code = '''#include <libintl.h>
int main(void) { return !!gettext("hello"); }'''

  jack_metadata_code = '''#include <jack/metadata.h>
int main(void) { return !!&jack_set_property; }'''

  if cpp.compiles(dladdr_code, args: platform_defines, name: 'dladdr')
    platform_defines += [
      '-DHAVE_DLADDR=1',
      '-DPATCHAGE_BUNDLED=1',
    ]
  else
    platform_defines += ['-DHAVE_DLADDR=0']
  endif

  platform_defines += '-DHAVE_GETTEXT=@0@'.format(
    cpp.compiles(gettext_code, args: platform_defines, name: 'gettext').to_int(),
  )

  platform_defines += '-DHAVE_JACK_METADATA=@0@'.format(
    cpp.compiles(
      jack_metadata_code,
      args: platform_defines,
      name: 'jack_metadata',
    ).to_int(),
  )

endif

################
# Dependencies #
################

m_dep = cpp.find_library('m', required: false)
dl_dep = cpp.find_library('dl', required: false)
thread_dep = dependency('threads', include_type: 'system')

fmt_dep = dependency(
  'fmt',
  fallback: ['fmt', 'fmt_dep'],
  include_type: 'system',
  version: ['>= 9.0.0', '<= 10.0.0'],
)

gthread_dep = dependency(
  'gthread-2.0',
  include_type: 'system',
  version: '>= 2.14.0',
)

glibmm_dep = dependency(
  'glibmm-2.4',
  include_type: 'system',
  version: '>= 2.14.0',
)

gtkmm_dep = dependency(
  'gtkmm-2.4',
  include_type: 'system',
  version: '>= 2.12.0',
)

ganv_dep = dependency(
  'ganv-1',
  include_type: 'system',
  version: '>= 1.8.2',
)

dependencies = [
  dl_dep,
  fmt_dep,
  ganv_dep,
  glibmm_dep,
  gthread_dep,
  gtkmm_dep,
  m_dep,
  thread_dep,
]

#######################
# Driver Dependencies #
#######################

# Optional ALSA sequencer support
alsa_dep = dependency(
  'alsa',
  include_type: 'system',
  required: get_option('alsa'),
)

# Optional JACK support
jack_dep = dependency(
  'jack',
  include_type: 'system',
  required: get_option('jack'),
  version: '>= 0.120.0',
)

# Optional JACK D-Bus support

dbus_dep = dependency(
  'dbus-1',
  include_type: 'system',
  required: get_option('jack_dbus'),
)

dbus_glib_dep = dependency(
  'dbus-glib-1',
  include_type: 'system',
  required: get_option('jack_dbus'),
)

if jack_dep.found() and dbus_dep.found() and dbus_glib_dep.found()
  message('Both libjack and D-Bus available, defaulting to libjack')
endif

#######################
# Translations (i18n) #
#######################

subdir('po')

###########
# Program #
###########

sources = files(
  'src/Canvas.cpp',
  'src/CanvasModule.cpp',
  'src/Configuration.cpp',
  'src/Drivers.cpp',
  'src/Legend.cpp',
  'src/Metadata.cpp',
  'src/Patchage.cpp',
  'src/Reactor.cpp',
  'src/TextViewLog.cpp',
  'src/event_to_string.cpp',
  'src/handle_event.cpp',
  'src/main.cpp',
)

if alsa_dep.found()
  sources += files('src/AlsaDriver.cpp')
  dependencies += [alsa_dep]
else
  sources += files('src/AlsaStubDriver.cpp')
endif

if jack_dep.found()
  sources += files('src/JackLibDriver.cpp')
  dependencies += [jack_dep]
elif dbus_dep.found() and dbus_glib_dep.found()
  sources += files('src/JackDbusDriver.cpp')
  dependencies += [dbus_dep, dbus_glib_dep]
else
  sources += files('src/JackStubDriver.cpp')
endif

executable(
  'patchage',
  sources,
  cpp_args: cpp_suppressions + platform_defines,
  dependencies: dependencies,
  install: true,
)

########
# Data #
########

subdir('icons')

config = configuration_data()
config.set('PATCHAGE_VERSION', meson.project_version())
config.set('BINDIR', get_option('prefix') / get_option('bindir'))

configure_file(
  configuration: config,
  input: files('src/patchage.ui.in'),
  install: true,
  install_dir: get_option('datadir') / 'patchage',
  output: 'patchage.ui',
)

configure_file(
  configuration: config,
  input: files('patchage.desktop.in'),
  install: true,
  install_dir: get_option('datadir') / 'applications',
  output: 'patchage.desktop',
)

install_man(files('doc/patchage.1'))

#########
# Tests #
#########

if get_option('lint')
  if not meson.is_subproject()
    # Check release metadata
    autoship = find_program('autoship', required: false)
    if autoship.found()
      test(
        'autoship',
        autoship,
        args: ['test', patchage_src_root],
        suite: 'data',
      )
    endif
  endif

  # Check licensing metadata
  reuse = find_program('reuse', required: false)
  if reuse.found()
    test(
      'REUSE',
      reuse,
      args: ['--root', patchage_src_root, 'lint'],
      suite: 'data',
    )
  endif
endif

if not meson.is_subproject()
  summary('Install prefix', get_option('prefix'))
  summary('Executables', get_option('prefix') / get_option('bindir'))
  summary('Man pages', get_option('prefix') / get_option('mandir'))
endif
