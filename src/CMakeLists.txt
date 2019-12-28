
configure_file(version.h.in "version.h" @ONLY)
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

set(CRE_PATH "${PROJECT_SOURCE_DIR}/lib/c-reusables")

add_library(mCtrl SHARED
    mCtrl.def

    # from c-reusables
    ${CRE_PATH}/data/buffer.c       ${CRE_PATH}/data/buffer.h
    ${CRE_PATH}/win32/memstream.c   ${CRE_PATH}/win32/memstream.h

    # Source:                       # Header:       # Public header:
                                                    ../include/mctrl.h
                                                    ../include/mCtrl/_common.h
                                                    ../include/mCtrl/_defs.h
    anim.c                          anim.h
    button.c                        button.h        ../include/mCtrl/button.h
    chart.c                         chart.h         ../include/mCtrl/chart.h
    color.c                         color.h
                                    compat.h
    debug.c                         debug.h
    dialog.c                        dialog.h        ../include/mCtrl/dialog.h
    doublebuffer.c                  doublebuffer.h
    dsa.c                           dsa.h
    expand.c                        expand.h        ../include/mCtrl/expand.h
    generic.c                       generic.h
    grid.c                          grid.h          ../include/mCtrl/grid.h
    html.c                          html.h          ../include/mCtrl/html.h
    imgview.c                       imgview.h       ../include/mCtrl/imgview.h
    labeledit.c                     labeledit.h
    mditab.c                        mditab.h        ../include/mCtrl/mditab.h
    mdtext.c                        mdtext.h
    mdview.c                        mdview.h        ../include/mCtrl/mdview.h
    menubar.c                       menubar.h       ../include/mCtrl/menubar.h
    misc.c                          misc.h
    module.c                        module.h
    mousedrag.c                     mousedrag.h
    mousewheel.c                    mousewheel.h
    resource.h                      resource.rc
    rgn16.c                         rgn16.h
    table.c                         table.h         ../include/mCtrl/table.h
    tooltip.c                       tooltip.h
    treelist.c                      treelist.h      ../include/mCtrl/treelist.h
    url.c                           url.h
    version.c                       version.h.in
    viewlist.c                      viewlist.h
    xcom.c                          xcom.h
    xdwm.c                          xdwm.h
    xd2d.c                          xd2d.h
    xdwrite.c                       xdwrite.h
    xwic.c                          xwic.h
)

target_link_libraries(mCtrl
    gdi32 comctl32 ole32 oleaut32 uuid uxtheme windowscodecs    # Windows SDK libs
    hsluv-c md4c-utf16)                                         # Libs distributed with us

add_definitions(-DMCTRL_BUILD)
add_definitions(-DUNICODE -D_UNICODE)
add_definitions(-D_WIN32_IE=0x0501 -D_WIN32_WINNT=0x0600 -DWINVER=_WIN32_WINNT)

# Tell <windows.h> to not include everything. We include what we need anyway
# and this saves compiler a lot of work in the parsing stage, so compilation
# is faster.
add_definitions(-DWIN32_LEAN_AND_MEAN)

# Enable COM interface preprocessor wrapper macros for C.
add_definitions(-DCOBJMACROS)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

