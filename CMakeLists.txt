
cmake_minimum_required(VERSION 3.0)
project(mCtrl C)

set(MCTRL_MAJOR_VERSION 0)
set(MCTRL_MINOR_VERSION 9)
set(MCTRL_PATCH_VERSION 6)
set(MCTRL_VERSION ${MCTRL_MAJOR_VERSION}.${MCTRL_MINOR_VERSION}.${MCTRL_PATCH_VERSION})


set(CMAKE_CONFIGURATION_TYPES Debug Release)
if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE $ENV{CMAKE_BUILD_TYPE})
endif()


set(CMAKE_SHARED_LIBRARY_PREFIX "")
#set(IMPORT_PREFIX "lib")
#set(IMPORT_SUFFIX ".a")


set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}")
set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}")


# set up include-directories
include_directories("${PROJECT_SOURCE_DIR}/include")


# Add sub-directories
add_subdirectory(src)
add_subdirectory(examples)
