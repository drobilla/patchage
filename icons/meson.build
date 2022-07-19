# Copyright 2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: CC0-1.0 OR GPL-3.0-or-later

svg_icon_sizes = [
  '16x16',
  '22x22',
  '32x32',
  '48x48',
]

png_icon_sizes = [
  '16x16',
  '22x22',
  '24x24',
  '32x32',
  '48x48',
  '128x128',
  '256x256',
]

icons_dir = get_option('prefix') / get_option('datadir') / 'icons' / 'hicolor'

install_data('scalable/patchage.svg',
             install_dir: icons_dir / 'scalable' / 'apps')

foreach size : svg_icon_sizes
  install_data(files(size / 'patchage.svg'),
               install_dir: icons_dir / size / 'apps')
endforeach

foreach size : png_icon_sizes
  install_data(files(size / 'patchage.png'),
               install_dir: icons_dir / size / 'apps')
endforeach
