
cmake_minimum_required(VERSION 3.1)
project(WinDrawLib C)


set(CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo MinSizeRel)
if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE $ENV{CMAKE_BUILD_TYPE})
endif()


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# set up include-directories
include_directories("${PROJECT_SOURCE_DIR}/include")

# Add sub-directories
add_subdirectory(src)
add_subdirectory(examples)
