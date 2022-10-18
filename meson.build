project('libffi', 'c', version : '3.4.4',
        meson_version : '>= 0.56.0',
        default_options : ['debug=true',
                           'optimization=3',
                           'warning_level=1'])

fs = import('fs')

libtool_version = run_command('meson-scripts/extract-libtool-version.py',
  meson.current_source_dir() / 'libtool-version',
  check: true).stdout().strip().split(':')
current = libtool_version[0].to_int()
revision = libtool_version[1].to_int()
age = libtool_version[2].to_int()
ffi_version = '@0@.@1@.@2@'.format(current - age, age, revision)
ffi_soversion = '@0@'.format(current - age)
ffi_darwin_versions = '@0@'.format(current + 1)

# NOTE: host = "cross" or "target"
host_cpu_family = host_machine.cpu_family()
host_system = host_machine.system()

cc = meson.get_compiler('c')
is_msvc = cc.get_id() == 'msvc'
is_msvc_like = cc.get_argument_syntax() == 'msvc'
is_gnu_like = cc.get_argument_syntax() == 'gcc'

if is_gnu_like
  add_project_arguments('-fexceptions', language: 'c')
endif

ffi_conf = configuration_data()
ffi_conf.set_quoted('VERSION', meson.project_version())

size_t = cc.sizeof('size_t')
ffi_conf.set('SIZEOF_SIZE_T', size_t)

if cc.has_header('stdlib.h') and cc.has_header('string.h')
  ffi_conf.set('STDC_HEADERS', 1)
endif

headers = [
  'sys/memfd.h',
  'alloca.h',
  'inttypes.h',
  'stdint.h',
  'stdio.h',
  'stdlib.h',
  'strings.h',
  'string.h',
  'sys/stat.h',
  'sys/types.h',
  'unistd.h',
  'dlfcn.h',
]

foreach h : headers
  if cc.has_header(h)
    define = 'HAVE_' + h.underscorify().to_upper()
    ffi_conf.set(define, 1)
  endif
endforeach

functions = [
  'memfd_create',
  'memcpy',
]

foreach f : functions
  if cc.has_function(f)
    define = 'HAVE_' + f.underscorify().to_upper()
    ffi_conf.set(define, 1)
  endif
endforeach

# Default values
c_sources = ['ffi.c']
asm_sources = ['sysv.S']
masm_args = []
targetdir = ''
have_long_double = ''
have_long_double_variant = false

if host_cpu_family == 'aarch64' and host_system in ['cygwin', 'windows']
  target = 'ARM_WIN64'
  targetdir = 'aarch64'
  if is_msvc
    asm_sources = ['win64_armasm.S']
  endif
elif host_cpu_family == 'aarch64'
  target = 'AARCH64'
elif host_cpu_family == 'alpha'
  target = 'ALPHA'
  asm_sources = ['osf.S']
  # Support 128-bit long double, changeable via command-line switch.
  have_long_double = 'defined(__LONG_DOUBLE_128__)'
elif host_cpu_family == 'arc'
  target = 'ARC'
  asm_sources = ['arcompact.S']
elif host_cpu_family == 'arm' and host_system in ['cygwin', 'windows']
  target = 'ARM_WIN32'
  targetdir = 'arm'
  if is_msvc
    asm_sources = ['sysv_msvc_arm32.S']
  endif
elif host_cpu_family == 'arm'
  target = 'ARM'
elif host_cpu_family == 'avr'
  target = 'AVR32'
elif host_cpu_family == 'bfin'
  # FIXME: bfin family is missing in Meson
  target = 'BFIN'
elif host_cpu_family == 'cris'
  # FIXME: cris family is missing in Meson
  target = 'LIBFFI_CRIS'
  targetdir = 'cris'
elif host_cpu_family == 'csky'
  target = 'CSKY'
elif host_cpu_family == 'frv'
  # FIXME: frv family is missing in Meson
  target = 'FRV'
  asm_sources = ['eabi.S']
elif (host_cpu_family in ['hppa', 'hppa64'] and host_system in ['linux', 'openbsd']) or \
     (host_cpu_family == 'parisc' and host_system == 'linux')
  # FIXME: hppa family is missing in Meson
  target = 'PA_LINUX'
  targetdir = 'pa'
  asm_sources = ['linux.S']
elif host_cpu_family == 'hppa' and host_system in ['hpux']
  # FIXME: hppa family is missing in Meson
  target = 'PA_HPUX'
  targetdir = 'pa'
  asm_sources = ['hpux32.S']
