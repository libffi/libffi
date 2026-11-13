cmake_minimum_required(VERSION 3.20)
project(libffi-package-consumer LANGUAGES C)

find_package(libffi 3.8 CONFIG REQUIRED)

add_executable(libffi-package-consumer ../smoke.c)
target_link_libraries(libffi-package-consumer PRIVATE libffi::ffi)
