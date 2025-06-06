/**
 * @file pe.h
 * @brief PE/COFF constants for EFI image generation on AArch64.
 *
 * This header defines numeric constants used for constructing a valid PE/COFF
 * header compatible with UEFI firmware. These constants cover DOS headers,
 * PE magic values, machine types, COFF header flags, subsystem types, and
 * section characteristics for `.text` and `.data`.
 *
 * @see Microsoft PE/COFF Specification
 * @see UEFI Specification (Sec. 2.1.1)
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#ifndef __PE_H__
#define __PE_H__

/**
 * @def XINO_EFISTUB_MAJOR_VERSION
 * @brief Major version of the XINO EFI stub.
 */
#define XINO_EFISTUB_MAJOR_VERSION 0x1

/**
 * @def XINO_EFISTUB_MINOR_VERSION
 * @brief Minor version of the XINO EFI stub.
 */
#define XINO_EFISTUB_MINOR_VERSION 0x0

/**
 * @def XINO_PE_MAGIC
 * @brief Magic number placed in the DOS header to identify XINO EFI images.
 *
 * ASCII for "XINO" (0x58494e4f).
 */
#define XINO_PE_MAGIC 0x58494e4f

/**
 * @def MZ_MAGIC
 * @brief DOS header signature ('MZ').
 */
#define MZ_MAGIC 0x5a4d

/**
 * @def PE_MAGIC
 * @brief PE signature ('PE\0\0').
 */
#define PE_MAGIC 0x00004550

/**
 * @def PE_OPT_MAGIC_PE32PLUS
 * @brief Optional header magic for PE32+ format.
 */
#define PE_OPT_MAGIC_PE32PLUS 0x020b

/* --- Machine Types --- */

/**
 * @def IMAGE_FILE_MACHINE_ARM64
 * @brief Machine type for AArch64/ARM64.
 */
#define IMAGE_FILE_MACHINE_ARM64 0xaa64

/* --- COFF Header Characteristics --- */

/**
 * @def IMAGE_FILE_DEBUG_STRIPPED
 * @brief Indicates debugging info has been removed.
 */
#define IMAGE_FILE_DEBUG_STRIPPED 0x0200

/**
 * @def IMAGE_FILE_EXECUTABLE_IMAGE
 * @brief Marks the file as an executable image.
 */
#define IMAGE_FILE_EXECUTABLE_IMAGE 0x0002

/**
 * @def IMAGE_FILE_LINE_NUMS_STRIPPED
 * @brief Line number info is stripped (deprecated).
 */
#define IMAGE_FILE_LINE_NUMS_STRIPPED 0x0004

/* --- Subsystem --- */

/**
 * @def IMAGE_SUBSYSTEM_EFI_APPLICATION
 * @brief Subsystem type for EFI applications.
 */
#define IMAGE_SUBSYSTEM_EFI_APPLICATION 10

/* --- DLL Characteristics --- */

/**
 * @def IMAGE_DLL_CHARACTERISTICS_NX_COMPAT
 * @brief Image supports DEP (No-Execute).
 */
#define IMAGE_DLL_CHARACTERISTICS_NX_COMPAT 0x0100

/* --- Section Characteristics --- */

/**
 * @def IMAGE_SCN_CNT_INITIALIZED_DATA
 * @brief Section contains initialized data.
 */
#define IMAGE_SCN_CNT_INITIALIZED_DATA 0x00000040

/**
 * @def IMAGE_SCN_CNT_CODE
 * @brief Section contains executable code.
 */
#define IMAGE_SCN_CNT_CODE 0x00000020

/**
 * @def IMAGE_SCN_MEM_READ
 * @brief Section is readable.
 */
#define IMAGE_SCN_MEM_READ 0x40000000

/**
 * @def IMAGE_SCN_MEM_EXECUTE
 * @brief Section is executable.
 */
#define IMAGE_SCN_MEM_EXECUTE 0x20000000

/**
 * @def IMAGE_SCN_MEM_WRITE
 * @brief Section is writable.
 */
#define IMAGE_SCN_MEM_WRITE 0x80000000

#endif /* __PE_H__ */
