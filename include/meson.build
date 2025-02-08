ffi_h_conf = configuration_data()
ffi_h_conf.set('VERSION', meson.project_version())
ffi_h_conf.set('FFI_EXEC_TRAMPOLINE_TABLE', ffi_conf.get('FFI_EXEC_TRAMPOLINE_TABLE', 0))
ffi_h_conf.set('HAVE_LONG_DOUBLE', ffi_conf.get('HAVE_LONG_DOUBLE', 0))
ffi_h_conf.set('TARGET', target)

configure_file(input : 'ffi.h.in',
  output : 'ffi.h',
  configuration : ffi_h_conf,
  install_dir : get_option('includedir'))

install_headers('../src' / targetdir / 'ffitarget.h')
