cmake_minimum_required(VERSION 2.8)
include (ExternalProject)

project (tlib)

option (DEBUG "Debug mode" OFF)

option (HOST_BIG_ENDIAN "Host big endian" OFF)
set (HOST_ARCH "i386" CACHE STRING "Host architecture")
set (HOST_WORD_SIZE "64" CACHE STRING "Host word size")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE "Release" CACHE
	      STRING "Choose the type of build." FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
	    "Debug" "Release" "RelWithDebInfo")
endif()

option (TARGET_BIG_ENDIAN "Target big endian" OFF)
set (TARGET_ARCH "" CACHE STRING "Target architecture")
set (TARGET_WORD_SIZE "32" CACHE STRING "Target word size")

# this must be placed after setting default values for:
# TARGET_BIG_ENDIAN, HOST_WORD_SIZE, TARGET_WORD_SIZE
ExternalProject_Add (tcglib
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/tcg
    PREFIX tcg
    CMAKE_ARGS -DBIG_ENDIAN:BOOL=${TARGET_BIG_ENDIAN} -DHOST_LONG_BITS:INT=${HOST_WORD_SIZE} -DTARGET_LONG_BITS:INT=${TARGET_WORD_SIZE}
    INSTALL_COMMAND "")

set_property (CACHE HOST_ARCH PROPERTY STRINGS i386 arm)
set_property (CACHE TARGET_ARCH PROPERTY STRINGS i386 arm arm-m sparc ppc riscv)

if(NOT HOST_ARCH)
    message (FATAL_ERROR "Host architecture not set")
endif()

if(NOT TARGET_ARCH)
    message (FATAL_ERROR "Target architecture not set")
endif()

string (TOUPPER "${HOST_ARCH}" HOST_ARCH_U)
string (TOUPPER "${TARGET_ARCH}" TARGET_ARCH_U)

if(TARGET_BIG_ENDIAN)
    set (BIG_ENDIAN_DEF -DTARGET_WORDS_BIGENDIAN=1)
endif()

set (TARGET_ACTUAL_ARCH ${TARGET_ARCH})
if("${TARGET_ARCH}" STREQUAL "arm-m")
    set (TARGET_ACTUAL_ARCH "arm")
    set (ARM_M_DEF -DTARGET_PROTO_ARM_M=1)
endif()
string (TOUPPER "${TARGET_ACTUAL_ARCH}" TARGET_ACTUAL_ARCH_U)

add_definitions (
    -fomit-frame-pointer
    -fPIC
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-sign-compare

    -DCONFIG_NEED_MMU
    -DCONFIG_${HOST_ARCH_U}_DIS=1
    -DCONFIG_${TARGET_ACTUAL_ARCH_U}_DIS=1

    -DTCG_TARGET_${HOST_ARCH_U}

    -DHOST_BITS_${HOST_WORD_SIZE}
    -DHOST_${HOST_ARCH_U}=1
    -DHOST_LONG_BITS=${HOST_WORD_SIZE}

    -DTARGET_SHORT_ALIGNMENT=2
    -DTARGET_INT_ALIGNMENT=4
    -DTARGET_LONG_ALIGNMENT=4
    -DTARGET_LLONG_ALIGNMENT=4

    -DTARGET_ARCH=${TARGET_ACTUAL_ARCH}
    -DTARGET_${TARGET_ACTUAL_ARCH_U}=1
    -DTARGET_LONG_BITS=${TARGET_WORD_SIZE}

    ${ARM_M_DEF}
    ${BIG_ENDIAN_DEF}
    )

include_directories (
    tcg
    fpu
    include
    include_empty
    tcg/${HOST_ARCH}
    arch/${TARGET_ACTUAL_ARCH}
    )

file (GLOB SOURCES
    "*.c"
    "fpu/*.c"
    "arch/*.c"
    "external/*.c"
    "arch/${TARGET_ACTUAL_ARCH}/*.c"
    )

add_library (tlib SHARED ${SOURCES})

add_dependencies (tlib tcglib)

target_link_libraries (tlib
    $<$<PLATFORM_ID:Linux>:-Wl,--wrap=memcpy>  # Decreases required glibc version
    -fPIC

    m
    rt
    pthread
    ${CMAKE_CURRENT_BINARY_DIR}/tcg/src/tcglib-build/libtcg.a
    )


