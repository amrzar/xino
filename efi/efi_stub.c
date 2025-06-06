/**
 * @file efi_stub.h
 * @brief UEFI core types and boot services interface for aarch64.
 *
 * Provides typedefs and function pointer types for key UEFI boot services,
 * memory descriptors, status codes, and standard types, as described in the
 * UEFI Specification.
 *
 * @see https://uefi.org/specs/UEFI/2.10/
 * @see ARM Cortex-A Series Version: 1.0. Programmer's Guide for ARMv8-A.
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#include <config.h> // for MIN_UKERNEL_ALIGN
#include <stddef.h>
#include <stdint.h>

#define BITS_PER_LONG 64

/**
 * @def EFIAPI
 * @brief Calling convention attribute for UEFI functions (MS ABI).
 */
#define EFIAPI __attribute__((ms_abi))

/**
 * @def EFI_PAGE_SIZE
 * @brief UEFI page size (4 KiB).
 */
#define EFI_PAGE_SIZE 0x1000

#define EFI_GET_MEMORY_MAP_SLACK_SLOTS 8

/**
 * @name EFI Data Types
 * @{
 * Define types to match UEFI Spec.
 * @see https://uefi.org/specs/UEFI/2.10/02_Overview.html#data-types
 */

/**
 * @typedef UINTN
 * @brief Unsigned integer of native width (64-bit on AArch64).
 */
typedef uint64_t UINTN;

/**
 * @typedef UINT8
 * @brief 8-bit unsigned integer.
 */
typedef uint8_t UINT8;

/**
 * @typedef UINT16
 * @brief 16-bit unsigned integer.
 */
typedef uint16_t UINT16;

/**
 * @typedef UINT32
 * @brief 32-bit unsigned integer.
 */
typedef uint32_t UINT32;

/**
 * @typedef UINT64
 * @brief 64-bit unsigned integer.
 */
typedef uint64_t UINT64;

/**
 * @typedef CHAR16
 * @brief 16-bit (UTF-16) character.
 */
typedef uint16_t CHAR16;

/**
 * @typedef VOID
 * @brief Void type, to match UEFI Spec convention.
 */
typedef void VOID;

/**
 * @struct EFI_GUID
 * @brief 128-bit globally unique identifier.
 */
typedef struct {
  UINT32 a;
  UINT16 b, c;
  UINT8 d[8];
} EFI_GUID;

/**
 * @typedef EFI_STATUS
 * @brief UEFI function return type.
 */
typedef UINTN EFI_STATUS;

/**
 * @typedef EFI_HANDLE
 * @brief Opaque handle to a UEFI object.
 */
typedef VOID *EFI_HANDLE;

/** @} */

/**
 * @name EFI Status Codes
 * @{
 * @see https://uefi.org/specs/UEFI/2.10/Apx_D_Status_Codes.html.
 */

/**
 * @def EFI_SUCCESS
 * @brief UEFI operation successful.
 */
#define EFI_SUCCESS 0

/**
 * @def EFI_LOAD_ERROR
 * @brief UEFI load error.
 */
#define EFI_LOAD_ERROR (1 | 1UL << (BITS_PER_LONG - 1))

/**
 * @def EFI_INVALID_PARAMETER
 * @brief UEFI invalid parameter error.
 */
#define EFI_INVALID_PARAMETER (2 | 1UL << (BITS_PER_LONG - 1))

/**
 * @def EFI_BUFFER_TOO_SMALL
 * @brief UEFI buffer too small error.
 */
#define EFI_BUFFER_TOO_SMALL (5 | 1UL << (BITS_PER_LONG - 1))

/** @} */

/**
 * @name EFI Boot Services
 * @{
 * @see https://uefi.org/specs/UEFI/2.10/07_Services_Boot_Services.html.
 */

/**
 * @typedef EFI_PHYSICAL_ADDRESS
 * @brief 64-bit physical address used by UEFI.
 */
typedef UINT64 EFI_PHYSICAL_ADDRESS;

