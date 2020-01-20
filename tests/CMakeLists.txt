

if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc")
elseif(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MT")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
endif()


add_executable(test-build WIN32 test-build.c)
# (test-build.exe uses LoadLibrary. Do not link with the mCtrl lib.)

add_executable(test-grid WIN32 test-grid.c)
target_link_libraries(test-grid mCtrl)

add_executable(test-table WIN32 test-table.c)
target_link_libraries(test-table mCtrl)
