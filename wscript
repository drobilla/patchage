#!/usr/bin/env python
# Licensed under the GNU GPL v3 or later, see COPYING file for details.
# Copyright 2008-2013 David Robillard
# Copyright 2008 Nedko Arnaudov

import os

from waflib import Options, Utils
from waflib.extras import autowaf

# Version of this package (even if built as a child)
PATCHAGE_VERSION = '1.0.1'

# Variables for 'waf dist'
APPNAME = 'patchage'
VERSION = PATCHAGE_VERSION

# Mandatory variables
top = '.'
out = 'build'


def options(ctx):
    ctx.load('compiler_cxx')

    opt = ctx.configuration_options()
    opt.add_option('--patchage-install-name', type='string',
                   default=APPNAME,
                   help='patchage install name')
    opt.add_option('--patchage-human-name', type='string',
                   default='Patchage',
                   help='patchage human name')

    ctx.add_flags(
        opt,
        {'jack-dbus':           'use Jack via D-Bus',
         'jack-session-manage': 'include JACK session management support',
         'no-alsa':             'do not build Alsa Sequencer support',
         'no-binloc':           'do not find files from executable location',
         'light-theme':         'use light coloured theme'})


def configure(conf):
    conf.load('compiler_cxx', cache=True)
    conf.load('autowaf', cache=True)
    autowaf.set_cxx_lang(conf, 'c++11')

    conf.check_pkg('dbus-1', uselib_store='DBUS', mandatory=False)
    conf.check_pkg('dbus-glib-1', uselib_store='DBUS_GLIB', mandatory=False)
    conf.check_pkg('gthread-2.0 >= 2.14.0', uselib_store='GTHREAD')
    conf.check_pkg('glibmm-2.4 >= 2.14.0', uselib_store='GLIBMM')
    conf.check_pkg('gtkmm-2.4 >= 2.12.0', uselib_store='GTKMM')
    conf.check_pkg('ganv-1 >= 1.5.2', uselib_store='GANV')

    if conf.env.DEST_OS == 'darwin':
        conf.check_pkg('gtk-mac-integration',
                       uselib_store='GTK_OSX',
                       mandatory=False)
        if conf.env.HAVE_GTK_OSX:
            conf.define('PATCHAGE_GTK_OSX', 1)

    # Check for dladdr
    conf.check_function('cxx', 'dladdr',
                        header_name = 'dlfcn.h',
                        defines     = ['_GNU_SOURCE'],
                        lib         = ['dl'],
                        define_name = 'HAVE_DLADDR',
                        mandatory   = False)

    # Use Jack D-Bus if requested (only one jack driver is allowed)
    use_jack_dbus = (Options.options.jack_dbus and
                     conf.env.HAVE_DBUS and
                     conf.env.HAVE_DBUS_GLIB)

    if use_jack_dbus:
        conf.define('HAVE_JACK_DBUS', 1)
    else:
        conf.check_pkg('jack >= 0.120.0', uselib_store='JACK', mandatory=False)
        if conf.env.HAVE_JACK:
            conf.define('PATCHAGE_LIBJACK', 1)
            if Options.options.jack_session_manage:
                conf.define('PATCHAGE_JACK_SESSION', 1)
                conf.check_function('cxx', 'jack_get_property',
                                    header_name = 'jack/metadata.h',
                                    define_name = 'HAVE_JACK_METADATA',
                                    uselib      = 'JACK',
                                    mandatory   = False)

    # Use Alsa if present unless --no-alsa
    if not Options.options.no_alsa:
        conf.check_pkg('alsa', uselib_store='ALSA', mandatory=False)

    # Find files at binary location if we have dladdr unless --no-binloc
    if not Options.options.no_binloc and conf.is_defined('HAVE_DLADDR'):
        conf.define('PATCHAGE_BINLOC', 1)

    if Options.options.light_theme:
        conf.define('PATCHAGE_USE_LIGHT_THEME', 1)

    # Boost headers
    conf.check_cxx(header_name='boost/format.hpp')
    conf.check_cxx(header_name='boost/shared_ptr.hpp')
    conf.check_cxx(header_name='boost/utility.hpp')
    conf.check_cxx(header_name='boost/weak_ptr.hpp')

    conf.env.PATCHAGE_VERSION = PATCHAGE_VERSION

    conf.env.APP_INSTALL_NAME = Options.options.patchage_install_name
    conf.env.APP_HUMAN_NAME = Options.options.patchage_human_name
    conf.define('PATCHAGE_DATA_DIR', os.path.join(
        conf.env.DATADIR, conf.env.APP_INSTALL_NAME))

    conf.write_config_header('patchage_config.h', remove=False)

    autowaf.display_summary(
        conf,
        {'Install name':            conf.env.APP_INSTALL_NAME,
         'App human name':          conf.env.APP_HUMAN_NAME,
         'Jack (D-Bus)':            conf.is_defined('HAVE_JACK_DBUS'),
         'Jack (libjack)':          conf.is_defined('PATCHAGE_LIBJACK'),
         'Jack Session Management': conf.is_defined('PATCHAGE_JACK_SESSION'),
         'Jack Metadata':           conf.is_defined('HAVE_JACK_METADATA'),
         'Alsa Sequencer':          bool(conf.env.HAVE_ALSA)})

    if conf.env.DEST_OS == 'darwin':
        autowaf.display_msg(conf, "Mac Integration",
                            bool(conf.env.HAVE_GTK_OSX))


