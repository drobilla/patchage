#!/usr/bin/env python
# Licensed under the GNU GPL v3 or later, see COPYING file for details.
# Copyright 2008-2013 David Robillard
# Copyright 2008 Nedko Arnaudov

import os

from waflib.extras import autowaf as autowaf
import waflib.Options as Options, waflib.Utils as Utils

# Version of this package (even if built as a child)
PATCHAGE_VERSION = '0.6.0'

# Variables for 'waf dist'
APPNAME = 'patchage'
VERSION = PATCHAGE_VERSION
APP_HUMAN_NAME = 'Patchage'

# Mandatory variables
top = '.'
out = 'build'

def options(opt):
    opt.load('compiler_cxx')
    autowaf.set_options(opt)
    opt.add_option('--patchage-install-name', type='string', default=APPNAME,
                    dest='patchage_install_name',
                    help='Patchage install name. [Default: '' + APPNAME + '']')
    opt.add_option('--patchage-human-name', type='string', default=APP_HUMAN_NAME,
                    dest='patchage_human_name',
                    help='Patchage human name [Default: '' + APP_HUMAN_NAME + '']')
    opt.add_option('--jack-dbus', action='store_true', dest='jack_dbus',
                    help='Use Jack via D-Bus [Default: False (use libjack)]')
    opt.add_option('--no-jack-session', action='store_true', dest='no_jack_session',
                    help='Do not build JACK session support')
    opt.add_option('--no-alsa', action='store_true', dest='no_alsa',
                    help='Do not build Alsa Sequencer support')
    opt.add_option('--no-binloc', action='store_true', dest='no_binloc',
                    help='Do not try to read files from executable location')

def configure(conf):
    conf.load('compiler_cxx')
    conf.line_just = 40
    autowaf.configure(conf)

    conf.check_cxx(cxxflags=["-std=c++0x"])
    conf.env.append_unique('CXXFLAGS', ['-std=c++0x'])

    autowaf.display_header('Patchage Configuration')
    autowaf.check_pkg(conf, 'dbus-1', uselib_store='DBUS',
                      mandatory=False)
    autowaf.check_pkg(conf, 'dbus-glib-1', uselib_store='DBUS_GLIB',
                      mandatory=False)
    autowaf.check_pkg(conf, 'gthread-2.0', uselib_store='GTHREAD',
                      atleast_version='2.14.0', mandatory=True)
    autowaf.check_pkg(conf, 'glibmm-2.4', uselib_store='GLIBMM',
                      atleast_version='2.14.0', mandatory=True)
    autowaf.check_pkg(conf, 'gtkmm-2.4', uselib_store='GTKMM',
                      atleast_version='2.12.0', mandatory=True)
    autowaf.check_pkg(conf, 'ganv-1', uselib_store='GANV',
                      atleast_version='1.2.1', mandatory=True)

    if Options.platform == 'darwin':
        autowaf.check_pkg(conf, 'gtk-mac-integration', uselib_store='GTK_OSX',
                          atleast_version='1.0.0', mandatory=True)
        if conf.is_defined('HAVE_GTK_OSX'):
            autowaf.define(conf, 'PATCHAGE_GTK_OSX', 1)

    # Check for dladdr
    conf.check(function_name='dladdr',
               header_name='dlfcn.h',
               defines=['_GNU_SOURCE'],
               lib=['dl'],
               define_name='HAVE_DLADDR',
               mandatory=False)

    # Use Jack D-Bus if requested (only one jack driver is allowed)
    if Options.options.jack_dbus and conf.is_defined('HAVE_DBUS') and conf.is_defined('HAVE_DBUS_GLIB'):
        autowaf.define(conf, 'HAVE_JACK_DBUS', 1)
    else:
        autowaf.check_pkg(conf, 'jack', uselib_store='JACK',
                          atleast_version='0.120.0', mandatory=False)
        if conf.is_defined('HAVE_JACK'):
            autowaf.define(conf, 'PATCHAGE_LIBJACK', 1)
            if not Options.options.no_jack_session:
                autowaf.define(conf, 'PATCHAGE_JACK_SESSION', 1)

    # Use Alsa if present unless --no-alsa
    if not Options.options.no_alsa:
        autowaf.check_pkg(conf, 'alsa', uselib_store='ALSA', mandatory=False)

    # Find files at binary location if we have dladdr unless --no-binloc
    if not Options.options.no_binloc and conf.is_defined('HAVE_DLADDR'):
        autowaf.define(conf, 'PATCHAGE_BINLOC', 1)

    # Boost headers
    autowaf.check_header(conf, 'cxx', 'boost/format.hpp')
    autowaf.check_header(conf, 'cxx', 'boost/shared_ptr.hpp')
    autowaf.check_header(conf, 'cxx', 'boost/utility.hpp')
    autowaf.check_header(conf, 'cxx', 'boost/weak_ptr.hpp')

    conf.env.PATCHAGE_VERSION = PATCHAGE_VERSION

    conf.env.APP_INSTALL_NAME = Options.options.patchage_install_name
    conf.env.APP_HUMAN_NAME = Options.options.patchage_human_name
    autowaf.define(conf, 'PATCHAGE_DATA_DIR', os.path.join(
                    conf.env.DATADIR, conf.env.APP_INSTALL_NAME))

    conf.write_config_header('patchage_config.h', remove=False)

    autowaf.display_msg(conf, "Install name", "'" + conf.env.APP_INSTALL_NAME + "'", 'CYAN')
    autowaf.display_msg(conf, "App human name", "'" + conf.env.APP_HUMAN_NAME + "'", 'CYAN')
    autowaf.display_msg(conf, "Jack (D-Bus)", conf.is_defined('HAVE_JACK_DBUS'))
    autowaf.display_msg(conf, "Jack (libjack)", conf.is_defined('PATCHAGE_LIBJACK'))
    autowaf.display_msg(conf, "Jack Session", conf.is_defined('PATCHAGE_JACK_SESSION'))
    autowaf.display_msg(conf, "Alsa Sequencer", conf.is_defined('HAVE_ALSA'))
    if Options.platform == 'darwin':
        autowaf.display_msg(conf, "Mac Integration", conf.is_defined('HAVE_GTK_OSX'))
        
    print('')

