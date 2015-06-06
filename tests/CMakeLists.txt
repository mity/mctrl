

if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mconsole -static-libgcc")
elseif(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MT")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO /SUBSYSTEM:CONSOLE")
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
endif()


file(GLOB TEST_SOURCES "*.c")

foreach(SOURCE ${TEST_SOURCES})
    get_filename_component(TARGET ${SOURCE} NAME_WE)

    string(REGEX REPLACE "\\.c$" "" BASENAME ${SOURCE})

    if(EXISTS ${BASENAME}.h)
        set(HEADER ${BASENAME}.h)
    else()
        unset(HEADER)
    endif()

    add_executable(${TARGET} ${SOURCE} ${HEADER})

    # all tests but test-build.exe link with mCtrl.dll
    if(NOT ${TARGET} MATCHES test-build)
        target_link_libraries(${TARGET} mCtrl)
    endif()
endforeach()

