cmake_minimum_required(VERSION 3.20)

project(libffi VERSION 3.8.0 LANGUAGES C)

include(CheckCSourceCompiles)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckTypeSize)
include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(TestBigEndian)

option(BUILD_SHARED_LIBS "Build libffi as a shared library" ON)
option(LIBFFI_BUILD_TESTING "Build the CMake smoke tests" ON)
option(LIBFFI_ENABLE_DEBUG "Enable libffi's extra run-time checks" OFF)
option(LIBFFI_ENABLE_RAW_API "Build the raw API" ON)
option(LIBFFI_ENABLE_STRUCTS "Build aggregate type support" ON)
option(LIBFFI_ENABLE_PURIFY_SAFETY "Enable Purify-compatible allocation behavior" OFF)
option(LIBFFI_ENABLE_EXEC_STATIC_TRAMP "Enable supported static trampoline backends" ON)

# Match configure.ac.  Keep these values next to project() so release version
# updates cannot make the public header drift from the CMake package version.
set(FFI_VERSION_STRING "${PROJECT_VERSION}")
math(EXPR FFI_VERSION_NUMBER
  "${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100 + ${PROJECT_VERSION_PATCH}")

set(_ffi_common_sources
  src/prep_cif.c
  src/types.c
  src/raw_api.c
  src/java_raw_api.c
  src/closures.c
  src/tramp.c)
if(LIBFFI_ENABLE_DEBUG)
  list(APPEND _ffi_common_sources src/debug.c)
endif()

# Select the same target implementation as configure.host.  CMake's target
# processor is used (rather than the build host), so toolchain-file cross builds
# select the correct backend as well.
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _ffi_processor)
set(_ffi_arch_sources)
set(_ffi_asm_sources)
set(HAVE_LONG_DOUBLE_VARIANT 0)

if(WIN32 AND MSVC)
  if(_ffi_processor MATCHES "^(amd64|x86_64|x64)$" OR
     (CMAKE_SIZEOF_VOID_P EQUAL 8 AND _ffi_processor MATCHES "x86"))
    set(TARGET X86_WIN64)
    set(TARGETDIR x86)
    set(_ffi_arch_sources src/x86/ffiw64.c)
    set(_ffi_asm_sources src/x86/win64_intel.S)
    set(_ffi_msvc_assembler ml64)
  elseif(_ffi_processor MATCHES "^(i[3-6]86|x86|win32)$")
    set(TARGET X86_WIN32)
    set(TARGETDIR x86)
    set(_ffi_arch_sources src/x86/ffi.c)
    set(_ffi_asm_sources src/x86/sysv_intel.S)
    set(_ffi_msvc_assembler ml)
  elseif(_ffi_processor MATCHES "^(arm64|aarch64)$")
    set(TARGET ARM_WIN64)
    set(TARGETDIR aarch64)
    set(_ffi_arch_sources src/aarch64/ffi.c)
    set(_ffi_asm_sources src/aarch64/win64_armasm.S)
    set(_ffi_msvc_assembler armasm64)
  elseif(_ffi_processor MATCHES "^arm")
    set(TARGET ARM_WIN32)
    set(TARGETDIR arm)
    set(_ffi_arch_sources src/arm/ffi.c)
    set(_ffi_asm_sources src/arm/sysv_msvc_arm32.S)
    set(_ffi_msvc_assembler armasm)
  else()
    message(FATAL_ERROR "Unsupported MSVC target processor: ${CMAKE_SYSTEM_PROCESSOR}")
  endif()
elseif(WIN32 AND _ffi_processor MATCHES "^(amd64|x86_64|x64)$")
  set(TARGET X86_WIN64)
  set(TARGETDIR x86)
  set(_ffi_arch_sources src/x86/ffiw64.c)
  set(_ffi_asm_sources src/x86/win64.S)
elseif(WIN32 AND _ffi_processor MATCHES "^(i[3-6]86|x86|win32)$")
  set(TARGET X86_WIN32)
  set(TARGETDIR x86)
  set(_ffi_arch_sources src/x86/ffi.c)
  set(_ffi_asm_sources src/x86/sysv.S)
