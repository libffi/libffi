# Install arch-specific ffi.h file as ffi-$ARCH.h
configure_file(input : 'ffi.h.in',
  output : 'ffi-@0@.h'.format(host_cpu_family),
  configuration : ffi_conf,
  install_dir : 'include')

# Install arch-specific ffitarget.h as ffitarget-$ARCH.h
# XXX: Hack to install the header with a different name
configure_file(
  input : '../src/@0@/ffitarget.h'.format(arch_subdir),
  output : 'ffitarget-@0@.h'.format(host_cpu_family),
  install_dir : 'include',
  copy : true)

# Install noarch ffitarget.h that includes the arch-specific ffitarget.h
ffitarget_h_noarch_conf = configuration_data()
ffitarget_h_noarch_conf.set('ARCH', host_cpu_family)
ffitarget_h_noarch_conf.set('HEADER', 'ffitarget')
configure_file(input : 'ffi_noarch.h.meson', output : 'ffitarget.h',
  configuration : ffitarget_h_noarch_conf,
  install_dir : 'include')

# Install noarch ffi.h that includes the arch-specific ffi.h header
ffi_h_noarch_conf = configuration_data()
ffi_h_noarch_conf.set('ARCH', host_cpu_family)
ffi_h_noarch_conf.set('HEADER', 'ffi')
configure_file(input : 'ffi_noarch.h.meson', output : 'ffi.h',
  configuration : ffi_h_noarch_conf,
  install_dir : 'include')