def build(bld):
    out_base = ''
    if bld.env.DEST_OS == 'darwin':
        out_base = 'Patchage.app/Contents/'

    # Program
    prog = bld(features     = 'cxx cxxprogram',
               includes     = ['.', 'src'],
               target       = out_base + bld.env.APP_INSTALL_NAME,
               uselib       = 'DBUS GANV DBUS_GLIB GTKMM GTHREAD GTK_OSX',
               install_path = '${BINDIR}')
    prog.source = '''
            src/Configuration.cpp
            src/Patchage.cpp
            src/PatchageCanvas.cpp
            src/PatchageEvent.cpp
            src/PatchageModule.cpp
            src/main.cpp
    '''
    if bld.is_defined('HAVE_JACK_DBUS'):
        prog.source += ' src/JackDbusDriver.cpp '
    if bld.is_defined('PATCHAGE_LIBJACK'):
        prog.source += ' src/JackDriver.cpp '
        prog.uselib += ' JACK NEWJACK '
    if bld.env.HAVE_ALSA:
        prog.source += ' src/AlsaDriver.cpp '
        prog.uselib += ' ALSA '
    if bld.is_defined('PATCHAGE_BINLOC') and bld.is_defined('HAVE_DLADDR'):
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

    if bld.env.DEST_OS == 'darwin':
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
        for i in ['pangorc', 'pango.modules', 'loaders.cache', 'gtkrc']:
            bld(rule   = 'cp ${SRC} ${TGT}',
                source = 'osx/' + i,
                target = out_base + 'Resources/' + i)

    # Icons
    # After installation, icon cache should be updated using:
    # gtk-update-icon-cache -f -t $(datadir)/icons/hicolor
    icon_sizes = [16, 22, 24, 32, 48, 128, 256]
    for s in icon_sizes:
        d = '%dx%d' % (s, s)
        bld.install_as(
            os.path.join(bld.env.DATADIR, 'icons', 'hicolor', d, 'apps',
                         bld.env.APP_INSTALL_NAME + '.png'),
            os.path.join('icons', d, 'patchage.png'))

    bld.install_as(
        os.path.join(bld.env.DATADIR, 'icons', 'hicolor', 'scalable', 'apps',
                     bld.env.APP_INSTALL_NAME + '.svg'),
        os.path.join('icons', 'scalable', 'patchage.svg'))

    bld.install_files('${MANDIR}/man1', bld.path.ant_glob('doc/*.1'))


def posts(ctx):
    path = str(ctx.path.abspath())
    autowaf.news_to_posts(
        os.path.join(path, 'NEWS'),
        {'title':        'Patchage',
         'description':  autowaf.get_blurb(os.path.join(path, 'README')),
         'dist_pattern': 'http://download.drobilla.net/patchage-%s.tar.bz2'},
        {'Author': 'drobilla',
         'Tags':   'Hacking, LAD, Patchage'},
        os.path.join(out, 'posts'))
