
add_library(hsluv-c STATIC hsluv.h hsluv.c)

# In Windows SDK, math functions are part of C runtime lib.
if(NOT "${CMAKE_C_COMPILER_ID}" STREQUAL "MSVC")
    target_link_libraries(hsluv-c m)
endif()