/**
 * @enum EFI_ALLOCATE_TYPE
 * @brief Allocation strategies for EFI memory services.
 */
typedef enum {
  ALLOCATE_ANY_PAGES,   /**< Allocate any available pages. */
  ALLOCATE_MAX_ADDRESS, /**< Allocate below a max address. */
  ALLOCATE_ADDRESS,     /**< Allocate at a specific address. */
} EFI_ALLOCATE_TYPE;

/**
 * @enum EFI_MEMORY_TYPE
 * @brief UEFI memory region types.
 */
typedef enum {
  EFI_RESERVED_MEMORY_TYPE, /**< Reserved. */
  EFI_LOADER_CODE,          /**< Loader code. */
  EFI_LOADER_DATA,          /**< Loader data. */
  EFI_BOOT_SERVICES_CODE,   /**< Boot services code. */
  EFI_BOOT_SERVICES_DATA,   /**< Boot services data. */
  /* Others dropped for brevity. */
} EFI_MEMORY_TYPE;

/**
 * @typedef EFI_ALLOCATE_PAGES
 * @brief Function pointer to EFI Boot Services AllocatePages().
 *
 * Allocates memory pages according to the specified type and memory type.
 * On output @p memory is the base address of the allocated page range and on
 * input, usage depends on @p type:
 *
 *   - `ALLOCATE_ANY_PAGES` @p memory is ignored.
 *   - `ALLOCATE_MAX_ADDRESS` @p memory is uppermost address to allocate.
 *   - `ALLOCATE_ADDRESS` @p memory is address to allocate.
 *
 * @param[in] type Allocation type (strategy).
 * @param[in] memory_type Type of memory to allocate.
 * @param[in] pages Number of pages to allocate.
 * @param[in, out] memory Pointer to a physical address.
 *
 * @see Section 7.2.1.
 *
 * @return `EFI_SUCCESS` on success, or a non-zero error code (e.g.
 *         `EFI_INVALID_PARAMETER`) on failure.
 */
typedef EFI_STATUS(EFIAPI *EFI_ALLOCATE_PAGES)(EFI_ALLOCATE_TYPE type,
                                               EFI_MEMORY_TYPE memory_type,
                                               UINTN pages,
                                               EFI_PHYSICAL_ADDRESS *memory);

/**
 * @typedef EFI_FREE_PAGES
 * @brief Function pointer to EFI Boot Services FreePages().
 *
 * @param memory Base physical address of pages to free.
 * @param pages Number of pages to free.
 *
 * @see Section 7.2.2.
 *
 * @return `EFI_SUCCESS` on success, or a non-zero error code (e.g.
 *         `EFI_INVALID_PARAMETER`) on failure.
 */
typedef EFI_STATUS(EFIAPI *EFI_FREE_PAGES)(EFI_PHYSICAL_ADDRESS memory,
                                           UINTN pages);

/**
 * @typedef EFI_VIRTUAL_ADDRESS
 * @brief 64-bit virtual address used by UEFI.
 */
typedef UINT64 EFI_VIRTUAL_ADDRESS;

/**
 * @struct EFI_MEMORY_DESCRIPTOR
 * @brief Describes a memory region in the EFI memory map.
 *
 * Each entry provides information about a contiguous range of physical or
 * virtual memory, its type, size, and attributes.
 *
 * @see Section 7.2.3.
 */
typedef struct {
  /**
   * @brief Type of the memory region as defined by @ref EFI_MEMORY_TYPE.
   */
  UINT32 type;

  /**
   * @brief Physical address of the first byte in the memory region.
   *
   * Must be aligned on a 4 KiB boundary, and not above 0xfffffffffffff000.
   */
  EFI_PHYSICAL_ADDRESS physical_start;

  /**
   * @brief Virtual address of the first byte in the memory region.
   *
   * Must be aligned on a 4 KiB boundary, and not above 0xfffffffffffff000.
   */
  EFI_VIRTUAL_ADDRESS virtual_start;

  /**
   * @brief Number of 4 KiB pages in the memory region.
   *
   * Must not be 0. Must not represent a region with a start address (physical
   * or virtual) above 0xfffffffffffff000.
   */
  UINT64 number_of_pages;

  /**
   * @brief Attributes of the memory region (bit mask).
   *
   * Attributes definitions are dropped for brevity.
   */
  UINT64 attribute;
} EFI_MEMORY_DESCRIPTOR;

