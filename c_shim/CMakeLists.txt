# c_shim/CMakeLists.txt

# Configurations:

# Source files.
file(GLOB_RECURSE C_SHIM_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

add_library(c_shim STATIC)

target_sources(c_shim PRIVATE ${C_SHIM_SOURCES})

# Build a PIC c_shim.
set_target_properties(c_shim PROPERTIES
  POSITION_INDEPENDENT_CODE ON
)

# Expose c_shim headers.
target_include_directories(c_shim PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_include_directories(c_shim PRIVATE
  "${CMAKE_BINARY_DIR}/include"   # for generated config.h (from top-level).
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Inherit common compiler flags.
target_link_libraries(c_shim PRIVATE
  xino_build_flags
)
