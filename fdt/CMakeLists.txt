# fdt/CMakeLists.txt

# Configurations:

# Source files.
file(GLOB_RECURSE FDT_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_library(fdt STATIC)

target_sources(fdt PRIVATE ${FDT_SOURCES})

# Build a PIC fdt.
set_target_properties(fdt PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

# Expose fdt headers.
target_include_directories(fdt PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_include_directories(fdt PRIVATE
  "${CMAKE_BINARY_DIR}/include"   # for generated config.h (from top-level).
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_definitions(fdt PRIVATE
  _POSIX_SOURCE
)

# Inherit common compiler flags.
target_link_libraries(fdt PRIVATE
  xino_build_flags
)
