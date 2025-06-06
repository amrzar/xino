# CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

project(xino
    VERSION 0.1.0
    DESCRIPTION "xino uKernel"
    LANGUAGES C CXX ASM
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Configurations:

# NONE.

# Compiler common flags
set(UKERNEL_COMMON_OPTIONS
    -ffreestanding
    -fdata-sections
    -ffunction-sections
    -fno-stack-protector
)

add_subdirectory(c_shim)
add_subdirectory(ukernel)

# Generate ''config.h''.
configure_file(
    ${CMAKE_SOURCE_DIR}/config.h.in
    ${CMAKE_BINARY_DIR}/include/config.h
    @ONLY
)

# Formatting target.
find_program(CLANG_FORMAT_EXE NAMES clang-format)
if (CLANG_FORMAT_EXE)
    file(GLOB_RECURSE ALL_SOURCES
        ${CMAKE_SOURCE_DIR}/c_shim/src/*.[ch]*
        ${CMAKE_SOURCE_DIR}/c_shim/include/*.[h]*
        ${CMAKE_SOURCE_DIR}/ukernel/src/*.[ch]*
        ${CMAKE_SOURCE_DIR}/ukernel/host_tools/src/*.[ch]*
        ${CMAKE_SOURCE_DIR}/ukernel/include/*.[h]*
    )
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i ${ALL_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}   # for .clang-format if existed.
        VERBATIM
    )
endif()