elif host_cpu_family == 'x86' and host_system in ['freebsd', 'openbsd']
  target = 'X86_FREEBSD'
  targetdir = 'x86'
  if is_msvc
    asm_sources = ['sysv_intel.S']
  endif
elif (host_cpu_family == 'x86' and host_system in ['cygwin', 'windows', 'os2', 'interix']) or \
     (host_cpu_family == 'x86_64' and host_system in ['cygwin', 'windows'])
  targetdir = 'x86'
  if size_t == 4
    target = 'X86_WIN32'
    if is_msvc
      asm_sources = ['sysv_intel.S']
      masm_args += '/safeseh'
    endif
  else
    target = 'X86_WIN64'
    c_sources = ['ffiw64.c']
    if is_msvc
      asm_sources = ['win64_intel.S']
    else
      asm_sources = ['win64.S']
    endif
  endif
elif host_cpu_family in ['x86', 'x86_64'] and host_system in ['darwin', 'ios']
  targetdir = 'x86'
  if size_t == 4
    target = 'X86_DARWIN'
    if is_msvc
      asm_sources = ['sysv_intel.S']
    endif
  else
    target = 'X86_64'
    c_sources = ['ffi64.c', 'ffiw64.c']
    asm_sources = ['unix64.S', 'win64.S']
  endif
elif host_cpu_family in ['x86', 'x86_64']
  targetdir = 'x86'
  if size_t == 4
    if cc.compiles('int foo (void) { return __x86_64__; }', name: 'X32')
      target = 'X86_64'
      c_sources = ['ffi64.c', 'ffiw64.c']
      asm_sources = ['unix64.S', 'win64.S']
    else
      target = 'X86'
      if is_msvc
        asm_sources = ['sysv_intel.S']
      endif
    endif
  else
    target = 'X86_64'
    c_sources = ['ffi64.c', 'ffiw64.c']
    asm_sources = ['unix64.S', 'win64.S']
  endif
elif host_cpu_family == 'ia64'
  target = 'IA64'
  asm_sources = ['unix.S']
elif host_cpu_family == 'kvx'
  # FIXME: kvx family is missing in Meson
  target = 'KVX'
elif host_cpu_family == 'loongarch64'
  target = 'LOONGARCH64'
elif host_cpu_family.startswith('m32r')
  target = 'M32R'
elif host_cpu_family == 'm68k'
  target = 'M68K'
elif host_cpu_family == 'm88k'
  # FIXME: m88k family is missing in Meson
  target = 'M88K'
  asm_sources = ['obsd.S']
elif host_cpu_family == 'microblaze'
  target = 'MICROBLAZE'
elif host_cpu_family == 'moxie'
  # This CPU family is used in CI cross compilation
  target = 'MOXIE'
  asm_sources = ['eabi.S']
elif host_cpu_family == 'metag'
  # FIXME: metag family is missing in Meson
  target = 'METAG'
elif host_cpu_family in ['mips', 'mips64'] and \
     (host_system.startswith('irix5.') or host_system.startswith('irix6.') or host_system.startswith('rtems'))
  target = 'MIPS'
  asm_sources = ['o32.S', 'n32.S']
elif host_cpu_family in ['mips', 'mips64'] and host_system in ['linux', 'openbsd', 'freebsd']
  target = 'MIPS'
  asm_sources = ['o32.S', 'n32.S']
  # Support 128-bit long double for NewABI.
  have_long_double = 'defined(__mips64)'
elif host_cpu_family == 'nios2' and host_system == 'linux'
  # FIXME: nios2 family is missing in Meson
  target = 'NIOS2'
elif host_cpu_family == 'or1k'
  # FIXME: or1k family is missing in Meson
  target = 'OR1K'
elif (host_cpu_family in ['ppc', 'ppc64'] and \
      host_system in ['linux', 'sysv', 'amigaos', 'eabi', 'beos', 'rtems']) or \
     (host_cpu_family == 'ppc64' and host_system == 'freebsd')
  target = 'POWERPC'
  c_sources = ['ffi.c', 'ffi_sysv.c', 'ffi_linux64.c']
  asm_sources = ['sysv.S', 'ppc_closure.S', 'linux64.S', 'linux64_closure.S']
  have_long_double_variant = true
