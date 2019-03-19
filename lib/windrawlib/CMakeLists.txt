
cmake_minimum_required(VERSION 3.1)
project(WinDrawLib C)


set(CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo MinSizeRel)
if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE $ENV{CMAKE_BUILD_TYPE})
endif()

# set up include-directories
include_directories("${PROJECT_SOURCE_DIR}/include")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

OPTION(WINDRAWLIB_BUILD_EXAMPLES "Enable/disable building of WinDrawLib examples" ON)

# Add sub-directories
add_subdirectory(src)
if(WINDRAWLIB_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
