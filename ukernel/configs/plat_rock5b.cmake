# ukernel/configs/plat_rock5b.cmake

set(UKERNEL_BASE "0xa00000" CACHE STRING "uKernel base")
set(UKERNEL_STACK_SIZE "0x4000" CACHE STRING "Stack size (16-byte aligned)")

set(UKERNEL_UART_DRIVER  "DW_APB" CACHE INTERNAL "UART driver" FORCE)
set(UKERNEL_UART_BASE "0xfeb50000" CACHE INTERNAL "UART base" FORCE)
