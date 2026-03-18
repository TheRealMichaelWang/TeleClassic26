cmake_minimum_required(VERSION 3.14)
project(TeleClassic26 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PLIBSYS_TESTS OFF CACHE BOOL "" FORCE)
set(PLIBSYS_BUILD_DOC OFF CACHE BOOL "" FORCE)
add_subdirectory(plibsys-master)

add_subdirectory(src)
