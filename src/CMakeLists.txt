
configure_file(version.h.in "version.h" @ONLY)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

file(GLOB MCTRL_SOURCES "*.c" "*.h" "*.rc" "mCtrl.def")

add_library(mCtrl SHARED ${MCTRL_SOURCES})

target_link_libraries(mCtrl gdi32 comctl32 uuid)

add_definitions(-DMCTRL_BUILD)
add_definitions(-DUNICODE -D_UNICODE)
add_definitions(-D_WIN32_IE=0x0501 -D_WIN32_WINNT=0x0600 -DWINVER=_WIN32_WINNT)

# Tell <windows.h> to not include everything. We include what we need anyway
# and this saves compiler a lot of work in the parsing stage, so compilation
# is faster.
add_definitions(-DWIN32_LEAN_AND_MEAN)

# Enable definition of some constants (e.g. M_PI) in <math.h>.
add_definitions(-D_USE_MATH_DEFINES)

# Enable COM interface preprocessor wrapper macros for C.
add_definitions(-DCOBJMACROS)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

if(CMAKE_COMPILER_IS_GNUCC)
    # Detect gcc version:
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -municode -ffast-math")

    # By default, CMake uses -O3 for Release builds. Lets stick with safer -O2:
    string(REGEX REPLACE "(^| )-O[0-9a-z]+" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")

    # With gcc >= 4.9, use LTO for Release builds:
    if(GCC_VERSION VERSION_GREATER 4.9 OR GCC_VERSION VERSION_EQUAL 4.9)
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -flto -fno-fat-lto-objects")
        set(CMAKE_LINKER_FLAGS_RELEASE "${CMAKE_LINKER_FLAGS_RELEASE} -flto -O2")
    endif()

    # Ensure we do not depend on libgcc.dll and that our exported symbols
    # don't have undesired symbol decorations:
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -Wl,--enable-stdcall-fixup -Wl,--kill-at")

    # By default, mingw-w64 uses its own swprintf() implementation (for the
    # sake of standard compliance). This enforces swprintf() from MSVCRT.DLL:
    add_definitions(-D_CRT_NON_CONFORMING_SWPRINTFS)

    # With gcc/binutils toolchain (e.g. mingw-w64), gcc option -Wl,--kill-at
    # results in unusable import lib because gcc/ld then un-decorates not just
    # DLL exports but unfortunately also import lib symbols. Hence lets create
    # the import lib manually below with gendef and dlltool.
    #
    # We assume 'gendef' and 'dlltool' are located in the same dir where
    # gcc is living.
    string(REGEX REPLACE "\(.*/.*)gcc" "\\1gendef" GENDEF ${CMAKE_C_COMPILER})
    string(REGEX REPLACE "\(.*/.*)gcc" "\\1dlltool" DLLTOOL ${CMAKE_C_COMPILER})
    if("${GENDEF}" MATCHES ".*gendef.*"  AND  "${DLLTOOL}" MATCHES ".*dlltool.*")
        if(EXISTS "${GENDEF}"  AND  EXISTS "${DLLTOOL}")
            message(STATUS "Using gendef+dlltool for creation of import library libmCtrl.a")
            add_custom_command(
                    TARGET "mCtrl" POST_BUILD
                    COMMAND ${GENDEF} ${GENDEF_FLAGS} -a mCtrl.dll
                    COMMAND ${DLLTOOL} ${DLLTOOL_FLAGS} --kill-at --input-def mCtrl.def --output-lib libmCtrl.dll.a --dllname mCtrl.dll
                    WORKING_DIRECTORY "${LIBRARY_OUTPUT_PATH}"
                    COMMENT "Creating import library."
                    VERBATIM
            )
        endif()
    endif()
elseif(MSVC)
    # Disable warnings about the so-called unsecured functions:
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fp:fast")

    # Make sure strings in string table resource are zero-terminated.
    # (RC.EXE by default removes the zero terminators even if they are
    # explicitly specified in the resource script):
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /n")

    # We provide our own manifest, so tell the linker to not generate it:
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MANIFEST:NO")

    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_RELEASE} /MT")
endif()

