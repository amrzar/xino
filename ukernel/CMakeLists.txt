# ukernel/CMakeLists.txt

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Configurations (ukernel and platform).
include(config.cmake)

# Generate linker script.
set(UKERNEL_LDS_IN "${CMAKE_CURRENT_SOURCE_DIR}/linker.ldspp")
set(UKERNEL_LDS_OUT "${CMAKE_CURRENT_BINARY_DIR}/linker.lds")

add_custom_command(
  OUTPUT "${UKERNEL_LDS_OUT}"
  COMMAND "${CMAKE_C_COMPILER}"
    -E -P -xc "${UKERNEL_LDS_IN}"
    -o "${UKERNEL_LDS_OUT}"
    -I "${CMAKE_BINARY_DIR}/include"  # for generated config.h (from top-level).
  DEPENDS "${UKERNEL_LDS_IN}"
)

add_custom_target(gen_ukernel_linker_script DEPENDS "${UKERNEL_LDS_OUT}")

# Code-gen codes.
set(UKERNEL_REGTOOL "${CMAKE_CURRENT_SOURCE_DIR}/../tools/xino_regtool.py")

set(UKERNEL_SYSREGHPP "${CMAKE_BINARY_DIR}/include/sysregs.hpp")

add_custom_command(
  OUTPUT "${UKERNEL_SYSREGHPP}"
  COMMAND python3 "${UKERNEL_REGTOOL}"
    -i "${CMAKE_CURRENT_SOURCE_DIR}/spec_sysregs"
    -o "${UKERNEL_SYSREGHPP}"
  DEPENDS "${UKERNEL_REGTOOL}" "${CMAKE_CURRENT_SOURCE_DIR}/spec_sysregs"
  VERBATIM
)

add_custom_target(gen_ukernel_codegen DEPENDS "${UKERNEL_SYSREGHPP}")

# Source files.
file(GLOB_RECURSE UKERNEL_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.S"
)

# Executable target.
add_executable(ukernel ${UKERNEL_SOURCES})

# Set the output name "ukernel.elf".
set_target_properties(ukernel PROPERTIES
  OUTPUT_NAME "ukernel"
  SUFFIX ".elf"
)

add_dependencies(ukernel gen_ukernel_codegen gen_ukernel_linker_script)

# Ensure to relink when ${UKERNEL_LDS_IN} changes.
set_property(TARGET ukernel APPEND PROPERTY LINK_DEPENDS "${UKERNEL_LDS_OUT}")

target_include_directories(ukernel PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_BINARY_DIR}/include"       # for generated config.h (from top-level).
  "${CMAKE_BINARY_DIR}/ukernel/include"
)

# Inherit common compiler flags.
target_link_libraries(ukernel PRIVATE
  xino_build_flags
)

target_compile_options(ukernel PRIVATE
  -fpie
  $<$<COMPILE_LANGUAGE:CXX>:-fexceptions;-frtti; -fno-threadsafe-statics>
)

target_link_options(ukernel PRIVATE
  -fuse-ld=lld
  -nostdlib
  -static
  # Pure linker options.
  LINKER:--pie
  LINKER:--eh-frame-hdr
  LINKER:-T,${UKERNEL_LDS_OUT}
  LINKER:--gc-sections
  LINKER:--no-undefined
  LINKER:--orphan-handling=error
  LINKER:--Map=${CMAKE_CURRENT_BINARY_DIR}/ukernel.map
)

# Runtime libraries.
find_library(CPP_LIB c++ REQUIRED)
find_library(CPPABI_LIB c++abi REQUIRED)
find_library(UNWIND_LIB unwind REQUIRED)
find_library(BUILTINS_LIB clang_rt.builtins-aarch64 REQUIRED)

target_link_libraries(ukernel PRIVATE
  c_shim
  ${CPP_LIB}
  ${CPPABI_LIB}
  ${UNWIND_LIB}
  ${BUILTINS_LIB}
)

set(XINO_BIN "${CMAKE_CURRENT_BINARY_DIR}/xino.bin")

add_custom_command(
  TARGET ukernel POST_BUILD
  COMMAND "${CMAKE_OBJCOPY}"
    -O binary "$<TARGET_FILE:ukernel>" "${XINO_BIN}"
  VERBATIM
)

# Install both the ukernel.elf and the xino.bin.
install(TARGETS ukernel RUNTIME DESTINATION bin)
install(FILES "${XINO_BIN}" DESTINATION bin)