elif host_cpu_family in ['ppc', 'ppc64'] and host_system == 'darwin'
  target = 'POWERPC_DARWIN'
  targetdir = 'powerpc'
  c_sources = ['ffi_darwin.c']
  asm_sources = ['darwin.S', 'darwin_closure.S']
elif host_cpu_family in ['ppc', 'rs6000'] and host_system in ['darwin', 'aix']
  target = 'POWERPC_AIX'
  targetdir = 'powerpc'
  c_sources = ['ffi_darwin.c']
  asm_sources = ['aix.S aix_closure.S']
elif host_cpu_family == 'ppc' and host_system in ['freebsd', 'openbsd', 'netbsd']
  target = 'POWERPC_FREEBSD'
  targetdir = 'powerpc'
  c_sources = ['ffi.c', 'ffi_sysv.c']
  asm_sources = ['sysv.S', ' pc_closure.S']
  if cc.get_define('__SPE__') != ''
    add_project_arguments('-D__NO_FPRS__', language: 'c')
  else
    have_long_double_variant = true
  endif
elif host_cpu_family in ['riscv32', 'riscv64']
  target = 'RISCV'
elif host_cpu_family in ['s390', 's390x']
  target = 'S390'
elif host_cpu_family == 'sh64' or host_cpu_family.startswith('sh5')
  # FIXME: Meson only has sh4 family
  target = 'SH64'
elif host_cpu_family.startswith('sh')
  # FIXME: Meson only has sh4 family
  target = 'SH'
elif host_cpu_family in ['sparc', 'sparc64']
  target = 'SPARC'
  c_sources = ['ffi.c', 'ffi64.c']
  asm_sources = ['v8.S', 'v9.S']
elif host_cpu_family == 'tile'
  # FIXME: tile family is missing in Meson
  target = 'TILE'
  asm_sources = ['tile.S']
elif host_cpu_family == 'vax'
  # FIXME: vax family is missing in Meson
  target = 'VAX'
  asm_sources = ['elfbsd.S']
elif host_cpu_family == 'xtensa'
  # FIXME: xtensa family is missing in Meson
  target = 'XTENSA'
else
  error('Unsupported pair: system "@0@", cpu family "@1@"'.format(host_system, host_cpu_family))
endif

assert(target != '')
if targetdir == ''
  targetdir = target.to_lower()
endif

size_long_double = cc.sizeof('long double')
size_double = cc.sizeof('double')
ffi_conf.set('SIZEOF_LONG_DOUBLE', size_long_double)
ffi_conf.set('SIZEOF_DOUBLE', size_double)
if have_long_double == ''
  if size_long_double > 0
    if have_long_double_variant
      ffi_conf.set('HAVE_LONG_DOUBLE_VARIANT', 1)
      ffi_conf.set('HAVE_LONG_DOUBLE', 1)
    elif size_long_double != size_double
      ffi_conf.set('HAVE_LONG_DOUBLE', 1)
    endif
  endif
else
  ffi_conf.set('HAVE_LONG_DOUBLE', have_long_double)
endif

if host_machine.endian() == 'big'
  ffi_conf.set('WORDS_BIGENDIAN', 1)
endif

# Assembly directive support
if cc.compiles('asm (".cfi_sections\\n.cfi_startproc\\n.cfi_endproc");', name : 'ASM .cfi')
  ffi_conf.set('HAVE_AS_CFI_PSEUDO_OP', 1)
endif

if target == 'SPARC'
  if cc.links('asm (".text; foo: nop; .data; .align 4; .byte 0; .uaword %r_disp32(foo); .text");',
              args: ['-fpic', '-shared'],
              name : 'ASM SPARC UA PCREL')
    ffi_conf.set('HAVE_AS_SPARC_UA_PCREL', 1)
  endif
  if cc.compiles('asm (".register %g2, #scratch");', name : 'ASM .register')
    ffi_conf.set('HAVE_AS_REGISTER_PSEUDO_OP', 1)
  endif
elif target.startswith('X86')
  if cc.compiles('asm (".text; foo: nop; .data; .long foo-.; .text");', name : 'ASM x86 PCREL')
    ffi_conf.set('HAVE_AS_X86_PCREL', 1)
  endif
elif target == 'S390'
  t = 'meson-scripts/test-cc-uses-zarch.py'
  if run_command(t, cc.cmd_array(), check: false).returncode() == 0
    ffi_conf.set('HAVE_AS_S390_ZARCH', 1)
  endif
endif

