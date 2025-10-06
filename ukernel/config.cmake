# ukernel/config.cmake

set(UKERNEL_PROFILE "standalone" CACHE STRING
  "Kernel profile: standalone or embedded")
set_property(CACHE UKERNEL_PROFILE PROPERTY STRINGS standalone embedded)

if(UKERNEL_PROFILE STREQUAL "embedded")
  set(UKERNEL_PAGE_16K TRUE CACHE BOOL "Use 16K pages" FORCE)
  set(UKERNEL_VA_BITS "36" CACHE STRING "" FORCE)
else() # Default is standalone.
  set(UKERNEL_PAGE_4K TRUE CACHE BOOL "Use 4K pages" FORCE)
  set(UKERNEL_VA_BITS "39" CACHE STRING "" FORCE)
endif()

set(UKERNEL_BASE "0x40200000" CACHE STRING "uKernel base")
set(UKERNEL_STACK_SIZE "0x4000" CACHE STRING
  "Stack size (16-bytes aligned, hex or decimal)")

# Hardware:

set(UKERNEL_UART_BASE "0x9000000" CACHE STRING "UART MMIO base")
set(UKERNEL_UART_DRIVER "PL011" CACHE STRING "UART driver: PL011")
set_property(CACHE UKERNEL_UART_DRIVER PROPERTY STRINGS PL011)
# Validate the UART driver.
if(NOT UKERNEL_UART_DRIVER STREQUAL "PL011")
  message(FATAL_ERROR "Unknown UKERNEL_UART_DRIVER='${UKERNEL_UART_DRIVER}'.")
endif()
