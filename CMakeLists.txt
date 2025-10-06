# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

project(xino
  VERSION 0.1.0
  DESCRIPTION "xino uKernel"
  LANGUAGES C CXX ASM
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF) # no gnu99; keep it portable C11.

# Configurations:

# NONE.

add_library(xino_build_flags INTERFACE)

# Shared compile flags.
target_compile_options(xino_build_flags INTERFACE
  -ffreestanding
  -fdata-sections
  -ffunction-sections
  -fno-stack-protector
  -mgeneral-regs-only
)

# Sub-directories; should be before configure_file().
add_subdirectory(c_shim)
add_subdirectory(ukernel)

configure_file(
  "${CMAKE_SOURCE_DIR}/config.h.in"
  "${CMAKE_BINARY_DIR}/include/config.h"
  @ONLY
)

# cmake --build ... --target style.
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if (CLANG_FORMAT_EXE)
  add_custom_target(style
    COMMAND find c_shim ukernel
      # Format (*.h, *.c, *.cpp, and *.hpp files).
      -regextype posix-extended -type f -regex ".*\.(c|h|cpp|hpp)$"
      -exec "${CLANG_FORMAT_EXE}" -i {} +
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    VERBATIM
  )
endif()
