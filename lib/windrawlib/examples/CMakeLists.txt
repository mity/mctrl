
add_definitions(-DUNICODE -D_UNICODE)
add_definitions(-D_WIN32_IE=0x0501 -D_WIN32_WINNT=0x0501 -DWINVER=_WIN32_WINNT)


if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wdouble-promotion -municode")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc")

    # By default, CMake uses -O3 for Release builds. Lets stick with safer -O2:
    string(REGEX REPLACE "(^| )-O[0-9a-z]+" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
elseif(MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELEASE} /MT")
    set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_RELEASE} /MT")
endif()


add_executable("cached-image" WIN32 "cached-image.c" "cached-image.rc")
target_link_libraries("cached-image" "windrawlib")

add_executable("draw-cached" WIN32 "draw-cached.c")
target_link_libraries("draw-cached" "windrawlib")

add_executable("draw-simple" WIN32 "draw-simple.c")
target_link_libraries("draw-simple" "windrawlib")

add_executable("draw-string" WIN32 "draw-string.c")
target_link_libraries("draw-string" "windrawlib")

add_executable("draw-styled" WIN32 "draw-styled.c")
target_link_libraries("draw-styled" "windrawlib")

add_executable("image-from-buffer" WIN32 "image-from-buffer.c" "image-from-buffer.h" "image-from-buffer.rc")
target_link_libraries("image-from-buffer" "windrawlib")

add_executable("image-from-hbitmap" WIN32 "image-from-hbitmap.c" "image-from-hbitmap.h" "image-from-hbitmap.rc")
target_link_libraries("image-from-hbitmap" "windrawlib")

add_executable("image-from-res" WIN32 "image-from-res.c" "image-from-res.rc")
target_link_libraries("image-from-res" "windrawlib")