def build(bld):
    out_base = ''
    if Options.platform == 'darwin':
        out_base = 'Patchage.app/Contents/'

    # Program
    prog = bld(features     = 'cxx cxxprogram',
               includes     = ['.', 'src'],
               target       = out_base + bld.env.APP_INSTALL_NAME,
               install_path = '${BINDIR}')
    autowaf.use_lib(bld, prog, 'DBUS GANV DBUS_GLIB GTKMM GNOMECANVAS GTHREAD GTK_OSX')
    prog.source = '''
            src/Patchage.cpp
            src/PatchageCanvas.cpp
            src/PatchageEvent.cpp
            src/PatchageModule.cpp
            src/StateManager.cpp
            src/main.cpp
    '''
    if bld.is_defined('HAVE_JACK_DBUS'):
        prog.source += ' src/JackDbusDriver.cpp '
    if bld.is_defined('PATCHAGE_LIBJACK'):
        prog.source += ' src/JackDriver.cpp '
        prog.uselib += ' JACK NEWJACK '
    if bld.is_defined('HAVE_ALSA'):
        prog.source += ' src/AlsaDriver.cpp '
        prog.uselib += ' ALSA '
    if bld.is_defined('PATCHAGE_BINLOC'):
        prog.lib = ['dl']

    # XML UI definition
    bld(features         = 'subst',
        source           = 'src/patchage.ui',
        target           = out_base + 'patchage.ui',
        install_path     = '${DATADIR}/' + bld.env.APP_INSTALL_NAME,
        chmod            = Utils.O644,
        PATCHAGE_VERSION = PATCHAGE_VERSION)

    # 'Desktop' file (menu entry, icon, etc)
    bld(features         = 'subst',
        source           = 'patchage.desktop.in',
        target           = 'patchage.desktop',
        install_path     = '${DATADIR}/applications',
        chmod            = Utils.O644,
        BINDIR           = os.path.normpath(bld.env.BINDIR),
        APP_INSTALL_NAME = bld.env.APP_INSTALL_NAME,
        APP_HUMAN_NAME   = bld.env.APP_HUMAN_NAME)

    if Options.platform == 'darwin':
        # Property list
        bld(features         = 'subst',
            source           = 'osx/Info.plist.in',
            target           = out_base + 'Info.plist',
            install_path     = '',
            chmod            = Utils.O644)

        # Icons
        bld(rule   = 'cp ${SRC} ${TGT}',
            source = 'osx/Patchage.icns',
            target = out_base + 'Resources/Patchage.icns')

        # Gtk/Pango/etc configuration files
        for i in ['pangorc', 'pango.modules', 'gtkrc', 'fonts.conf']:
            bld(rule   = 'cp ${SRC} ${TGT}',
                source = 'osx/' + i,
                target = out_base + 'Resources/' + i)

    # Icons
    # After installation, icon cache should be updated using:
    # gtk-update-icon-cache -f -t $(datadir)/icons/hicolor
    icon_sizes = [16, 22, 24, 32, 48, 128, 256, 512]
    for s in icon_sizes:
        d = '%dx%d' % (s, s)
        bld.install_as(
                os.path.join(bld.env.DATADIR, 'icons', 'hicolor', d, 'apps',
                                bld.env.APP_INSTALL_NAME + '.png'),
                'icons/' + d + '/patchage.png')

    bld.install_as(
            os.path.join(bld.env.DATADIR, 'icons', 'hicolor', 'scalable', 'apps',
                            bld.env.APP_INSTALL_NAME + '.svg'),
            'icons/scalable/patchage.svg')

    bld.install_files('${MANDIR}/man1', bld.path.ant_glob('doc/*.1'))
