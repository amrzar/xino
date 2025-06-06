# efi/CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

project(xino_efi
    VERSION 0.1.0
    DESCRIPTION "efi"
    LANGUAGES C ASM
)

find_program(GZIP gzip REQUIRED)
find_program(NM nm REQUIRED)

# In ${UKERNEL_ELF}, we expect two symbols:
#  - ''__ukernel_size'' is size of image including BSS
#  - ''__ukernel_code_size'' is size of code

set(UKERNEL_ELF ${CMAKE_BINARY_DIR}/ukernel.elf)

# Generate ''ukernel.bin'' and pad for BSS section.
set(UKERNEL_BIN ${CMAKE_CURRENT_BINARY_DIR}/ukernel.bin)

add_custom_command(
    OUTPUT ${UKERNEL_BIN}
    COMMAND ${CMAKE_OBJCOPY}
        -O binary
        ${UKERNEL_ELF}
        ${UKERNEL_BIN}
    COMMAND /bin/sh -c
        "truncate -s \$(${NM} --radix=d ${UKERNEL_ELF} | \
            grep __ukernel_size | cut -d' ' -f1) \
                ${UKERNEL_BIN}"
    DEPENDS ${UKERNEL_ELF}
    VERBATIM
)

# Generate '' ukernel.gz ''.
set(UKERNEL_GZ ${CMAKE_CURRENT_BINARY_DIR}/ukernel.gz)

add_custom_command(
    OUTPUT ${UKERNEL_GZ}
    COMMAND ${GZIP} -cfn ${UKERNEL_BIN} > ${UKERNEL_GZ}
    DEPENDS ${UKERNEL_BIN}
)

# Generate '' ukernel.gz.o ''.
set(UKERNEL_GZ_OBJ ${CMAKE_CURRENT_BINARY_DIR}/ukernel.gz.o)

# EXPECTED ${UKERNEL_GZ_OBJ}
#
# Symbol table '.symtab' contains 5 entries:
#    Num:    Value          Size Type    Bind   Vis      Ndx Name
#      0: 0000000000000000     0 NOTYPE  LOCAL  DEFAULT  UND 
#      1: 0000000000000000     0 NOTYPE  GLOBAL DEFAULT    3 _binary__[...]
#      2: 0000000000000000     0 NOTYPE  GLOBAL DEFAULT    3 _binary__[...]
#      3: 0000000000000000     0 NOTYPE  GLOBAL DEFAULT  ABS _binary__[...]
#      4: 0000000000000000     0 NOTYPE  GLOBAL DEFAULT  ABS code_size

add_custom_command(
    OUTPUT ${UKERNEL_GZ_OBJ}
    COMMAND /bin/sh -c
        "${CMAKE_OBJCOPY} -I binary -O elf64-littleaarch64 \
            ${UKERNEL_GZ} ${UKERNEL_GZ_OBJ} \
            --add-symbol code_size=0x\$(${NM} ${UKERNEL_ELF} | \
                grep __ukernel_code_size | cut -d' ' -f1) \
            --rename-section .data=.gz..data,load,alloc,readonly,contents"
    DEPENDS ${UKERNEL_GZ} ${UKERNEL_ELF}
    VERBATIM
)

# Generate '' linker.lds ''.
set(EFI_LDS_IN ${CMAKE_CURRENT_SOURCE_DIR}/efi_linker.lds.in)
set(EFI_LDS_OUT ${CMAKE_CURRENT_BINARY_DIR}/efi_linker.lds)

add_custom_command(
  OUTPUT ${EFI_LDS_OUT}
  COMMAND ${CMAKE_C_COMPILER}
    -E -P -xc ${EFI_LDS_IN}
    -o ${EFI_LDS_OUT}
  DEPENDS ${EFI_LDS_IN}
)

add_custom_target(gen_efi_linker_script DEPENDS ${EFI_LDS_OUT})

# Extra options

set(EFI_COMMON_OPTIONS
    ${UKERNEL_COMMON_OPTIONS}
    -O2     # So clang does not spit c functions
)

set(EFI_LINKER_OPTIONS
    -nostdlib
    -static
    -Wl,--gc-sections
    -T${EFI_LDS_OUT}
    -fuse-ld=lld
)

# Source files.
file(GLOB_RECURSE EFI_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*.S
)

# Executable target.
add_executable(efi_app
  ${EFI_SOURCES}
  ${UKERNEL_GZ_OBJ}
)

add_dependencies(efi_app gen_efi_linker_script)

target_include_directories(efi_app PRIVATE
    ${CMAKE_BINARY_DIR}/include     # for the generated config.h
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(efi_app PRIVATE
    ${EFI_COMMON_OPTIONS}
)

target_link_options(efi_app PRIVATE
    ${EFI_LINKER_OPTIONS}
)