elseif(WIN32 AND _ffi_processor MATCHES "^(arm64|aarch64)$")
  set(TARGET ARM_WIN64)
  set(TARGETDIR aarch64)
  set(_ffi_arch_sources src/aarch64/ffi.c)
  set(_ffi_asm_sources src/aarch64/sysv.S)
elseif(WIN32 AND _ffi_processor MATCHES "^arm")
  set(TARGET ARM_WIN32)
  set(TARGETDIR arm)
  set(_ffi_arch_sources src/arm/ffi.c)
  set(_ffi_asm_sources src/arm/sysv.S)
elseif(_ffi_processor MATCHES "^(amd64|x86_64|x64)$")
  set(TARGET X86_64)
  set(TARGETDIR x86)
  set(_ffi_arch_sources src/x86/ffi64.c src/x86/ffiw64.c)
  set(_ffi_asm_sources src/x86/unix64.S src/x86/win64.S)
elseif(_ffi_processor MATCHES "^(i[3-6]86|x86)$")
  if(APPLE)
    set(TARGET X86_DARWIN)
  elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD|OpenBSD")
    set(TARGET X86_FREEBSD)
  else()
    set(TARGET X86)
  endif()
  set(TARGETDIR x86)
  set(_ffi_arch_sources src/x86/ffi.c)
  set(_ffi_asm_sources src/x86/sysv.S)
elseif(_ffi_processor MATCHES "^(arm64|aarch64|arm64e)$")
  set(TARGET AARCH64)
  set(TARGETDIR aarch64)
  set(_ffi_arch_sources src/aarch64/ffi.c)
  set(_ffi_asm_sources src/aarch64/sysv.S)
elseif(_ffi_processor MATCHES "^arm")
  set(TARGET ARM)
  set(TARGETDIR arm)
  set(_ffi_arch_sources src/arm/ffi.c)
  set(_ffi_asm_sources src/arm/sysv.S)
elseif(_ffi_processor MATCHES "^alpha")
  set(TARGET ALPHA)
  set(TARGETDIR alpha)
  set(_ffi_arch_sources src/alpha/ffi.c)
  set(_ffi_asm_sources src/alpha/osf.S)
elseif(_ffi_processor MATCHES "^arc")
  set(TARGET ARC)
  set(TARGETDIR arc)
  set(_ffi_arch_sources src/arc/ffi.c)
  set(_ffi_asm_sources src/arc/arcompact.S)
elseif(_ffi_processor MATCHES "^avr32")
  set(TARGET AVR32)
  set(TARGETDIR avr32)
  set(_ffi_arch_sources src/avr32/ffi.c)
  set(_ffi_asm_sources src/avr32/sysv.S)
elseif(_ffi_processor MATCHES "^bfin")
  set(TARGET BFIN)
  set(TARGETDIR bfin)
  set(_ffi_arch_sources src/bfin/ffi.c)
  set(_ffi_asm_sources src/bfin/sysv.S)
elseif(_ffi_processor MATCHES "^cris")
  set(TARGET LIBFFI_CRIS)
  set(TARGETDIR cris)
  set(_ffi_arch_sources src/cris/ffi.c)
  set(_ffi_asm_sources src/cris/sysv.S)
elseif(_ffi_processor MATCHES "^csky")
  set(TARGET CSKY)
  set(TARGETDIR csky)
  set(_ffi_arch_sources src/csky/ffi.c)
  set(_ffi_asm_sources src/csky/sysv.S)
elseif(_ffi_processor MATCHES "^frv")
  set(TARGET FRV)
  set(TARGETDIR frv)
  set(_ffi_arch_sources src/frv/ffi.c)
  set(_ffi_asm_sources src/frv/eabi.S)
elseif(_ffi_processor MATCHES "^(hppa|parisc)")
  set(TARGETDIR pa)
  if(CMAKE_SYSTEM_NAME STREQUAL "HP-UX" AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TARGET PA64_HPUX)
    set(_ffi_arch_sources src/pa/ffi64.c)
    set(_ffi_asm_sources src/pa/hpux64.S)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "HP-UX")
    set(TARGET PA_HPUX)
    set(_ffi_arch_sources src/pa/ffi.c)
    set(_ffi_asm_sources src/pa/hpux32.S)
  else()
    set(TARGET PA_LINUX)
    set(_ffi_arch_sources src/pa/ffi.c)
    set(_ffi_asm_sources src/pa/linux.S)
  endif()
