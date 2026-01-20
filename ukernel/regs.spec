{
  "namespace" : "xino::autogen::regs",
                "description"
      : "System registers used by the uKernel (EL2/VHE)",
        "macro_prefix" : "XINO",
                         "registers"
      : [
        {
          "encoding" : "CurrentEL",
          "width" : 64,
          "fields" : [ {
            "name" : "el",
            "lsb" : 2,
            "width" : 2,
            "access" : "ro",
            "description" : "Current exception level."
          } ]
        },
        {
          "encoding" : "ID_AA64MMFR0_EL1",
          "width" : 64,
          "fields" : [
            {
              "name" : "t_gran4_2",
              "lsb" : 40,
              "width" : 4,
              "access" : "ro",
              "description" :
                  "Stage-2 4KB granule support (alternative ID scheme).",
              "enum_values" : {
                "t_gran4" : 0,
                "not_supported" : 1,
                "supported" : 2,
                "large_pa_52_bits" : 3
              }
            },
            {
              "name" : "t_gran16_2",
              "lsb" : 32,
              "width" : 4,
              "access" : "ro",
              "description" :
                  "Stage-2 16KB granule support (alternative ID scheme).",
              "enum_values" : {
                "t_gran16" : 0,
                "not_supported" : 1,
                "supported" : 2,
                "large_pa_52_bits" : 3
              }
            },
            {
              "name" : "t_gran4",
              "lsb" : 28,
              "width" : 4,
              "access" : "ro",
              "description" : "Stage-1 4KB translation granule support.",
              "enum_values" : {
                "supported" : 0,
                "large_pa_52_bits" : 1,
                "not_supported" : 15
              }
            },
            {
              "name" : "t_gran16",
              "lsb" : 20,
              "width" : 4,
              "access" : "ro",
              "description" : "Stage-1 16KB translation granule support.",
              "enum_values" :
                  {"not_supported" : 0, "supported" : 1, "large_pa_52_bits" : 2}
            },
            {
              "name" : "pa_range",
              "lsb" : 0,
              "width" : 4,
              "access" : "ro",
              "description" : "Physical address size supported.",
              "enum_values" : {
                "pa_32_bits" : 0,
                "pa_36_bits" : 1,
                "pa_40_bits" : 2,
                "pa_42_bits" : 3,
                "pa_44_bits" : 4,
                "pa_48_bits" : 5,
                "pa_52_bits" : 6,
                "pa_56_bits" : 7
              }
            }
          ]
        },
        {
          "encoding" : "ID_AA64MMFR1_EL1",
          "width" : 64,
          "fields" : [
            {
              "name" : "xnx",
              "lsb" : 28,
              "width" : 4,
              "access" : "ro",
              "description" : "Execute-never control.",
              "enum_values" : {"not_supported" : 0, "supported" : 1}
            },
            {
              "name" : "vh",
              "lsb" : 8,
              "width" : 4,
              "access" : "ro",
              "description" : "Virtualization Host Extensions.",
              "enum_values" : {"not_supported" : 0, "supported" : 1}
            }
          ]
        },
        {
          "encoding" : "ID_AA64MMFR2_EL1",
          "width" : 64,
          "fields" : [ {
            "name" : "st",
            "lsb" : 28,
            "width" : 4,
            "access" : "ro",
            "description" : "Support for small translation tables",
            "enum_values" : {"not_supported" : 0, "supported" : 1}
          } ]
        },
        {
          "encoding" : "MAIR_EL2",
          "width" : 64,
          "policy" : {"post_write" : "isb"},
          "fields" : [ {
            "access" : "rw",
            "description" : "Memory Attribute Indirection Register."
          } ]
        },
        {
          "encoding" : "DAIFSet",
          "width" : 64,
          "policy" : {
            "post_write" : "isb",
            "immediate_bits" :
                {"fiq" : 1, "irq" : 2, "s_error" : 4, "debug" : 8}
          }
        },
        {
          "encoding" : "DAIFClr",
          "width" : 64,
          "policy" : {
            "post_write" : "isb",
            "immediate_bits" :
                {"fiq" : 1, "irq" : 2, "s_error" : 4, "debug" : 8}
          }
        },
        {
          "encoding" : "DAIF",
          "width" : 64,
          "policy" : {"post_write" : "isb"},
          "fields" : [
            {
              "name" : "d",
              "bit" : 9,
              "access" : "rw",
              "description" : "Debug mask: 1 = mask Debug exceptions"
            },
            {
              "name" : "a",
              "bit" : 8,
              "access" : "rw",
              "description" : "SError mask: 1 = mask SError exceptions"
            },
            {
              "name" : "i",
              "bit" : 7,
              "access" : "rw",
              "description" : "IRQ mask: 1 = mask IRQ"
            },
            {
              "name" : "f",
              "bit" : 6,
              "access" : "rw",
              "description" : "FIQ mask: 1 = mask FIQ"
            }
          ]
        },
        {
          "encoding" : "SCTLR_EL2",
          "width" : 64,
          "policy" : {"post_write" : "isb"},
          "fields" : [
            {
              "name" : "m",
              "bit" : 0,
              "access" : "rw",
              "description" : "MMU enable for EL2&0 stage-1."
            },
            {
              "name" : "a",
              "bit" : 1,
              "access" : "rw",
              "description" : "Alignment check enable."
            },
            {
              "name" : "c",
              "bit" : 2,
              "access" : "rw",
              "description" : "Data/unified cache enable."
            },
            {
              "name" : "sa",
              "bit" : 3,
              "access" : "rw",
              "description" : "SP alignment check enable."
            },
            {
              "name" : "i",
              "bit" : 12,
              "access" : "rw",
              "description" : "Instruction cache enable."
            },
            {
              "name" : "wxn",
              "bit" : 19,
              "access" : "rw",
              "description" : "Write-eXecute Never."
            }
          ]
        },
        {
          "encoding" : "TCR_EL2",
          "width" : 64,
          "policy" : {"post_write" : "isb"},
          "fields" : [
            {
              "name" : "t0sz",
              "lsb" : 0,
              "width" : 6,
              "access" : "rw",
              "description" : "TTBR0_EL2 VA size: T0SZ = 64 - VA_BITS."
            },
            {
              "name" : "epd0",
              "bit" : 7,
              "access" : "rw",
              "description" : "0: enable TTBR0 walks; 1: disable."
            },
            {
              "name" : "irgn0",
              "lsb" : 8,
              "width" : 2,
              "access" : "rw",
              "description" : "TTBR0 walk inner cacheability.",
              "enum_values" : {
                "non_cacheable" : 0,
                "wb_with_wa" : 1,
                "wt" : 2,
                "wb_without_wa" : 3
              }
            },
            {
              "name" : "orgn0",
              "lsb" : 10,
              "width" : 2,
              "access" : "rw",
              "description" : "TTBR0 walk outer cacheability.",
              "enum_values" : {
                "non_cacheable" : 0,
                "wb_with_wa" : 1,
                "wt" : 2,
                "wb_without_wa" : 3
              }
            },
            {
              "name" : "sh0",
              "lsb" : 12,
              "width" : 2,
              "access" : "rw",
              "description" : "TTBR0 walk shareability.",
              "enum_values" : {
                "non_shareable" : 0,
                "outer_shareable" : 2,
                "inner_shareable" : 3
              }
            },
            {
              "name" : "tg0",
              "lsb" : 14,
              "width" : 2,
              "access" : "rw",
              "description" : "TTBR0 granule size.",
              "enum_values" :
                  {"granule_4k" : 0, "granule_64k" : 1, "granule_16k" : 2}
            },
            {
              "name" : "t1sz",
              "lsb" : 16,
              "width" : 6,
              "access" : "rw",
              "description" : "TTBR1_EL2 VA size: T1SZ = 64 - VA_BITS."
            },
            {
              "name" : "a1",
              "bit" : 22,
              "access" : "rw",
              "description" :
                  "Selects whether TTBR0_EL2 or TTBR1_EL2 defines the ASID.",
              "enum_values" : {"ttbr0_el2_asid" : 0, "ttbr1_el2_asid" : 1}
            },
            {
              "name" : "epd1",
              "bit" : 23,
              "access" : "rw",
              "description" : "0: enable TTBR1 walks; 1: disable."
            },
            {
              "name" : "irgn1",
              "lsb" : 24,
              "width" : 2,
              "access" : "rw",
              "description" : "TTBR1 walk inner cacheability.",
              "enum_values" : {
                "non_cacheable" : 0,
                "wb_with_wa" : 1,
                "wt" : 2,
                "wb_without_wa" : 3
              }
            },
            {
              "name" : "orgn1",
              "lsb" : 26,
              "width" : 2,
              "access" : "rw",
              "description" : "TTBR1 walk outer cacheability.",
              "enum_values" : {
                "non_cacheable" : 0,
                "wb_with_wa" : 1,
                "wt" : 2,
                "wb_without_wa" : 3
              }
            },
            {
              "name" : "sh1",
              "lsb" : 28,
              "width" : 2,
              "access" : "rw",
              "description" : "TTBR1 walk shareability.",
              "enum_values" : {
                "non_shareable" : 0,
                "outer_shareable" : 2,
                "inner_shareable" : 3
              }
            },
            {
              "name" : "tg1",
              "lsb" : 30,
              "width" : 2,
              "access" : "rw",
              "description" : "TTBR1 granule size.",
              "enum_values" :
                  {"granule_16k" : 1, "granule_4k" : 2, "granule_64k" : 3}
            },
            {
              "name" : "ips",
              "lsb" : 32,
              "width" : 3,
              "access" : "rw",
              "description" : "Intermediate physical address size.",
              "enum_values" : {
                "pa_32_bits" : 0,
                "pa_36_bits" : 1,
                "pa_40_bits" : 2,
                "pa_42_bits" : 3,
                "pa_44_bits" : 4,
                "pa_48_bits" : 5,
                "pa_52_bits" : 6,
                "pa_56_bits" : 7
              }
            },
            {
              "name" : "as",
              "bit" : 36,
              "access" : "rw",
              "description" : "ASID size.",
              "enum_values" : {"asid_8_bits" : 0, "asid_16_bits" : 1}
            },
            {
              "name" : "tbi0",
              "bit" : 37,
              "access" : "rw",
              "description" : "Top Byte Ignore for TTBR0 EL2 regime."
            },
            {
              "name" : "tbi1",
              "bit" : 38,
              "access" : "rw",
              "description" : "Top Byte Ignore for TTBR1 EL2 regime."
            }
          ]
        },
        {
          "encoding" : "TTBR0_EL2",
          "width" : 64,
          "policy" : {"post_write" : "isb"},
          "fields" : [
            {
              "name" : "comm_not_priv",
              "bit" : 0,
              "access" : "rw",
              "description" : "Common-not-Private (FEAT_TTCNP)."
            },
            {
              "name" : "base_addr",
              "lsb" : 1,
              "width" : 47,
              "access" : "rw",
              "description" : "Base address (alignment per TG0)."
            },
            {
              "name" : "asid",
              "lsb" : 48,
              "width" : 16,
              "access" : "rw",
              "description" : "ASID."
            }
          ]
        },
        {
          "encoding" : "TTBR1_EL2",
          "width" : 64,
          "policy" : {"post_write" : "isb"},
          "fields" : [
            {
              "name" : "comm_not_priv",
              "bit" : 0,
              "access" : "rw",
              "description" : "Common-not-Private (FEAT_TTCNP)."
            },
            {
              "name" : "base_addr",
              "lsb" : 1,
              "width" : 47,
              "access" : "rw",
              "description" : "Base address (alignment per TG1)."
            },
            {
              "name" : "asid",
              "lsb" : 48,
              "width" : 16,
              "access" : "rw",
              "description" : "ASID."
            }
          ]
        },
        {
          "encoding" : "HCR_EL2",
          "width" : 64,
          "policy" : {"post_write" : "isb"},
          "fields" : [
            {
              "name" : "vm",
              "bit" : 0,
              "access" : "rw",
              "description" : "Enable stage-2 translation for EL1/0."
            },
            {
              "name" : "fmo",
              "bit" : 3,
              "access" : "rw",
              "description" : "Route FIQ to EL2."
            },
            {
              "name" : "imo",
              "bit" : 4,
              "access" : "rw",
              "description" : "Route IRQ to EL2."
            },
            {
              "name" : "amo",
              "bit" : 5,
              "access" : "rw",
              "description" : "Route SError to EL2."
            },
            {
              "name" : "rw",
              "bit" : 31,
              "access" : "rw",
              "description" :
                  "Execution state for lower ELs: 0=AArch32, 1=AArch64."
            },
            {
              "name" : "e2h",
              "bit" : 34,
              "access" : "rw",
              "description" : "VHE: EL2 has EL1-like controls for EL2&0."
            }
          ]
        },
        {
          "encoding" : "VTCR_EL2",
          "width" : 64,
          "policy" : {"post_write" : "isb"},
          "fields" : [
            {
              "name" : "t0sz",
              "lsb" : 0,
              "width" : 6,
              "access" : "rw",
              "description" : "IPA size via T0SZ = 64 - IPA_BITS."
            },
            {
              "name" : "sl0",
              "lsb" : 6,
              "width" : 2,
              "access" : "rw",
              "description" : "Starting level of S2 translation table walk."
            },
            {
              "name" : "irgn0",
              "lsb" : 8,
              "width" : 2,
              "access" : "rw",
              "description" : "S2 walk inner cacheability.",
              "enum_values" : {
                "non_cacheable" : 0,
                "wb_with_wa" : 1,
                "wt" : 2,
                "wb_without_wa" : 3
              }
            },
            {
              "name" : "orgn0",
              "lsb" : 10,
              "width" : 2,
              "access" : "rw",
              "description" : "S2 walk outer cacheability.",
              "enum_values" : {
                "non_cacheable" : 0,
                "wb_with_wa" : 1,
                "wt" : 2,
                "wb_without_wa" : 3
              }
            },
            {
              "name" : "sh0",
              "lsb" : 12,
              "width" : 2,
              "access" : "rw",
              "description" : "S2 walk shareability.",
              "enum_values" : {
                "non_shareable" : 0,
                "outer_shareable" : 2,
                "inner_shareable" : 3
              }
            },
            {
              "name" : "tg0",
              "lsb" : 14,
              "width" : 2,
              "access" : "rw",
              "description" : "S2 granule size.",
              "enum_values" :
                  {"granule_4k" : 0, "granule_64k" : 1, "granule_16k" : 2}
            },
            {
              "name" : "ps",
              "lsb" : 16,
              "width" : 3,
              "access" : "rw",
              "description" : "S2 physical address size.",
              "enum_values" : {
                "pa_32_bits" : 0,
                "pa_36_bits" : 1,
                "pa_40_bits" : 2,
                "pa_42_bits" : 3,
                "pa_44_bits" : 4,
                "pa_48_bits" : 5,
                "pa_52_bits" : 6,
                "pa_56_bits" : 7
              }
            }
          ]
        },
        {
          "encoding" : "VTTBR_EL2",
          "width" : 64,
          "policy" : {"post_write" : "isb"},
          "fields" : [
            {
              "name" : "comm_not_priv",
              "bit" : 0,
              "access" : "rw",
              "description" : "Common-not-Private for S2 walks (FEAT_TTCNP)."
            },
            {
              "name" : "base_addr",
              "lsb" : 1,
              "width" : 47,
              "access" : "rw",
              "description" : "S2 base address (alignment per VTCR.TG0)."
            },
            {
              "name" : "vmid",
              "lsb" : 48,
              "width" : 16,
              "access" : "rw",
              "description" : "VMID (8 or 16 bits depending on implementation)."
            }
          ]
        }
      ]
}