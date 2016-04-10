
file(GLOB WINDRAWLIB_SOURCES "*.c" "*.h")

add_library(windrawlib STATIC ${WINDRAWLIB_SOURCES})

add_definitions(-DUNICODE -D_UNICODE)
add_definitions(-D_WIN32_IE=0x0501 -D_WIN32_WINNT=0x0600 -DWINVER=_WIN32_WINNT)

# Tell <windows.h> to not include everything. We include what we need anyway
# and this saves compiler a lot of work in the parsing stage, so compilation
# is faster.
add_definitions(-DWIN32_LEAN_AND_MEAN)

# Enable COM interface preprocessor wrapper macros for C.
add_definitions(-DCOBJMACROS)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

if(CMAKE_COMPILER_IS_GNUCC)
    # Detect gcc version:
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)

    # Enable many warnings:
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wdouble-promotion")

    # Use unicode (-W suffixed symbols):
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -municode")

    # Use faster but less precise float math; it suffices for our purposes:
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffast-math")

    # By default, CMake uses -O3 for Release builds. Lets stick with safer -O2:
    string(REGEX REPLACE "(^| )-O[0-9a-z]+" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
elseif(MSVC)
    # Disable warnings about the so-called unsecured functions:
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)

    # Use faster but less precise float math; it suffices for our purposes:
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fp:fast")

    # Specify proper C runtime library:
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_RELEASE} /MT")
endif()