elseif(_ffi_processor MATCHES "^ia64")
  set(TARGET IA64)
  set(TARGETDIR ia64)
  set(_ffi_arch_sources src/ia64/ffi.c)
  set(_ffi_asm_sources src/ia64/unix.S)
elseif(_ffi_processor MATCHES "^kvx")
  set(TARGET KVX)
  set(TARGETDIR kvx)
  set(_ffi_arch_sources src/kvx/ffi.c)
  set(_ffi_asm_sources src/kvx/sysv.S)
elseif(_ffi_processor MATCHES "^riscv")
  set(TARGET RISCV)
  set(TARGETDIR riscv)
  set(_ffi_arch_sources src/riscv/ffi.c)
  set(_ffi_asm_sources src/riscv/sysv.S)
elseif(_ffi_processor MATCHES "^s390")
  set(TARGET S390)
  set(TARGETDIR s390)
  set(_ffi_arch_sources src/s390/ffi.c)
  set(_ffi_asm_sources src/s390/sysv.S)
elseif(_ffi_processor MATCHES "^loongarch")
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TARGET LOONGARCH64)
  else()
    set(TARGET LOONGARCH32)
  endif()
  set(TARGETDIR loongarch)
  set(_ffi_arch_sources src/loongarch/ffi.c)
  set(_ffi_asm_sources src/loongarch/sysv.S)
elseif(_ffi_processor MATCHES "^powerpc|^ppc")
  set(TARGETDIR powerpc)
  if(APPLE)
    set(TARGET POWERPC_DARWIN)
    set(_ffi_arch_sources src/powerpc/ffi_darwin.c)
    set(_ffi_asm_sources src/powerpc/darwin.S src/powerpc/darwin_closure.S)
  elseif(CMAKE_SYSTEM_NAME STREQUAL "AIX")
    set(TARGET POWERPC_AIX)
    set(_ffi_arch_sources src/powerpc/ffi_darwin.c)
    set(_ffi_asm_sources src/powerpc/aix.S src/powerpc/aix_closure.S)
  elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD|OpenBSD|NetBSD")
    set(TARGET POWERPC_FREEBSD)
    set(HAVE_LONG_DOUBLE_VARIANT 1)
    set(_ffi_arch_sources src/powerpc/ffi.c src/powerpc/ffi_sysv.c)
    set(_ffi_asm_sources src/powerpc/sysv.S src/powerpc/ppc_closure.S)
  else()
    set(TARGET POWERPC)
    set(HAVE_LONG_DOUBLE_VARIANT 1)
    set(_ffi_arch_sources
      src/powerpc/ffi.c src/powerpc/ffi_sysv.c src/powerpc/ffi_linux64.c)
    set(_ffi_asm_sources
      src/powerpc/sysv.S src/powerpc/ppc_closure.S
      src/powerpc/linux64.S src/powerpc/linux64_closure.S)
  endif()
elseif(_ffi_processor MATCHES "^mips")
  set(TARGET MIPS)
  set(TARGETDIR mips)
  set(_ffi_arch_sources src/mips/ffi.c)
  set(_ffi_asm_sources src/mips/o32.S src/mips/n32.S)
elseif(_ffi_processor MATCHES "^sparc")
  set(TARGET SPARC)
  set(TARGETDIR sparc)
  set(_ffi_arch_sources src/sparc/ffi.c src/sparc/ffi64.c)
  set(_ffi_asm_sources src/sparc/v8.S src/sparc/v9.S)
elseif(_ffi_processor MATCHES "^m32r")
  set(TARGET M32R)
  set(TARGETDIR m32r)
  set(_ffi_arch_sources src/m32r/ffi.c)
  set(_ffi_asm_sources src/m32r/sysv.S)
elseif(_ffi_processor MATCHES "^m68k")
  set(TARGET M68K)
  set(TARGETDIR m68k)
  set(_ffi_arch_sources src/m68k/ffi.c)
  set(_ffi_asm_sources src/m68k/sysv.S)
