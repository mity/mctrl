
cmake_minimum_required(VERSION 3.0)
project(mCtrl C)

set(MCTRL_MAJOR_VERSION 0)
set(MCTRL_MINOR_VERSION 9)
set(MCTRL_PATCH_VERSION 6)
set(MCTRL_VERSION ${MCTRL_MAJOR_VERSION}.${MCTRL_MINOR_VERSION}.${MCTRL_PATCH_VERSION})

set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug or Release)")
set(CMAKE_CONFIGURATION_TYPES Debug Release)

set(CMAKE_SHARED_LIBRARY_PREFIX "")
#set(IMPORT_PREFIX "lib")
#set(IMPORT_SUFFIX ".a")


if (CMAKE_COMPILER_IS_GNUCC)
    set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")
    set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/bin")
elseif(MSVC)
    set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}")
    set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}")
endif()


# set up include-directories
include_directories("${PROJECT_SOURCE_DIR}/include")


# Add sub-directories
add_subdirectory(src)
add_subdirectory(examples)
