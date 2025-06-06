# ukernel/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

project(xino_ukernel
    VERSION 0.1.0
    DESCRIPTION "uKernel"
    LANGUAGES C CXX ASM
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate '' linker.lds ''.
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

add_dependencies(ukernel.elf gen_ukernel_linker_script)

target_include_directories(ukernel.elf PRIVATE
    ${CMAKE_BINARY_DIR}/include     # for the generated config.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include
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

# Copy final '' ukernel.ef '' to build directory.
add_custom_command(TARGET ukernel.elf POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:ukernel.elf>
        ${CMAKE_BINARY_DIR}/ukernel.elf
)