elseif(_ffi_processor MATCHES "^m88k")
  set(TARGET M88K)
  set(TARGETDIR m88k)
  set(_ffi_arch_sources src/m88k/ffi.c)
  set(_ffi_asm_sources src/m88k/obsd.S)
elseif(_ffi_processor MATCHES "^metag")
  set(TARGET METAG)
  set(TARGETDIR metag)
  set(_ffi_arch_sources src/metag/ffi.c)
  set(_ffi_asm_sources src/metag/sysv.S)
elseif(_ffi_processor MATCHES "^microblaze")
  set(TARGET MICROBLAZE)
  set(TARGETDIR microblaze)
  set(_ffi_arch_sources src/microblaze/ffi.c)
  set(_ffi_asm_sources src/microblaze/sysv.S)
elseif(_ffi_processor MATCHES "^moxie")
  set(TARGET MOXIE)
  set(TARGETDIR moxie)
  set(_ffi_arch_sources src/moxie/ffi.c)
  set(_ffi_asm_sources src/moxie/eabi.S)
elseif(_ffi_processor MATCHES "^or1k")
  set(TARGET OR1K)
  set(TARGETDIR or1k)
  set(_ffi_arch_sources src/or1k/ffi.c)
  set(_ffi_asm_sources src/or1k/sysv.S)
elseif(_ffi_processor MATCHES "^(sh64|sh5)")
  set(TARGET SH64)
  set(TARGETDIR sh64)
  set(_ffi_arch_sources src/sh64/ffi.c)
  set(_ffi_asm_sources src/sh64/sysv.S)
elseif(_ffi_processor MATCHES "^sh")
  set(TARGET SH)
  set(TARGETDIR sh)
  set(_ffi_arch_sources src/sh/ffi.c)
  set(_ffi_asm_sources src/sh/sysv.S)
elseif(_ffi_processor MATCHES "^tile")
  set(TARGET TILE)
  set(TARGETDIR tile)
  set(_ffi_arch_sources src/tile/ffi.c)
  set(_ffi_asm_sources src/tile/tile.S)
elseif(_ffi_processor MATCHES "^vax")
  set(TARGET VAX)
  set(TARGETDIR vax)
  set(_ffi_arch_sources src/vax/ffi.c)
  set(_ffi_asm_sources src/vax/elfbsd.S)
elseif(_ffi_processor MATCHES "^wasm32")
  set(TARGET wasm32)
  set(TARGETDIR wasm)
  set(_ffi_arch_sources src/wasm/ffi.c)
elseif(_ffi_processor MATCHES "^wasm64")
  set(TARGET wasm64)
  set(TARGETDIR wasm)
  set(_ffi_arch_sources src/wasm/ffi.c)
elseif(_ffi_processor MATCHES "^xtensa")
  set(TARGET XTENSA)
  set(TARGETDIR xtensa)
  set(_ffi_arch_sources src/xtensa/ffi.c)
  set(_ffi_asm_sources src/xtensa/sysv.S)
else()
  message(FATAL_ERROR
    "Unsupported target processor '${CMAKE_SYSTEM_PROCESSOR}'. "
    "Add its configure.host source mapping to CMakeLists.txt.")
endif()

check_include_file(alloca.h HAVE_ALLOCA_H)
check_include_file(sys/memfd.h HAVE_SYS_MEMFD_H)
check_function_exists(memcpy HAVE_MEMCPY)
check_function_exists(memfd_create HAVE_MEMFD_CREATE)
check_function_exists(mkostemp HAVE_MKOSTEMP)
check_c_source_compiles(
  "int main(void) { __int128_t value = 0; return (int)value; }"
  HAVE_INT128)
check_c_source_compiles(
  "int __attribute__((visibility(\"hidden\"))) f(void) { return 0; }
   int main(void) { return f(); }"
  HAVE_HIDDEN_VISIBILITY_ATTRIBUTE)

if(NOT MSVC)
  check_c_source_compiles(
    "__asm__(\".text\\n.cfi_startproc\\nnop\\n.cfi_endproc\\n\");
     int main(void) { return 0; }"
    HAVE_AS_CFI_PSEUDO_OP)
  if("${TARGET}" MATCHES "^X86")
    check_c_source_compiles(
      "__asm__(\".text\\n.Lffi_pcrel_test: nop\\n.data\\n.long .Lffi_pcrel_test-.\\n.text\\n\");
       int main(void) { return 0; }"
      HAVE_AS_X86_PCREL)
  endif()
