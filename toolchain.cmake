# toolchain.cmake - Xino AArch64

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(ARCH_TRIPLE "aarch64-none-elf")

# Compilers (ignore override); expect these to be on $PATH.
set(CMAKE_C_COMPILER   clang   CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER clang++ CACHE FILEPATH "" FORCE)
set(CMAKE_ASM_COMPILER clang   CACHE FILEPATH "" FORCE)

# LLVM binutils replacements; expect these to be on $PATH.
set(CMAKE_AR      llvm-ar      CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB  llvm-ranlib  CACHE FILEPATH "" FORCE)
set(CMAKE_OBJCOPY llvm-objcopy CACHE FILEPATH "" FORCE)
set(CMAKE_OBJDUMP llvm-objdump CACHE FILEPATH "" FORCE)
set(CMAKE_SIZE    llvm-size    CACHE FILEPATH "" FORCE)

# Targets to build for (affects compile and link driver).
set(CMAKE_C_COMPILER_TARGET   "${ARCH_TRIPLE}" CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER_TARGET "${ARCH_TRIPLE}" CACHE STRING "" FORCE)
set(CMAKE_ASM_COMPILER_TARGET "${ARCH_TRIPLE}" CACHE STRING "" FORCE)

# Use DWARF v4, no optimization for step-by-step debugging.
set(CMAKE_C_FLAGS_DEBUG   "-g -gdwarf-4 -O0")
set(CMAKE_CXX_FLAGS_DEBUG "-g -gdwarf-4 -O0")
set(CMAKE_ASM_FLAGS_DEBUG "-g -gdwarf-4")

# Require sysroot via env.
if(DEFINED ENV{XINO_SYSROOT} AND NOT "$ENV{XINO_SYSROOT}" STREQUAL "")
  set(_XINO_SYSROOT "$ENV{XINO_SYSROOT}")
else()
  message(FATAL_ERROR
    "ENV{XINO_SYSROOT} must be set.")
endif()

if(NOT IS_DIRECTORY "${_XINO_SYSROOT}")
  message(FATAL_ERROR
    "ENV{XINO_SYSROOT}='${_XINO_SYSROOT}' is not a directory.")
endif()

# Force into cache so it can not be overridden.
set(CMAKE_SYSROOT "${_XINO_SYSROOT}" CACHE PATH "" FORCE)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)  # Don't search sysroot for tools.
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)   # Libraries only in sysroot.
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)   # Headers only in sysroot.
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)   # Packages only in sysroot.

# try_compile() should not attempt to run or link against host glibc etc.
# Building a STATIC lib during try_compile() keeps it simple.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
