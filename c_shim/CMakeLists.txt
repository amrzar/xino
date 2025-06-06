# c_shim/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

project(c_shim
    VERSION 0.1.0
    DESCRIPTION "c_shim"
    LANGUAGES C
)

# Source files.
file(GLOB_RECURSE SHIM_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
)

add_library(c_shim STATIC
    ${SHIM_SOURCES}
)

# Expose c_shim headers.
target_include_directories(c_shim PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(c_shim PRIVATE
    ${CMAKE_BINARY_DIR}/include     # for the generated config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(c_shim PRIVATE
    ${UKERNEL_COMMON_OPTIONS}
)
