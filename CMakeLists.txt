
cmake_minimum_required(VERSION 3.7)
project(mCtrl C)

set(MCTRL_MAJOR_VERSION 0)
set(MCTRL_MINOR_VERSION 11)
set(MCTRL_PATCH_VERSION 5)
set(MCTRL_VERSION ${MCTRL_MAJOR_VERSION}.${MCTRL_MINOR_VERSION}.${MCTRL_PATCH_VERSION})


set(CMAKE_CONFIGURATION_TYPES Debug Release RelWithDebInfo MinSizeRel)
if ("${CMAKE_BUILD_TYPE}" STREQUAL "")
    set(CMAKE_BUILD_TYPE $ENV{CMAKE_BUILD_TYPE})
endif()


set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

# set up include-directories
include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories("${PROJECT_SOURCE_DIR}/lib/acutest/include")
include_directories("${PROJECT_SOURCE_DIR}/lib/hsluv-c/src")

set(HSLUV_C_TESTS OFF CACHE BOOL "Disable building hsluv-c tests.")
set(MCTRL_BUILD_COMPLETETREE ON CACHE BOOL "Tell examples/CMakeListst.txt it is built as part of complete mCtrl build.")

OPTION(MCTRL_BUILD_EXAMPLES "Enable/disable building of mCtrl examples" ON)
OPTION(MCTRL_BUILD_TESTS "Enable/disable building of mCtrl tests" ON)


if(MSVC)
    # Enforce multi-threading runtime lib.
    string(REGEX REPLACE "/M[DT]d?" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REGEX REPLACE "/M[DT]d?" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REGEX REPLACE "/M[DT]d?" "" CMAKE_C_FLAGS_RELWITHDEBINFO "{$CMAKE_C_FLAGS_RELWITHDEBINFO}")
    string(REGEX REPLACE "/M[DT]d?" "" CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL}")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_RELEASE} /MT")
endif()


# Add sub-directories
add_subdirectory(lib/hsluv-c)
add_subdirectory(src)
if(MCTRL_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(MCTRL_BUILD_TESTS)
    add_subdirectory(tests)
endif()