/**
 * @typedef EFI_GET_MEMORY_MAP
 * @brief Function pointer to EFI Boot Services GetMemoryMap().
 *
 * On input, @p map_size is the size of the buffer allocated by the caller.
 * On output, it is the size of the buffer returned by the firmware if the
 * buffer was large enough, or the size of the buffer needed to contain the map
 * if the buffer was too small.
 *
 * @param[in, out] memory_map_size Size of the memory map.
 * @param[out] memory_map Pointer to memory map buffer.
 * @param[out] map_key Key for exiting boot services.
 * @param[out] desc_size Size of each descriptor.
 * @param[out] desc_version Descriptor version.
 *
 * @see Section 7.2.3.
 *
 * @return `EFI_SUCCESS` on success, or a non-zero error code (e.g.
 *         `EFI_INVALID_PARAMETER`, `EFI_BUFFER_TOO_SMALL`) on failure.
 */
typedef EFI_STATUS(EFIAPI *EFI_GET_MEMORY_MAP)(
    UINTN *map_size, EFI_MEMORY_DESCRIPTOR *memory_map, UINTN *map_key,
    UINTN *desc_size, UINT32 *desc_version);

/**
 * @typedef EFI_ALLOCATE_POOL
 * @brief Function pointer to EFI Boot Services AllocatePool().
 *
 * @param pool_type Memory type for the pool.
 * @param size Size in bytes to allocate.
 * @param buffer Output pointer to allocated buffer.
 *
 * @see Section 7.2.4.
 *
 * @return `EFI_SUCCESS` on success, or a non-zero error code (e.g.
 *         `EFI_INVALID_PARAMETER`) on failure.
 */
typedef EFI_STATUS(EFIAPI *EFI_ALLOCATE_POOL)(EFI_MEMORY_TYPE pool_type,
                                              UINTN size, VOID **buffer);

/**
 * @typedef EFI_FREE_POOL
 * @brief Function pointer to EFI Boot Services FreePool().
 *
 * @param buffer Buffer to free.
 *
 * @see Section 7.2.5.
 *
 * @return `EFI_SUCCESS` on success, or a non-zero error code (e.g.
 *         `EFI_INVALID_PARAMETER`) on failure.
 */
typedef EFI_STATUS(EFIAPI *EFI_FREE_POOL)(VOID *buffer);

/**
 * @typedef EFI_EXIT_BOOT_SERVICES
 * @brief Function pointer to EFI Boot Services ExitBootServices().
 *
 * @param image_handle Image handle of the application.
 * @param map_key Key from GetMemoryMap.
 *
 * @see Section 7.4.6.
 *
 * @return `EFI_SUCCESS` on success, or a non-zero error code (e.g.
 *         `EFI_INVALID_PARAMETER`) on failure.
 */
typedef EFI_STATUS(EFIAPI *EFI_EXIT_BOOT_SERVICES)(EFI_HANDLE image_handle,
                                                   UINTN map_key);

/** @} */

/**
 * @name EFI System Table
 * @{
 * @see https://uefi.org/specs/UEFI/2.10/04_EFI_System_Table.html.
 */

/**
 * @struct EFI_TABLE_HEADER
 * @brief Standard header for all UEFI tables.
 *
 * Provides signature, versioning, size, and CRC for UEFI tables, including
 * System Table, Boot Services Table, and Runtime Services Table.
 *
 * @see Section 4.2.
 */