endif()

test_big_endian(WORDS_BIGENDIAN)
check_type_size("size_t" SIZEOF_SIZE_T LANGUAGE C)
check_type_size("double" SIZEOF_DOUBLE LANGUAGE C)
check_type_size("long double" SIZEOF_LONG_DOUBLE LANGUAGE C)
if(SIZEOF_LONG_DOUBLE GREATER SIZEOF_DOUBLE)
  set(HAVE_LONG_DOUBLE 1)
else()
  set(HAVE_LONG_DOUBLE 0)
endif()

set(FFI_EXEC_TRAMPOLINE_TABLE 0)
if(APPLE AND (_ffi_processor MATCHES "arm|aarch64"))
  set(FFI_EXEC_TRAMPOLINE_TABLE 1)
endif()
set(FFI_MMAP_EXEC_WRIT 0)
if(APPLE OR CMAKE_SYSTEM_NAME MATCHES "DragonFly|FreeBSD|OpenBSD|SunOS" OR ANDROID)
  set(FFI_MMAP_EXEC_WRIT 1)
endif()

set(FFI_DEBUG ${LIBFFI_ENABLE_DEBUG})
if(LIBFFI_ENABLE_RAW_API)
  set(FFI_NO_RAW_API 0)
else()
  set(FFI_NO_RAW_API 1)
endif()
if(LIBFFI_ENABLE_STRUCTS)
  set(FFI_NO_STRUCTS 0)
else()
  set(FFI_NO_STRUCTS 1)
endif()
set(USING_PURIFY ${LIBFFI_ENABLE_PURIFY_SAFETY})
set(FFI_EXEC_STATIC_TRAMP 0)
if(LIBFFI_ENABLE_EXEC_STATIC_TRAMP AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
   "${TARGET}" MATCHES "^(AARCH64|ARM|X86|X86_64|LOONGARCH|S390|POWERPC|RISCV)")
  set(FFI_EXEC_STATIC_TRAMP 1)
endif()

set(PACKAGE_NAME "libffi")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE_STRING "libffi ${PROJECT_VERSION}")
set(PACKAGE_BUGREPORT "https://github.com/libffi/libffi/issues")
set(PACKAGE_URL "https://sourceware.org/libffi/")
set(EH_FRAME_FLAGS "a")
set(STDC_HEADERS 1)

file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include")
configure_file(include/ffi.h.in "${CMAKE_CURRENT_BINARY_DIR}/include/ffi.h" @ONLY)
configure_file(cmake/fficonfig.h.in "${CMAKE_CURRENT_BINARY_DIR}/include/fficonfig.h")
configure_file(src/${TARGETDIR}/ffitarget.h
  "${CMAKE_CURRENT_BINARY_DIR}/include/ffitarget.h" COPYONLY)

