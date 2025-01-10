# Copyright 2020-2024 David Robillard <d@drobilla.net>
# SPDX-License-Identifier: 0BSD OR GPL-3.0-or-later

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

  # Check code with cppcheck
  cppcheck = find_program('cppcheck', required: false)
  if cppcheck.found()
    compdb_path = join_paths(patchage_build_root, 'compile_commands.json')
    suppress_path = join_paths(patchage_src_root, '.suppress.cppcheck')
    test(
      'cppcheck',
      cppcheck,
      args: [
        '--enable=warning,style,performance,portability',
        '--error-exitcode=1',
        '--project=' + compdb_path,
        '--suppressions-list=' + suppress_path,
        '-DPATCHAGE_DISABLE_FMT_WARNINGS=',
        '-q',
      ],
      suite: 'code',
      timeout: 60,
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