typedef struct {
  UINT64 signature;   /**< 64-bit signature that identifies the table type. */
  UINT32 revision;    /**< Revision of the EFI Specification for this table. */
  UINT32 header_size; /**< Size in bytes of the entire table, including
                           `EFI_TABLE_HEADER`. */
  UINT32 crc32;    /**< 32-bit CRC of the entire table for header_size bytes. */
  UINT32 reserved; /**< Reserved; must be set to 0. */
} EFI_TABLE_HEADER;

/**
 * @struct EFI_CONFIGURATION_TABLE
 * @brief Describes a platform configuration table entry.
 *
 * Each entry contains a GUID identifying the table and a pointer to the
 * vendor-specific data. The EFI System Table provides a list of these
 * tables, allowing OS loaders and applications to find firmware or platform
 * specific data structures.
 *
 * @see Section 4.6
 */
typedef struct {
  EFI_GUID guid;
  VOID *table;
} EFI_CONFIGURATION_TABLE;

/**
 * @struct EFI_BOOT_SERVICES
 * @brief UEFI Boot Services table.
 *
 * This structure contains function pointers for all UEFI boot services.
 * Only a subset of members are defined here for brevity.
 *
 * @see Section 4.4.
 */
typedef struct {
  EFI_TABLE_HEADER hdr;

  UINT64 pad1[2];

  /* Memory Services. */
  EFI_ALLOCATE_PAGES allocate_pages; // EFI 1.0+
  EFI_FREE_PAGES free_pages;         // EFI 1.0+
  EFI_GET_MEMORY_MAP get_memory_map; // EFI 1.0+
  EFI_ALLOCATE_POOL allocate_pool;   // EFI 1.0+
  EFI_FREE_POOL free_pool;           // EFI 1.0+

  UINT64 pad2[15];

  /* Image Services. */
  UINT64 pad3[4];
  EFI_EXIT_BOOT_SERVICES exit_boot_services; // EFI 1.0+

  UINT64 pad4[17];
} EFI_BOOT_SERVICES;

/**
 * @struct EFI_SYSTEM_TABLE
 * @brief UEFI System Table.
 *
 * Entry point to UEFI firmware data and services.
 * Only a subset of members are defined here for brevity.
 *
 * @see Section 4.3.
 */
typedef struct {
  EFI_TABLE_HEADER hdr;
  CHAR16 *firmware_vendor;  /**< Vendor string for system firmware. */
  UINT32 firmware_revision; /**< Firmware vendor-specific revision. */
  // UINT32 pad;
  UINT64 pad1[7];
  EFI_BOOT_SERVICES *boot_services;
  UINTN table_entries;
  EFI_CONFIGURATION_TABLE *config_table;
} EFI_SYSTEM_TABLE;

/** @} */

/* ------------------------------------------------------------------------- */
/*                                  Helpers                                  */
/* ------------------------------------------------------------------------- */

static EFI_SYSTEM_TABLE *efi_systab;

/**
 * @brief Start address of the embedded gzipped payload in the binary.
 */
extern unsigned char __efistub__gzdata_start[];

/**
 * @brief End address (one past the last byte) of the embedded gzipped payload.
 */
extern unsigned char __efistub__gzdata_end[];

/**
 * @brief Size of the embedded payload, 32bit unaligned (see efi_linker.lds.in).
 */
extern __attribute__((aligned(1))) uint32_t __efistub_payload_size;

/**
 * @def efi_bs_call
 * @brief Call a UEFI Boot Services function.
 *
 * Example:
 * @code
 * efi_bs_call(allocate_pages, ...);
 * @endcode
 */
#define efi_bs_call(func, ...) efi_systab->boot_services->func(__VA_ARGS__)

#define ALIGN_UP_MASK(x, mask) (((x) + (mask)) & ~(mask))
#define ALIGN_UP(x, a) ALIGN_UP_MASK(x, (__typeof__(x))(a) - 1)

