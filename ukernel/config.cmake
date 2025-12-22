# ukernel/config.cmake

# ukernel configurations.

set(UKERNEL_SMP FALSE CACHE BOOL "Multicore uKernel")

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

# Platform:

set(UKERNEL_PLATFORM "rock5b" CACHE STRING "Target platform")
set_property(CACHE UKERNEL_PLATFORM PROPERTY STRINGS rock5b)

# Platform configutaions

# Platform == Rock 5B+
if(UKERNEL_PLATFORM STREQUAL "rock5b")
  set(UKERNEL_BASE "0xa00000" CACHE STRING "uKernel base")
  set(UKERNEL_STACK_SIZE "0x4000" CACHE STRING "Stack size (16-byte aligned)")
  #  -- Drivers --
  set(UKERNEL_UART_DRIVER  "DW_APB" CACHE INTERNAL "UART driver" FORCE)
  set(UKERNEL_UART_BASE "0xfeb50000" CACHE INTERNAL "UART base" FORCE)
else()
  message(FATAL_ERROR "Unknown UKERNEL_PLATFORM='${UKERNEL_PLATFORM}'")
endif()
