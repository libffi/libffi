cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW) # respect CMAKE_MSVC_RUNTIME_LIBRARY

project(libffi LANGUAGES C)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(FFI_DEBUG "Extra debugging" OFF)

if(MSVC)
    add_compile_options(/Zc:__cplusplus /permissive-)
else()
    message(FATAL_ERROR "TODO: support non-MSVC builds")
endif()

include(CheckIncludeFile)
include(CheckTypeSize)
include(CheckFunctionExists)

check_include_file("dlfcn.h" HAVE_DLFCN_H)
check_include_file("inttypes.h" HAVE_INTTYPES_H)
check_include_file("memory.h" HAVE_MEMORY_H)
check_include_file("stdlib.h" HAVE_STDLIB_H)
check_include_file("strings.h" HAVE_STRINGS_H)
check_include_file("string.h" HAVE_STRING_H)
check_include_file("sys/mman.h" HAVE_SYS_MMAN_H)
check_include_file("sys/stat.h" HAVE_SYS_STAT_H)
check_include_file("sys/types.h" HAVE_SYS_TYPES_H)
check_include_file("unistd.h" HAVE_UNISTD_H)
check_type_size("size_t" SIZEOF_SIZE_T LANGUAGE C)
check_type_size("double" SIZEOF_DOUBLE LANGUAGE C)
check_type_size("long double" SIZEOF_LONG_DOUBLE LANGUAGE C)
check_function_exists("memcpy" HAVE_MEMCPY)

set(HAVE_LONG_DOUBLE ${HAVE_SIZEOF_LONG_DOUBLE})
set(TARGET X86_WIN64)

set(FFI_EXEC_TRAMPOLINE_TABLE 0) # 1 on apple arm

configure_file(include/ffi.h.in ffi.h @ONLY)
configure_file(include/fficonfig.h.cmake.in fficonfig.h @ONLY)

add_custom_command(
    OUTPUT ffi_cc.s
    COMMAND cl.exe /EP /nologo /I${CMAKE_CURRENT_SOURCE_DIR}/include /I${CMAKE_CURRENT_SOURCE_DIR}/src/x86 /I. "${CMAKE_CURRENT_SOURCE_DIR}/src/x86/win64_intel.S" > ffi_cc.s
    MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/src/x86/win64_intel.S"
    DEPENDS fficonfig.h
    VERBATIM
)

add_custom_command(
    OUTPUT ffi_cc.obj
    COMMAND ml64.exe /nologo /c ffi_cc.s /Zi
    MAIN_DEPENDENCY ffi_cc.s
    VERBATIM
)

set(SRCS
    src/prep_cif.c
    src/types.c
    src/raw_api.c
    src/java_raw_api.c
    src/closures.c
    src/tramp.c
)

if(FFI_DEBUG)
    list(APPEND SRCS src/debug.c)
endif()

list(APPEND SRCS
    src/x86/ffiw64.c
    ffi_cc.obj
)

add_library(libffi ${SRCS})
target_compile_definitions(libffi PRIVATE USE_DL_PREFIX FFI_BUILDING_DLL)
target_include_directories(libffi PUBLIC include)
target_include_directories(libffi PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(libffi PRIVATE src/x86)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(libffi PRIVATE FFI_BUILDING_DLL)
else()
    target_compile_definitions(libffi PRIVATE FFI_STATIC_BUILD)
endif()