/**
 * @brief Allocate physically aligned UEFI memory pages.
 *
 * Allocates memory pages with the specified size and alignment from the given
 * EFI memory type. If the alignment is larger than the default page size,
 * additional pages are requested to guarantee alignment, and unused leading or
 * trailing pages are freed.
 *
 * @param size The minimum number of bytes to allocate.
 * @param align Required alignment in bytes (minimum: `EFI_PAGE_SIZE`).
 * @param memory_type UEFI memory type to allocate from.
 * @param alloc_addr Output physical address to the allocated buffer.
 *
 * @return `EFI_SUCCESS` on success, or a non-zero error code (e.g.
 *         `EFI_INVALID_PARAMETER`) on failure.
 */
static EFI_STATUS efi_allocate_pages(UINTN size, UINTN align,
                                     EFI_MEMORY_TYPE memory_type,
                                     EFI_PHYSICAL_ADDRESS *alloc_addr) {
  EFI_PHYSICAL_ADDRESS aligned_addr;
  EFI_PHYSICAL_ADDRESS raw_addr;
  EFI_STATUS status;
  UINTN pad_pages;
  UINTN trailing;
  UINTN leading;

  if (align < EFI_PAGE_SIZE)
    align = EFI_PAGE_SIZE;

  size = ALIGN_UP(size, EFI_PAGE_SIZE);
  /* Extra pages to allocate to guarantee we can align. */
  pad_pages = align / EFI_PAGE_SIZE - 1;

  status = efi_bs_call(allocate_pages, ALLOCATE_ANY_PAGES, memory_type,
                       size / EFI_PAGE_SIZE + pad_pages, &raw_addr);
  if (status != EFI_SUCCESS)
    return status;

  /* Align allocated memory: */

  aligned_addr = ALIGN_UP(raw_addr, align);
  /* Free the ''leading'' unused pages. */
  leading = (aligned_addr - raw_addr) / EFI_PAGE_SIZE;
  if (leading)
    efi_bs_call(free_pages, raw_addr, leading);
  /* Free the ''trailing'' unused pages. */
  trailing = pad_pages - leading;
  if (trailing)
    efi_bs_call(free_pages, aligned_addr + size, trailing);

  /* Return address. */
  *alloc_addr = aligned_addr;

  return EFI_SUCCESS;
}

/**
 * @brief Exits UEFI boot services, handing over control to the OS.
 *
 * @param image_handle Image handle passed to EFI entry.
 *
 * @return `EFI_SUCCESS` on success, or a non-zero error code (e.g.
 *         `EFI_INVALID_PARAMETER`, `EFI_LOAD_ERROR`) on failure.
 */
static EFI_STATUS efi_exit_boot_services(EFI_HANDLE image_handle) {
  EFI_MEMORY_DESCRIPTOR *memory_map;
  EFI_STATUS status;
  UINT32 desc_ver;
  UINTN desc_size;
  UINTN map_key;

  UINTN map_size = 0;
  /* Query required buffer size. */
  status = efi_bs_call(get_memory_map, &map_size, NULL, &map_key, &desc_size,
                       &desc_ver);
  if (status != EFI_BUFFER_TOO_SMALL)
    return EFI_LOAD_ERROR;

  /* Allocate a bit of slack for descriptor table growth. */
  UINTN alloc_size = map_size + desc_size * EFI_GET_MEMORY_MAP_SLACK_SLOTS;
  status = efi_bs_call(allocate_pool, EFI_LOADER_DATA, alloc_size,
                       (VOID **)&memory_map);
  if (status != EFI_SUCCESS)
    return status;

  do {
    map_size = alloc_size;
    status = efi_bs_call(get_memory_map, &map_size, memory_map, &map_key,
                         &desc_size, &desc_ver);
    if (status != EFI_SUCCESS)
      break;

    status = efi_bs_call(exit_boot_services, image_handle, map_key);
    /* Check if map changed under our feet - refetch and retry. */
  } while (status == EFI_INVALID_PARAMETER);

  return status;
}

/* Return 1 if `a` is equal `b`. */
static int guid_equals(const EFI_GUID *a, const EFI_GUID *b) {
  int i;
  const uint8_t *pa = (const uint8_t *)a;
  const uint8_t *pb = (const uint8_t *)b;

  for (i = 0; i < sizeof(EFI_GUID); ++i)
    if (pa[i] != pb[i])
      return 0;

  return 1;
}

