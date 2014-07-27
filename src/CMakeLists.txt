
configure_file(version.h.in "version.h" @ONLY)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

file(GLOB MCTRL_SOURCES "*.c" "*.h" "*.rc" "mCtrl.def")

add_library(mCtrl SHARED ${MCTRL_SOURCES})


if(CMAKE_COMPILER_IS_GNUCC)
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -municode")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")

    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc -Wl,--enable-stdcall-fixup -Wl,--kill-at")

    # With gcc >= 4.9, use LTO for release builds:
    if(GCC_VERSION VERSION_GREATER 4.9 OR GCC_VERSION VERSION_EQUAL 4.9)
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -flto")
        set(CMAKE_LINKER_FLAGS_RELEASE "${CMAKE_LINKER_FLAGS_RELEASE} -flto")
    endif()

    # With gcc/binutils toolchain (e.g. mingw-w64), gcc '-Wl,--kill-at results'
    # results in unusable import lib because gcc/ld then un-decorates not just
    # DLL exports but unfortunately also import lib symbols. Hence lets build
    # this as module and create import lib manually below (with 'gendef' and
    # 'dlltool').
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
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /n")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MANIFEST:NO")
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
endif()


add_definitions(
    -DUNICODE -D_UNICODE
    -D_WIN32_IE=0x0501 -D_WIN32_WINNT=0x0600 -DWINVER=_WIN32_WINNT
    -D_CRT_NON_CONFORMING_SWPRINTFS
    -DWIN32_LEAN_AND_MEAN
    -DMCTRL_BUILD
)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -D__INLINE_ISEQUAL_GUID")

target_link_libraries(mCtrl gdi32 comctl32)
