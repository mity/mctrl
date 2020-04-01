

# Note this file serves twofold purpose. It serves as a partial CMakefile
# as part of complete mCtrl source tree, and it is also capable to work
# as a standalone CMakefile when deployed with the directory examples into
# a release binary package.

# === The stand-alone mode ==================================
if(NOT "${MCTRL_BUILD_COMPLETETREE}")
    cmake_minimum_required(VERSION 3.1)
    project(mCtrl-examples C)
    set(EXECUTABLE_OUTPUT_PATH "${PROJECT_BINARY_DIR}")
    include_directories("${PROJECT_SOURCE_DIR}/../include")
    if("${CMAKE_SIZEOF_VOID_P}" EQUAL 8)
        link_directories("${PROJECT_SOURCE_DIR}/../lib64")
    else()
        link_directories("${PROJECT_SOURCE_DIR}/../lib")
    endif()
endif()
# ===========================================================


add_definitions(-DUNICODE -D_UNICODE)
add_definitions(-D_WIN32_IE=0x0501 -D_WIN32_WINNT=0x0501 -DWINVER=_WIN32_WINNT)


if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wdouble-promotion -municode")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc")

    # By default, CMake uses -O3 for Release builds. Lets stick with safer -O2:
    string(REGEX REPLACE "(^| )-O[0-9a-z]+" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")

    # By default, mingw-w64 uses its own swprintf() implementation (for the
    # sake of standard compliance). This enforces swprintf() from MSVCRT.DLL:
    add_definitions(-D_CRT_NON_CONFORMING_SWPRINTFS)

    # Enable DEP (data execution prevention) and ASLR (address space layout
    # randomization).
    # See https://lists.gnu.org/archive/html/bug-binutils/2015-09/msg00204.html
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--nxcompat")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--disable-auto-image-base -Wl,--dynamicbase")
    if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
        # For 64-bit build, enable higher entropy for the memory relocations:
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--high-entropy-va")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--image-base -Wl,0x140000000")
    endif()
elseif(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
endif()


add_executable(example-button WIN32 example-button.c example-button.h example-button.rc)
target_link_libraries(example-button mCtrl comctl32)

add_executable(example-dllgetversion WIN32 example-dllgetversion.c example-default.rc)

add_executable(example-expand WIN32 example-expand.c example-expand.h example-expand.rc)
target_link_libraries(example-expand mCtrl comctl32)

add_executable(example-grid WIN32 example-grid.c example-grid.h example-grid.rc)
target_link_libraries(example-grid mCtrl comctl32)

add_executable(example-grid-customdraw WIN32 example-grid-customdraw.c example-default.rc)
target_link_libraries(example-grid-customdraw mCtrl comctl32)

add_executable(example-grid-ownerdata WIN32 example-grid-ownerdata.c example-default.rc)
target_link_libraries(example-grid-ownerdata mCtrl comctl32)

add_executable(example-html WIN32 example-html.c example-html.rc)
target_link_libraries(example-html mCtrl comctl32)

add_executable(example-chart WIN32 example-chart.c example-chart.h example-chart.rc)
target_link_libraries(example-chart mCtrl comctl32)

add_executable(example-imgview WIN32 example-imgview.c example-imgview.h example-imgview.rc)
target_link_libraries(example-imgview mCtrl comctl32)

add_executable(example-mditab WIN32 example-mditab.c example-mditab.h example-mditab.rc)
target_link_libraries(example-mditab mCtrl comctl32)

add_executable(example-mdview WIN32 example-mdview.c example-mdview.h example-mdview.rc)
target_link_libraries(example-mdview mCtrl comctl32)

add_executable(example-mditab-dwm WIN32 example-mditab-dwm.c example-mditab-dwm.h example-mditab-dwm.rc)
target_link_libraries(example-mditab-dwm mCtrl comctl32)

add_executable(example-menubar WIN32 example-menubar.c example-menubar.h example-menubar.rc)
target_link_libraries(example-menubar mCtrl comctl32)

add_executable(example-treelist WIN32 example-treelist.c example-default.rc)
target_link_libraries(example-treelist mCtrl comctl32)

add_executable(example-treelist-ownerdata WIN32 example-treelist-ownerdata.c example-treelist-ownerdata.h example-treelist-ownerdata.rc)
target_link_libraries(example-treelist-ownerdata mCtrl comctl32 advapi32)
