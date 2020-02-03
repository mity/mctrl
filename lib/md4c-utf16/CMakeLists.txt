
add_definitions(-DUNICODE -D_UNICODE)
add_definitions(-D_WIN32_IE=0x0501 -D_WIN32_WINNT=0x0600 -DWINVER=_WIN32_WINNT)

# Tell <windows.h> to not include everything. We include what we need anyway
# and this saves compiler a lot of work in the parsing stage, so compilation
# is faster.
add_definitions(-DWIN32_LEAN_AND_MEAN)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG")

if(CMAKE_COMPILER_IS_GNUCC)
    # Enable many warnings:
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wdouble-promotion")
elseif(MSVC)
    # Disable warnings about the so-called unsecured functions:
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
endif()

include_directories("${PROJECT_SOURCE_DIR}")
add_library(md4c-utf16 STATIC md4c-utf16.h md4c-utf16.c)