if("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    # Enable many warnings:
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wdouble-promotion")

    # Use unicode (-W suffixed symbols):
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -municode")

    # Use faster but less precise float math; it suffices for our purposes:
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ffast-math")

    # By default, mingw-w64 uses its own swprintf() implementation, for the
    # sake of standard compliance. We don't need it, so lets enforce the one
    # from MSVCRT.DLL:
    add_definitions(-D_CRT_NON_CONFORMING_SWPRINTFS)

    # We don't want to depend on LIBGCC.DLL:
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc")

    # Ensure our exported symbols don't have undesired symbol decorations:
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--enable-stdcall-fixup -Wl,--kill-at")

    # Enable DEP (data execution prevention) and ASLR (address space layout
    # randomization).
    # See https://lists.gnu.org/archive/html/bug-binutils/2015-09/msg00204.html
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--nxcompat")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--disable-auto-image-base,--dynamicbase")
    if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
        # For 64-bit build, enable higher entropy for the memory relocations:
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--high-entropy-va")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--image-base,0x180000000")
    endif()

    # With gcc >= 4.9, use LTO for Release builds:
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 4.9)
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -flto -fno-fat-lto-objects")
        set(CMAKE_LINKER_FLAGS_RELEASE "${CMAKE_LINKER_FLAGS_RELEASE} -flto")
    endif()

    # With recent gcc versions, use -O3 rather then -O2.
    # Set it also for LDFLAGS (needed in the case of LTO build):
    if(CMAKE_C_COMPILER_VERSION VERSION_GREATER_EQUAL 8.0)
        set(OPTIMIZE_LEVEL "-O3")
    else()
        set(OPTIMIZE_LEVEL "-O2")
    endif()
    # Remove the default optimization options:
    string(REGEX REPLACE "(^| )-O[0-9a-z]+" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    string(REGEX REPLACE "(^| )-O[0-9a-z]+" "" CMAKE_LINKER_FLAGS_RELEASE "${CMAKE_LINKER_FLAGS_RELEASE}")
    # Enforce the desired ones:
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} ${OPTIMIZE_LEVEL}")
    set(CMAKE_LINKER_FLAGS_RELEASE "${CMAKE_LINKER_FLAGS_RELEASE} ${OPTIMIZE_LEVEL}")

    # With gcc/binutils toolchain (e.g. mingw-w64), gcc option -Wl,--kill-at
    # results in unusable import lib because gcc/ld then un-decorates not just
    # DLL exports but unfortunately also the import lib symbols.
    #
    # Therefore, lets re-create the import lib manually below with gendef and
    # dlltool. (We assume the tools are located in the same dir as gcc.)
    string(REGEX REPLACE "\(.*/.*)gcc" "\\1gendef" GENDEF ${CMAKE_C_COMPILER})
    string(REGEX REPLACE "\(.*/.*)gcc" "\\1dlltool" DLLTOOL ${CMAKE_C_COMPILER})
    if("${GENDEF}" MATCHES ".*gendef.*"  AND  "${DLLTOOL}" MATCHES ".*dlltool.*")
        if(EXISTS "${GENDEF}"  AND  EXISTS "${DLLTOOL}")
            message(STATUS "Using gendef+dlltool for creation of import library libmCtrl.a")

            get_property(MCTRL_DLL_DIR TARGET mCtrl PROPERTY RUNTIME_OUTPUT_DIRECTORY)
            get_property(MCTRL_IMPORT_DIR TARGET mCtrl PROPERTY ARCHIVE_OUTPUT_DIRECTORY)

            add_custom_command(
                    TARGET "mCtrl" POST_BUILD
                    COMMAND ${GENDEF} ${GENDEF_FLAGS} -a mCtrl.dll
                    COMMAND ${DLLTOOL} ${DLLTOOL_FLAGS} --kill-at --input-def mCtrl.def --output-lib ${MCTRL_IMPORT_DIR}/libmCtrl.dll.a --dllname mCtrl.dll
                    WORKING_DIRECTORY ${MCTRL_DLL_DIR}
                    COMMENT "Creating import library."
                    VERBATIM
            )
        endif()
    endif()
endif()

if("${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
    # Disable warnings about the so-called unsecured functions:
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)

    # Use faster but less precise float math; it suffices for our purposes:
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /fp:fast")

    # Make sure strings in string table resource are zero-terminated.
    # (RC.EXE by default removes the zero terminators even if they are
    # explicitly specified in the resource script):
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /n")

    # We provide our own manifest, so tell the linker to not generate it:
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /MANIFEST:NO")

    # MSVC 6.0 does not expose GdiAlphaBlend() and friends from import lib
    # for GDI32.DLL. We have to use the wrappers from MSIMG32.DLL.
    # (See also preprocessor magic in src/compat.h)
    if(NOT(MSVC_VERSION GREATER 1200))
        target_link_libraries(mCtrl msimg32)
    endif()
endif()

