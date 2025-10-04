# ukernel/CMakeLists.txt

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Configurations:

# NONE.

# Generate linker script.
set(UKERNEL_LDS_IN "${CMAKE_CURRENT_SOURCE_DIR}/linker.lds.in")
set(UKERNEL_LDS_OUT "${CMAKE_CURRENT_BINARY_DIR}/linker.lds")

add_custom_command(
  OUTPUT "${UKERNEL_LDS_OUT}"
  COMMAND "${CMAKE_C_COMPILER}"
    -E -P -xc "${UKERNEL_LDS_IN}"
    -o "${UKERNEL_LDS_OUT}"
  DEPENDS "${UKERNEL_LDS_IN}"
)

add_custom_target(gen_ukernel_linker_script DEPENDS "${UKERNEL_LDS_OUT}")

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

add_dependencies(ukernel gen_ukernel_linker_script)
# Ensure to relink when it changes.
set_property(TARGET ukernel APPEND PROPERTY LINK_DEPENDS "${UKERNEL_LDS_OUT}")

target_include_directories(ukernel PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  "${CMAKE_BINARY_DIR}/include"   # for generated config.h (from top-level).
  "${CMAKE_BINARY_DIR}/ukernel/include"
)

# Inherit common compiler flags.
target_link_libraries(ukernel PRIVATE
  xino_build_flags
)

target_compile_options(ukernel PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>:-fexceptions;-frtti>
)

target_link_options(ukernel PRIVATE
  -nostdlib
  -static
  -fuse-ld=lld
  LINKER:-T,${UKERNEL_LDS_OUT}
  LINKER:--gc-sections
  LINKER:-Map,${CMAKE_CURRENT_BINARY_DIR}/ukernel.map
  LINKER:--no-undefined
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

install(TARGETS ukernel RUNTIME DESTINATION bin)
