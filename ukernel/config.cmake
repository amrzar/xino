# ukernel/config.cmake

# ukernel configurations.

set(UKERNEL_PROFILE "standalone" CACHE STRING
  "Kernel profile: standalone (4K + 39-bit VA) or embedded (16K + 36-bit VA)")
set_property(CACHE UKERNEL_PROFILE PROPERTY STRINGS standalone embedded)

if(UKERNEL_PROFILE STREQUAL "embedded")
  set(UKERNEL_PAGE_16K TRUE CACHE INTERNAL "Use 16K pages" FORCE)
  set(UKERNEL_VA_BITS "36" CACHE INTERNAL "" FORCE)
else() # Default is standalone.
  set(UKERNEL_PAGE_4K TRUE CACHE INTERNAL "Use 4K pages" FORCE)
  set(UKERNEL_VA_BITS "39" CACHE INTERNAL "" FORCE)
endif()

set(UKERNEL_BASE "0x40200000" CACHE STRING "uKernel base")
set(UKERNEL_STACK_SIZE "0x4000" CACHE STRING "Stack size (16-byte aligned)")

# Platform:

set(UKERNEL_PLATFORM "qemu" CACHE STRING "Target platform")
set_property(CACHE UKERNEL_PLATFORM PROPERTY STRINGS qemu rock5b)

# Platform configutaions

# Platform == QEMU
if(UKERNEL_PLATFORM STREQUAL "qemu")
include(configs/plat_qemu.cmake)
# Platform == Rock 5B+
elseif(UKERNEL_PLATFORM STREQUAL "rock5b")
include(configs/plat_rock5b.cmake)
else()
  message(FATAL_ERROR "Unknown UKERNEL_PLATFORM='${UKERNEL_PLATFORM}'")
endif()
