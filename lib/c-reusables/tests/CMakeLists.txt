
if(CMAKE_COMPILER_IS_GNUCC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
    if(WIN32)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mconsole -static-libgcc")
    endif()
elseif(MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MT")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO /SUBSYSTEM:CONSOLE")
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
endif()


add_definitions(-DCRE_TEST)


add_executable(test-buffer acutest.h test-buffer.c ../data/buffer.h ../data/buffer.c)
target_include_directories(test-buffer PRIVATE ../data)

add_executable(test-list acutest.h test-list.c ../data/list.h)
target_include_directories(test-list PRIVATE ../data)

add_executable(test-rbtree acutest.h test-rbtree.c ../data/rbtree.h ../data/rbtree.c)
target_include_directories(test-rbtree PRIVATE ../data)

add_executable(test-value acutest.h test-value.c ../data/value.h ../data/value.c)
target_include_directories(test-value PRIVATE ../data)

add_executable(test-base64 acutest.h test-base64.c ../encode/base64.h ../encode/base64.c)
target_include_directories(test-base64 PRIVATE ../encode)

add_executable(test-hex acutest.h test-hex.c ../encode/hex.h ../encode/hex.c)
target_include_directories(test-hex PRIVATE ../encode)

add_executable(test-crc32 acutest.h test-crc32.c ../hash/crc32.h ../hash/crc32.c)
target_include_directories(test-crc32 PRIVATE ../hash)

add_executable(test-fnv1a acutest.h test-fnv1a.c ../hash/fnv1a.h ../hash/fnv1a.c)
target_include_directories(test-fnv1a PRIVATE ../hash)

add_executable(test-cmdline acutest.h test-cmdline.c ../misc/cmdline.h ../misc/cmdline.c)
target_include_directories(test-cmdline PRIVATE ../misc)

add_executable(test-defs acutest.h test-defs.c ../misc/defs.h)
target_include_directories(test-defs PRIVATE ../misc)

add_executable(test-malloca acutest.h test-malloca.c ../mem/malloca.h)
target_include_directories(test-malloca PRIVATE ../mem)

if(WIN32)
    add_executable(test-memstream acutest.h test-memstream.c ../win32/memstream.h ../win32/memstream.c)
    target_include_directories(test-memstream PRIVATE ../win32)
endif()