VOID *efi_get_config_table(EFI_GUID guid) {
  UINTN i;

  for (i = 0; i < efi_systab->table_entries; i++) {
    if (guid_equals(&guid, &efi_systab->config_table[i].guid))
      return efi_systab->config_table[i].table;
  }

  return NULL;
}

/* ------------------------------------------------------------------------- */

/* gzip_decompress.c */
int decompress_gzip(unsigned char *dest, size_t *dest_len,
                    unsigned char *source, size_t source_len);

#define DEVICE_TREE_GUID                                                       \
  ((EFI_GUID){0xb1b621d5, 0xf19c, 0x41a5, 0x83, 0x0b, 0xd9, 0x15, 0x2c, 0x69,  \
              0xaa, 0xe0})

static void clean_cache(unsigned long code_base) {
  extern uint32_t __efistub_code_size;
  size_t line_size, code_size;
  uint64_t ctr;

  /* __efistub_code_size is at least EFI_PAGE_SIZE aligned. */
  code_size = __efistub_code_size;

  /* D13.2.34 CTR_EL0, Cache Type Register. */
  asm volatile("mrs %0, CTR_EL0" : "=r"(ctr));

  /* DminLine, bits [19:16]: Log2 of the number of words (4-bytes). */
  line_size = 4 << (ctr >> 16 & 0xf);

  /* Ignore IDC, bit [28]; always flush to PoU. */

  /* See Example 11-4 Cleaning a line of self-modifying code. */

  do {
    asm("dc cvau, %0" : : "r"(code_base));
    code_base += line_size;
    code_size -= line_size;
  } while (code_size >= line_size);

  asm volatile("dsb ish" : : : "memory");

  asm("ic ialluis");
  asm volatile("dsb ish" : : : "memory");

  /* Synchronize the fetched instruction stream. */
  asm volatile("isb" : : : "memory");
}

/**
 * @brief UEFI image entry point, as required by the UEFI specification.
 *
 * This function is the entry point for UEFI applications and is called by
 * the UEFI firmware.
 *
 *   - Initializes global UEFI system table pointer.
 *   - Allocates and decompresses the ukernel image.
 *   - Locates the device tree table if present.
 *   - Exits boot services, handing control to the ukernel.
 *
 * @param image_handle The UEFI image handle provided by firmware.
 * @param system_table Pointer to the UEFI system table provided by firmware.
 *
 * @see Section 4.1
 *
 * @return `EFI_SUCCESS` on success, or a non-zero error code (e.g.
 *         `EFI_INVALID_PARAMETER`, `EFI_LOAD_ERROR`) on failure.
 */
EFI_STATUS EFIAPI __efistub_efi_entry(EFI_HANDLE image_handle,
                                      EFI_SYSTEM_TABLE *system_table) {
  efi_systab = system_table;

  EFI_PHYSICAL_ADDRESS alloc_addr;
  /* Read size of ukernel. */
  UINTN alloc_size = __efistub_payload_size;
  EFI_STATUS status;
  VOID *fdt;

  status = efi_allocate_pages(alloc_size, MIN_UKERNEL_ALIGN, EFI_LOADER_CODE,
                              &alloc_addr);
  if (status != EFI_SUCCESS)
    return status;

  if (decompress_gzip((unsigned char *)alloc_addr, &alloc_size,
                      (unsigned char *)__efistub__gzdata_start,
                      __efistub__gzdata_end - __efistub__gzdata_start))
    return EFI_LOAD_ERROR;

  clean_cache(alloc_addr);

  fdt = efi_get_config_table(DEVICE_TREE_GUID);

  /* uKernel entry address: */
  void (*__attribute__((noreturn)) uk_entry)(uint64_t) = (void *)alloc_addr;

  status = efi_exit_boot_services(image_handle);
  if (status != EFI_SUCCESS)
    return status;

  uk_entry((uint64_t)fdt);

  return EFI_SUCCESS;
}