set(_ffi_generated_asm_objects)
if(MSVC AND _ffi_asm_sources)
  get_filename_component(_ffi_compiler_dir "${CMAKE_C_COMPILER}" DIRECTORY)
  find_program(_ffi_assembler_program NAMES "${_ffi_msvc_assembler}"
    HINTS "${_ffi_compiler_dir}" NO_DEFAULT_PATH)
  if(NOT _ffi_assembler_program)
    find_program(_ffi_assembler_program NAMES "${_ffi_msvc_assembler}")
  endif()
  if(NOT _ffi_assembler_program)
    message(FATAL_ERROR
      "${_ffi_msvc_assembler} was not found. Run CMake from the matching "
      "Visual Studio developer command prompt.")
  endif()

  file(GLOB _ffi_asm_headers CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/${TARGETDIR}/*.h")
  foreach(_ffi_asm_source IN LISTS _ffi_asm_sources)
    get_filename_component(_ffi_asm_name "${_ffi_asm_source}" NAME_WE)
    set(_ffi_preprocessed "${CMAKE_CURRENT_BINARY_DIR}/asm/${_ffi_asm_name}.asm")
    set(_ffi_object "${CMAKE_CURRENT_BINARY_DIR}/asm/${_ffi_asm_name}.obj")
    add_custom_command(
      OUTPUT "${_ffi_object}"
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/asm"
      COMMAND "${CMAKE_C_COMPILER}" /nologo /P /EP
        "/Fi${_ffi_preprocessed}"
        "/I${CMAKE_CURRENT_BINARY_DIR}/include"
        "/I${CMAKE_CURRENT_SOURCE_DIR}/include"
        "/I${CMAKE_CURRENT_SOURCE_DIR}/src"
        "/I${CMAKE_CURRENT_SOURCE_DIR}/src/${TARGETDIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/${_ffi_asm_source}"
      COMMAND "${_ffi_assembler_program}" /nologo /c
        "/Fo${_ffi_object}" "${_ffi_preprocessed}"
      DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${_ffi_asm_source}"
        "${CMAKE_CURRENT_BINARY_DIR}/include/ffi.h"
        "${CMAKE_CURRENT_BINARY_DIR}/include/fficonfig.h"
        "${CMAKE_CURRENT_BINARY_DIR}/include/ffitarget.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/include/ffi_cfi.h"
        ${_ffi_asm_headers}
      VERBATIM
      COMMENT "Preprocessing and assembling ${_ffi_asm_source}")
    set_source_files_properties("${_ffi_object}"
      PROPERTIES GENERATED TRUE EXTERNAL_OBJECT TRUE)
    list(APPEND _ffi_generated_asm_objects "${_ffi_object}")
  endforeach()
else()
  if(_ffi_asm_sources)
    enable_language(ASM)
  endif()
endif()

add_library(ffi
  ${_ffi_common_sources}
  ${_ffi_arch_sources}
  ${_ffi_asm_sources}
  ${_ffi_generated_asm_objects})
add_library(libffi::ffi ALIAS ffi)

target_compile_features(ffi PUBLIC c_std_99)
target_include_directories(ffi
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/${TARGETDIR}")
set_target_properties(ffi PROPERTIES
  C_VISIBILITY_PRESET hidden
  EXPORT_NAME ffi
  OUTPUT_NAME ffi
  POSITION_INDEPENDENT_CODE ON
  VERSION 8.1.0
  SOVERSION 8)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(ffi PRIVATE FFI_BUILDING_DLL)
else()
  target_compile_definitions(ffi PUBLIC FFI_STATIC_BUILD)
endif()
if(LIBFFI_ENABLE_DEBUG)
  target_compile_definitions(ffi PRIVATE FFI_DEBUG)
endif()
if(MSVC)
  target_compile_options(ffi PRIVATE /W3)
else()
  target_compile_options(ffi PRIVATE -fexceptions)
endif()

set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
set(toolexeclibdir "\${libdir}")
set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
configure_file(libffi.pc.in "${CMAKE_CURRENT_BINARY_DIR}/libffi.pc" @ONLY)

set(_ffi_cmake_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/libffi")
configure_package_config_file(
  cmake/libffiConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/libffiConfig.cmake"
  INSTALL_DESTINATION "${_ffi_cmake_install_dir}")
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/libffiConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion)

install(TARGETS ffi EXPORT libffiTargets
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/include/ffi.h"
  "${CMAKE_CURRENT_BINARY_DIR}/include/ffitarget.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/libffi.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
install(EXPORT libffiTargets
  FILE libffiTargets.cmake
  NAMESPACE libffi::
  DESTINATION "${_ffi_cmake_install_dir}")
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/libffiConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/libffiConfigVersion.cmake"
  DESTINATION "${_ffi_cmake_install_dir}")

if(LIBFFI_BUILD_TESTING)
  include(CTest)
  if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_executable(libffi-cmake-smoke cmake/tests/smoke.c)
    target_link_libraries(libffi-cmake-smoke PRIVATE libffi::ffi)
    add_test(NAME libffi.cmake.smoke COMMAND libffi-cmake-smoke)
  endif()
endif()

message(STATUS "libffi target: ${TARGET} (${TARGETDIR})")
message(STATUS "libffi sources: ${_ffi_arch_sources};${_ffi_asm_sources}")
