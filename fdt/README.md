
# DTB in xino

For read-only parsing (find nodes, read properties, iterate), the smallest practical subset from [dtc/libfdt/](ttps://github.com/dgibson/dtc/tree/main/libfdt) is:

| File | Purpose |
| --- | --- |
| fdt.c | Core header checks, structure walking helpers, etc. |
| fdt_ro.c | Read-only API: `fdt_path_offset()`, `fdt_getprop()`, `fdt_next_node()`, etc. |
| libfdt.h | Public API header. |
| fdt.h | DTB format definitions. |
| libfdt_env.h | Porting layer (types + endianness + libc hooks). |
| libfdt_internal.h | Private header needed to build the `.c` files |
