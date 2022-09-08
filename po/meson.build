# Copyright 2020-2022 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR GPL-3.0-or-later

i18n = import('i18n')

add_project_arguments(
  ['-DGETTEXT_PACKAGE="@0@"'.format(meson.project_name())],
  language: 'cpp',
)

i18n.gettext(
  meson.project_name(),
  args: [
    '--add-comments',
    '--check=bullet-unicode',
    '--check=ellipsis-unicode',
    '--check=quote-unicode',
    '--check=space-ellipsis',
    '--copyright-holder=FULL NAME <EMAIL@ADDRESS>',
    '--from-code=UTF-8',
    '--msgid-bugs-address=https://gitlab.com/drobilla/patchage/issues/new',
    '--package-version=@0@'.format(meson.project_version()),
    '--sentence-end=double-space',
    '--sort-by-file',
    '--width=80',
  ],
)
