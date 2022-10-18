custom_target(
  input: 'libffi.texi',
  output: 'libffi.info',
  depend_files: 'version.texi',
  command: [find_program('makeinfo'),
    '-I', meson.current_source_dir(),
    '-o', '@OUTPUT@',
    '@INPUT@'
  ],
  build_by_default: true,
  install: true,
  install_dir: get_option('datadir') / 'info',
)
