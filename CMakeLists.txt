# mgr - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(mgr LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(generated/rexglue.cmake)

# Sources
set(MGR_SOURCES
    src/main.cpp
)

if(WIN32)
    add_executable(mgr WIN32 ${MGR_SOURCES})
else()
    add_executable(mgr ${MGR_SOURCES})
endif()

rexglue_setup_target(mgr)
