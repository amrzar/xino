# U-Boot + TF-A

This document explains how mainline
[U-Boot](https://github.com/u-boot/u-boot) works together with
[Arm Trusted Firmware-A (TF-A)](https://github.com/ARM-software/arm-trusted-firmware)
on the [Rock 5B platform](https://radxa.com/products/rock5/5bp/).

## TF-A overview

Arm Trusted Firmware-A defines these logical stages:

#### BL1 – ROM or TF-A loader

- Trusted ROM code in SoC (for RK3588 this is the MaskROM).
- Very small, hard-wired.

#### BL2 – Trusted boot firmware

- Runs from SRAM or early DRAM.
- Loads and authenticates BL31, BL32, BL33.

#### BL31 – EL3 runtime / secure monitor

- Runs at EL3 in secure world.
- Handles SMC calls, PSCI, context switch between secure/non-secure.

#### BL32 – Trusted OS / secure runtime

- Optional, e.g. OP-TEE or a Secure Partition Manager.

#### BL33 – Non-secure firmware (bootloader / OS)

- U-Boot proper or a UEFI firmware image or Linux kernel.

## U-Boot + TF-A overview

- TPL/SPL are built from U-Boot source (except DDR TPL for rk35xx which
  is still external).
- BL31/BL32 are from TF-A source.
- Everything gets packed by `binman` into `u-boot-rockchip.bin`.

### U-Boot side files

When you build mainline U-Boot for Rockchip, you typically get:

File | What it is | Stage
---|---|---
`tpl/u-boot-tpl.bin` or `tpl/u-boot-tpl-dtb.bin` | TPL *(Tertiary Program Loader)* runs from SRAM and just does DDR init, then returns to ROM or jumps to SPL. | TPL (earliest non-ROM stage)
`spl/u-boot-spl.bin` or `spl/u-boot-spl-dtb.bin` | SPL *(Secondary Program Loader)* runs from DRAM, loads BL31 + U-Boot proper and jumps there. | BL2-like
`u-boot-nodtb.bin` + `u-boot.dtb` / `u-boot.bin` | U-Boot proper. | BL33
`u-boot.itb` (optional) | U-Boot + BL31 + DTs. | BL31 + BL33 in one container
`u-boot-rockchip.bin` | Final Rockchip-specific image: `binman` packs TPL, SPL, BL31, U-Boot proper (and DTs) at the right offsets so the MaskROM can boot it directly. | Contains TPL + SPL + BL31 + BL33

#### Notes

For RK35xx (RK3568/RK3588), mainline U-Boot still can’t do DDR init
itself, so it needs an external TPL binary (from
[`rkbin`](https://github.com/radxa/rkbin.git)) using `ROCKCHIP_TPL=`.
`binman` embeds that as the TPL part in `u-boot-rockchip.bin`.

#### Boot sequence

**ROM (BL1) → external TPL (DDR init) → back to ROM → SPL → BL31 →
BL33 (U-Boot proper)**

## Building `u-boot-rockchip.bin`

### Prerequisites

### Build instructions

```bash
git clone --depth 1 https://github.com/radxa/rkbin.git

# 1) Rockchip BL31 blob (or a TF-A BL31 build)
export BL31=../rkbin/bin/rk35/rk3588_bl31_v1.33.elf

# 2) Rockchip DDR init as external TPL
export ROCKCHIP_TPL=../rkbin/bin/rk35/rk3588_ddr_lp4_2112MHz_lp5_2736MHz_v1.09.bin

# 3) Mainline U-Boot
git clone --depth 1 https://source.denx.de/u-boot/u-boot.git
cd u-boot
make rock5b-rk3588_defconfig      # or generic-rk3588_defconfig, etc.
make CROSS_COMPILE=aarch64-linux-gnu- -j"$(nproc)"
```

### `u-boot-rockchip.bin` layout

It contains two separate files:

- `idbloader.img` – contains boot header + DDR init + SPL.
- `u-boot.itb` – U-Boot + BL31 + DTs.

```text
┌────────────────────────────────────────────┐
│ Rockchip IDBlock header                    │
│   + DDR init (ROCKCHIP_TPL = ddr.bin)      │
│   + SPL (u-boot-spl.bin)                   │
└────────────────────────────────────────────┘
               -- padding --
┌────────────────────────────────────────────┐
│ u-boot.itb (U-Boot + BL31 + DTs)           │
└────────────────────────────────────────────┘
```

## Flashing to eMMC/SD

Flash `u-boot-rockchip.bin` to SD/eMMC starting at sector `64`
(`0x40`):

```bash
sudo dd if=u-boot-rockchip.bin of=/dev/sdX seek=64
sync
```

Make sure to replace `/dev/sdX` with your actual device.

## Runtime boot flow

### Stage 1 – BootROM (in SoC mask ROM)

1. Power-on / reset.
2. RK3588 BootROM samples boot mode pins / fuses to pick the first boot
   source (SD, eMMC, SPI, etc.).
3. It looks at sector `0x40` (`64`) for a valid IDBlock header (the
   start of `idbloader.img` inside `u-boot-rockchip.bin`).
4. If the header is sane, BootROM:
   - Loads the DDR init program (TPL) into internal SRAM.
   - Optionally also records information about the "next stage" (SPL) in
     the header.

Then it jumps to the TPL entry point.

### Stage 2a – TPL (in SRAM)

TPL is either:

- Pure U-Boot TPL (for SoCs that support it), or
- An external TPL binary (the DDR init blob) that is supplied using
  `ROCKCHIP_TPL=`.

What TPL does:

1. Runs entirely from on-chip SRAM.
2. Sets up clocks, DRAM controller, performs DDR training, etc.
3. When DRAM is stable, it doesn’t load U-Boot itself; instead it
   returns to BootROM using the "back to BootROM" mechanism.

So at this point:

- DRAM is ready.
- Control goes back into BootROM, which knows where the "next stage" is
  from the IDBlock header.

### Stage 2b – BootROM loads SPL to DRAM

Back in BootROM:

1. BootROM now loads the SPL binary from the same `idbloader` structure,
   but this time into DRAM.
2. It jumps to the SPL entry point.

### Stage 3 – SPL: minimal U-Boot stage in DRAM

SPL now runs out of DRAM.

1. Performs basic board init that wasn't done in TPL (clocks, pinmux,
   MMC, etc.).
2. Locates and loads `u-boot.itb` from the boot storage.
3. Parses the `u-boot.itb` image:
   - Finds the BL31 image (passed using the `BL31=` env var).
   - Finds the U-Boot image (`u-boot-nodtb.bin`).
   - Picks the right DTB for your board.
   - Optionally finds BL32 (OP-TEE) if present.
4. Copies BL31 to its secure EL3 address, and BL33 (U-Boot) to its
   non-secure address.
5. Sets up the EL3 hand-off parameters (including entry for U-Boot, DTB
   pointer, PSCI hooks) and jumps to BL31 at EL3.

From here on, BootROM/TPL/SPL are out of the picture.

### Stage 4 – BL31 at EL3

BL31 is now in control at EL3:

1. Sets up EL3 exception vectors, secure monitor, and PSCI SMC handlers.
2. Optionally, if BL32 (OP-TEE) is present, BL31 may load/enter it, then
   later resume to EL3.
3. Sets up the non-secure world entry state (`SCR_EL3`, `SPSR_EL3`,
   general-purpose regs).
4. Branches to BL33 entry (U-Boot proper) with an `eret`.

### Stage 5 – U-Boot proper at EL2 (or EL1)

Now we are in U-Boot, non-secure, with a DTB and PSCI services
available using SMC.

From U-Boot's point of view:

1. Do full board init (UART, MMC/SD, NVMe, USB, Ethernet, etc.).
2. Boot `xino`.

## Run xino


