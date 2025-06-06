/**
 * @file pe_header.S
 * @brief Generates a PE/COFF header for EFI stub booting (AArch64).
 *
 * This file constructs a valid MS-DOS + PE/COFF header required for EFI
 * firmware to recognize and load the binary image. It includes a DOS stub,
 * PE header, optional header fields, data directories, and section headers for
 * `.text` and `.data` segments.
 *
 * INSPIRED FROM LINUX Z-BOOT.
 *
 * @see pe.png
 *
 * @author Amirreza Zarrabi
 * @date 2025
 */

#include "pe.h"

    .section ".head", "a"

    .global     __efistub_efi_header
__efistub_efi_header:
.Ldoshdr:
    // DOS Header.
    .short  MZ_MAGIC                // (''MZ''), MS-DOS header.
    .long   0, 0                    // Reserved.
    .long   __efistub__gzdata_start - .Ldoshdr  // Offset to the payload 
    .long   __efistub__gzdata_size              // Size of the payload

    .org    .Ldoshdr + 0x38
    .long   XINO_PE_MAGIC           // (''XINO'').
    .long   .Lpehdr - .Ldoshdr      // Pointer to PE header (Off: 0x3C).

.Lpehdr:
    // COFF Header.
    .long   PE_MAGIC                // (''PE\0\0''), COFF header. 
    .short  IMAGE_FILE_MACHINE_ARM64    // Machine.
    .short  .Lsection_count         // NumberOfSection. 
    .long   0                       // TimeDateStamp.
    .long   0                       // PointerToSymbolTable (DEPRECATED).
    .long   0                       // NumberOfSymbolTable (DEPRECATED).
    .short  .Lsection_table - .Loptional_header     // SizeOptionalHeader. 
    .short  IMAGE_FILE_DEBUG_STRIPPED | \
            IMAGE_FILE_EXECUTABLE_IMAGE | \
            IMAGE_FILE_LINE_NUMS_STRIPPED           // Characteristics.

.Loptional_header:
    // Standard COFF Fields.
    .short  PE_OPT_MAGIC_PE32PLUS
    .byte   0       // MajorLinkerVersion.
    .byte   0       // MinorLinkerVersion.
    .long   _etext - .Lefi_header_end               // SizOfCode.
    .long   __data_size                             // SizeOfInitializedData.
    .long   0                                       // SizeOfUninitializedData.
    .long   __efistub_efi_entry - .Ldoshdr          // AddressOfEntryPoint (RVA).
    .long   .Lefi_header_end - .Ldoshdr             // BaseOfCode (RVA).
    .quad   0       // ImageBase.

    // Windows Specific Fields.
    .long   4096    // SectionAlignment.
    .long   512     // FileAlignment.
    .short  0       // MajorOperatingSystemVersion.
    .short  0       // MinorOperatingSystemVersion.
    .short  XINO_EFISTUB_MAJOR_VERSION              // MajorImageVersion.
    .short  XINO_EFISTUB_MINOR_VERSION              // MinorImageVersion.
    .short  0       // MajorSubsystemVersion.
    .short  0       // MinorSubsystemVersion.
    .long   0       // Win32VersionValue (zeros filled).
    .long   _end - .Ldoshdr                         // SizeOfImage.
    .long   .Lefi_header_end - .Ldoshdr             // SizeOfHeaders.
    .long   0       // Checksum.
    .short  IMAGE_SUBSYSTEM_EFI_APPLICATION         // Subsystem.
    .short  IMAGE_DLL_CHARACTERISTICS_NX_COMPAT     // DllCharacteristics.
    .quad   0       // SizeOfStackReserve.
    .quad   0       // SizeOfStackCommit.
    .quad   0       // SizeOfHeapReserve.
    .quad   0       // SizeOfHeapCommit.
    .long   0       // LoaderFlags (zeros filled).
    .long   (.Lsection_table - .) / 8               // NumberOfRvaAndSizes

    // Data Directories.
    .quad   0       // ExportTable (RVA), SizeOfExportTable.
    .quad   0       // ImportTable (RVA), SizeOfImportTable.
    .quad   0       // ResourceTable (RVA), SizeOfResourceTable.
    .quad   0       // ExceptionTable (RVA), SizeOfExceptionTable.
    .quad   0       // CertificationTable (RVA), SizeOfCertificationTable.
    .quad   0       // BaseRelocationTable (RVA), SizeOfBaseRelocationTable.

.Lsection_table:
    // Section Table.
    .ascii  ".text\0\0\0"
    .long   _etext - .Lefi_header_end               // VirtualSize.
    .long   .Lefi_header_end - .Ldoshdr             // VirtualAddress (RVA).
    .long   _etext - .Lefi_header_end               // SizeOfRawData.
    .long   .Lefi_header_end - .Ldoshdr             // PointerToRawData.

    .long   0, 0    // PointerToRelocations, PointerToLineNumbers.
    .short  0, 0    // NumberOfRelocations, NumberOfLineNumbers.
    .long   IMAGE_SCN_CNT_CODE | \
            IMAGE_SCN_MEM_READ | \
            IMAGE_SCN_MEM_EXECUTE                   // Characteristics.
    
    .ascii  ".data\0\0\0"
    .long   __data_size                             // VirtualSize.
    .long   _etext - .Ldoshdr                       // VirtualAddress (RVA).
    .long   __data_rawsize                          // SizeOfRawData.
    .long   _etext - .Ldoshdr                       // PointerToRawData.
    
    .long   0, 0    // PointerToRelocations, PointerToLineNumbers.
    .short  0, 0    // NumberOfRelocations, NumberOfLineNumbers.
    .long   IMAGE_SCN_CNT_INITIALIZED_DATA | \
            IMAGE_SCN_MEM_READ | \
            IMAGE_SCN_MEM_WRITE                     // Characteristics.

    .set    .Lsection_count, (. - .Lsection_table) / 40

.p2align    12
    .Lefi_header_end:
