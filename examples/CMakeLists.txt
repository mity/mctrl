

# Note this file serves twofold purpose. It serves as a partial CMakefile
# as part of complete mCtrl source tree, and it is also capable to work
# as a standalone CMakefile when deployed with the directory examples into
# a release binary package.

# === The stand-alone mode ==================================
if(NOT "${BUILD_COMPLETE_MCTRL}" STREQUAL "TRUE")
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
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -static-libgcc")

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
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO /SUBSYSTEM:WINDOWS")
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_RELEASE} /MT")
endif()


# Each example mini-app is implemented in a single C source file.
file(GLOB EXAMPLE_SOURCES "*.c")
foreach(SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(TARGET ${SOURCE} NAME_WE)

    string(REGEX REPLACE "\\.c$" "" BASENAME ${SOURCE})

    # Check if this example has an appropriate header and/or resource script
    # and add those to the mini-project.
    if(EXISTS ${BASENAME}.h)
        set(HEADER ${BASENAME}.h)
    else()
        unset(HEADER)
    endif()
    if(EXISTS ${BASENAME}.rc)
        set(RESOURCE ${BASENAME}.rc)
    else()
        set(RESOURCE example-default.rc)
    endif()
    add_executable(${TARGET} ${SOURCE} ${HEADER} ${RESOURCE})

    # All examples link with comctl32.dll.
    target_link_libraries(${TARGET} comctl32)

    # All examples but ex_dllgetversion.exe link with mCtrl.dll.
    # (ex_dllgetversion.exe loads it via LoadLibrary().)
    if(NOT ${TARGET} MATCHES ex_dllgetversion)
        target_link_libraries(${TARGET} mCtrl)
    endif()
endforeach()