ptrauth_code = '''
#ifdef __clang__
# if __has_feature(ptrauth_calls)
#  define HAVE_PTRAUTH 1
# endif
#endif

#ifndef HAVE_PTRAUTH
# error Pointer authentication not supported
#endif
'''

if cc.compiles(ptrauth_code)
  ffi_conf.set('HAVE_PTRAUTH', 1)
endif

if get_option('pax_emutramp')
  ffi_conf.set('FFI_MMAP_EXEC_EMUTRAMP_PAX', 1)
endif

if cc.symbols_have_underscore_prefix()
  ffi_conf.set('SYMBOL_UNDERSCORE', 1)
endif

if host_cpu_family in ['arm', 'aarch64'] and host_system == 'darwin'
  message('Cannot use PROT_EXEC on this target, using fallback')
  ffi_conf.set('FFI_EXEC_TRAMPOLINE_TABLE', 1)
elif host_system in ['android', 'darwin', 'openbsd', 'freebsd', 'solaris', 'qnx']
  message('Cannot use malloc on this target, using fallback')
  ffi_conf.set('FFI_MMAP_EXEC_WRIT', 1)
endif

if target == 'X86_64'
  t = 'meson-scripts/test-unwind-section.py'
  if run_command(t, cc.cmd_array(), check: false).returncode() == 0
    ffi_conf.set('HAVE_AS_X86_64_UNWIND_SECTION_TYPE', 1)
  endif
endif

if host_system == 'windows' and is_gnu_like
  # so printf supports long double under mingw-w64 (used in cls_longdouble_va)
  add_project_arguments('-D__USE_MINGW_ANSI_STDIO=1', language: 'c')
endif

if is_gnu_like
  no_lto = cc.get_supported_arguments('-fno-lto')
  t = 'meson-scripts/test-ro-eh-frame.py'
  if run_command(t, cc.cmd_array(), '-fpic', '-fexceptions', no_lto, check: false).returncode() == 0
    ffi_conf.set('EH_FRAME_FLAGS', 'aw')
  else
    ffi_conf.set('HAVE_RO_EH_FRAME', 1)
    ffi_conf.set_quoted('EH_FRAME_FLAGS', 'a')
  endif
endif

if cc.has_function_attribute('visibility')
  t = 'meson-scripts/test-cc-supports-hidden-visibility.py'
  if run_command(t, cc.cmd_array(), check: false).returncode() == 0
    message('.hidden pseudo-op is available')
    ffi_conf.set('HAVE_HIDDEN_VISIBILITY_ATTRIBUTE', 1)
  else
    message('.hidden pseudo-op is NOT available')
  endif
endif

# User options
if get_option('ffi-debug')
  ffi_conf.set('FFI_DEBUG', 1)
endif
if not get_option('structs')
  ffi_conf.set('FFI_NO_STRUCTS', 1)
endif
if not get_option('raw_api')
  ffi_conf.set('FFI_NO_RAW_API', 1)
endif
if get_option('exe_static_tramp')
  if host_system == 'cygwin'
    # Only define static trampolines if we are using the cygwin runtime.
    # Will this need to be changed for mingw?
    if is_gnu_like
      ffi_conf.set('FFI_EXEC_STATIC_TRAMP', 1)
    endif
  elif host_system == 'linux' and host_cpu_family in ['arm', 'aarch64', 'x86', 'x86_64', 'loongarch']
    ffi_conf.set('FFI_EXEC_STATIC_TRAMP', 1)
  endif
endif
if get_option('purify_safety')
  ffi_conf.set('USING_PURIFY', 1)
endif

# Configure fficonfig.h (not installed)
configure_file(input : 'fficonfig.h.meson', output : 'fficonfig.h',
  configuration : ffi_conf)

# Configure and install headers
subdir('include')

# Configure ffi_conf some more and declare libffi.so
subdir('src')

if get_option('doc')
  subdir('doc')
endif

if get_option('tests')
  if meson.version().version_compare('>=0.58.0')
    subdir('testsuite')
  else
    error('Meson >= 0.58 is required for unit tests')
  endif
endif

install_man([
  'man/ffi.3',
  'man/ffi_call.3',
  'man/ffi_prep_cif.3',
  'man/ffi_prep_cif_var.3'])

summary({
  'Host CPU': host_machine.cpu(),
  'Host CPU family': host_cpu_family,
  'Host system': host_system,
  'Target': target,
  'ASM sources': asm_sources,
  'C sources': c_sources,
})
