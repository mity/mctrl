
include_directories("${PROJECT_SOURCE_DIR}/src")

add_executable(test_hsluv acutest.h test_hsluv.c snapshot.h)
target_link_libraries(test_hsluv hsluv-c)
