cmake_minimum_required(VERSION 2.8)

project (tcg)

option (BIG_ENDIAN "Big endian" OFF)
set (HOST_LONG_BITS 0 CACHE INT "Host word size (0 - autodetection)")
set (HOST_ARCHITECTURE "i386" CACHE STRING "Host architecture")
set_property (CACHE HOST_ARCHITECTURE PROPERTY STRINGS i386 arm)
set (TARGET_LONG_BITS 32 CACHE INT "Target word size")

if(HOST_LONG_BITS EQUAL 0)
    # Determine bitness
    # 32 or 64 bits
    set (HOST_LONG_BITS 32 CACHE INT "Host word size (0 - autodetection)" FORCE)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set (HOST_LONG_BITS 64 CACHE INT "Host word size (0 - autodetection)" FORCE)
    endif()
endif()

string (TOUPPER ${HOST_ARCHITECTURE} HOST_UPPER)

if(BIG_ENDIAN)
    set (BIG_ENDIAN_DEF -DTARGET_WORDS_BIGENDIAN)
endif()

add_definitions (
    -fomit-frame-pointer
    -fPIC

    -DTARGET_LONG_BITS=${TARGET_LONG_BITS}
    -DHOST_LONG_BITS=${HOST_LONG_BITS}

    ${BIG_ENDIAN_DEF}
    )

include_directories (${HOST_ARCHITECTURE})

add_library (tcg STATIC
    additional.c
    optimize.c
    tcg.c
    )

