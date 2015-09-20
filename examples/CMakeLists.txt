
add_definitions(-DUNICODE -D_UNICODE)

if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -municode")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mwindows -static-libgcc")
elseif(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MT")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO /SUBSYSTEM:WINDOWS")
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
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

