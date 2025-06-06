# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

project(xino
    VERSION 0.1.0
    DESCRIPTION "xino uKernel"
    LANGUAGES C CXX ASM
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(XINO_ENABLE_DEBUG "Enable debug symbols (-g)" OFF)
set(XINO_HEAP_SIZE 0x10000 CACHE STRING "xino heap size")

# Generate '' config.h ''.
configure_file(
    ${CMAKE_SOURCE_DIR}/config.h.in
    ${CMAKE_BINARY_DIR}/include/config.h
    @ONLY
)

# Compiler common flags
set(UKERNEL_COMMON_OPTIONS
    -ffreestanding
    -fdata-sections
    -ffunction-sections
    -fno-stack-protector
)

# Conditional flags
if(XINO_ENABLE_DEBUG)
    list(APPEND UKERNEL_COMMON_OPTIONS -g)
endif()

add_subdirectory(c_shim)
add_subdirectory(ukernel)

# Formatting target.
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if (CLANG_FORMAT_EXE)
    file(GLOB_RECURSE ALL_SOURCES
        ${CMAKE_SOURCE_DIR}/c_shim/src/*.[ch]*
        ${CMAKE_SOURCE_DIR}/c_shim/include/*.[h]*
        ${CMAKE_SOURCE_DIR}/ukernel/src/*.[ch]*
        ${CMAKE_SOURCE_DIR}/ukernel/include/*.[h]*
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}   # for .clang-format if existed.
        VERBATIM
    )
endif()
