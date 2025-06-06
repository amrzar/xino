# ukernel/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

project(xino_ukernel
    VERSION 0.1.0
    DESCRIPTION "uKernel"
    LANGUAGES C CXX ASM
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configurations:

# NONE.

include(ExternalProject)

# Build host's tools.
ExternalProject_Add(host_tools
    PREFIX ${CMAKE_CURRENT_BINARY_DIR}
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/host_tools
    CMAKE_ARGS -G "${CMAKE_GENERATOR}"
        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    INSTALL_COMMAND ""
    BUILD_ALWAYS 1
)

ExternalProject_Get_Property(host_tools BINARY_DIR)

# Generate ''linker.lds''.
set(UKERNEL_LDS_IN ${CMAKE_CURRENT_SOURCE_DIR}/linker.lds.in)
set(UKERNEL_LDS_OUT ${CMAKE_CURRENT_BINARY_DIR}/linker.lds)

add_custom_command(
    OUTPUT ${UKERNEL_LDS_OUT}
    COMMAND ${CMAKE_C_COMPILER}
        -E -P -xc ${UKERNEL_LDS_IN}
        -o ${UKERNEL_LDS_OUT}
    DEPENDS ${UKERNEL_LDS_IN}
)

add_custom_target(gen_ukernel_linker_script DEPENDS ${UKERNEL_LDS_OUT})

# Generate ''sysreg_gen.h''
set(SYSREG_IN ${CMAKE_CURRENT_SOURCE_DIR}/spec/sysreg)
set(SYSREG_OUT ${CMAKE_BINARY_DIR}/ukernel/include/sysreg_gen.h)

add_custom_command(
    OUTPUT ${SYSREG_OUT}
    COMMAND ${BINARY_DIR}/sysreg_gen
        --spec ${SYSREG_IN}
        --out ${SYSREG_OUT}
        --type ${CMAKE_BUILD_TYPE}
    DEPENDS host_tools ${SYSREG_IN}
)

add_custom_target(auto_gen DEPENDS ${SYSREG_OUT})

# Extra options

set(UKERNEL_CXX_OPTIONS
    -fexceptions 
    -frtti
)

set(UKERNEL_LINKER_OPTIONS
    -nostdlib
    -static
    -Wl,--gc-sections
    -T${UKERNEL_LDS_OUT}
    -fuse-ld=lld
)

# Source files.
file(GLOB_RECURSE UKERNEL_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.S
)

# Executable target.
add_executable(ukernel.elf
    ${UKERNEL_SOURCES}
)

add_dependencies(ukernel.elf auto_gen gen_ukernel_linker_script)

target_include_directories(ukernel.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/include             # for the generated config.h
    ${CMAKE_BINARY_DIR}/ukernel/include     # for auto-generated files.
)

target_compile_options(ukernel.elf PRIVATE
    ${UKERNEL_COMMON_OPTIONS}
    $<$<COMPILE_LANGUAGE:CXX>:${UKERNEL_CXX_OPTIONS}>
)

target_link_options(ukernel.elf PRIVATE
    ${UKERNEL_LINKER_OPTIONS}
)

# Runtime libraries.
find_library(CPP_LIB c++ REQUIRED)
find_library(CPPABI_LIB c++abi REQUIRED)
find_library(UNWIND_LIB unwind REQUIRED)
find_library(BUILTINS_LIB clang_rt.builtins-aarch64 REQUIRED)

target_link_libraries(ukernel.elf PRIVATE
    c_shim
    ${CPP_LIB}
    ${CPPABI_LIB}
    ${UNWIND_LIB}
    ${BUILTINS_LIB}
)

install(TARGETS ukernel.elf RUNTIME DESTINATION bin)
