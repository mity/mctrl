
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
elseif(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO /SUBSYSTEM:WINDOWS")
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_RELEASE} /MT")
endif()


file(GLOB EXAMPLE_SOURCES "*.c")

foreach(SOURCE ${EXAMPLE_SOURCES})
    get_filename_component(TARGET ${SOURCE} NAME_WE)

    string(REGEX REPLACE "\\.c$" "" BASENAME ${SOURCE})

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

    # all examples link with comctl32.dll
    target_link_libraries(${TARGET} comctl32)

    # all examples but ex_dllgetversion.exe link with mCtrl.dll
    if(NOT ${TARGET} MATCHES ex_dllgetversion)
        target_link_libraries(${TARGET} mCtrl)
    endif()
endforeach()

