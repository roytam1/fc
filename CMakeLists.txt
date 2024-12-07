# CMake minimum version
cmake_minimum_required(VERSION 3.0)

# project name and language
project(fc C RC)

# add include directories
include_directories(.)

# fc.exe
add_executable(fc fc.c texta.c textw.c fc.rc)
target_link_libraries(fc comctl32 shlwapi)
