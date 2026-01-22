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

set(UKERNEL_KIMAGE_SLOT_SIZE "0x20000000" CACHE INTERNAL
  "uKernel mapping size" FORCE) # 512 Mb.

set(UKERNEL_DEVMAP_SLOT_SIZE "0x4000000" CACHE INTERNAL
  "Device mapping size" FORCE)  # 64 Mb.

set(UKERNEL_BOOT_HEAP_SIZE "0x2000000" CACHE INTERNAL
  "uKernel heap used during boot" FORCE) # 32 Mb.

# Platform:

set(UKERNEL_PLATFORM "rock5b" CACHE STRING "Target platform")
set_property(CACHE UKERNEL_PLATFORM PROPERTY STRINGS rock5b)

# Platform configurations

# Platform == Rock 5B+
if(UKERNEL_PLATFORM STREQUAL "rock5b")
  set(UKERNEL_BASE "0xa00000UL" CACHE STRING "uKernel base")
  set(UKERNEL_STACK_SIZE "0x4000" CACHE STRING
    "Stack size (16-byte aligned)") # 16 KB.
  #  -- Drivers --
  set(UKERNEL_UART_DRIVER  "DW_APB" CACHE INTERNAL "UART driver" FORCE)
  set(UKERNEL_UART_BASE "0xfeb50000UL" CACHE INTERNAL "UART base" FORCE)
else()
  message(FATAL_ERROR "Unknown UKERNEL_PLATFORM='${UKERNEL_PLATFORM}'")
endif()

# CPU configurations

set(UKERNEL_CACHE_LINE "64" CACHE STRING "")
