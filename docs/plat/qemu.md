# QEMU

## Prerequisites

```bash
sudo apt update
sudo apt install qemu-system-arm gdb-multiarch
```

## Boot xino

```bash
qemu-system-aarch64 -M virt -cpu cortex-a53 -m 128M -nographic -serial mon:stdio \
	-device loader,file=build-xino/ukernel/xino.bin,addr=0x40200000,force-raw=on \
	-device loader,addr=0x40200000,cpu-num=0
```

## Build xino for QEMU

```bash
cmake -S . -B build-xino -G "Unix Makefiles" \
  -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake \
  -DCMAKE_INSTALL_PREFIX=$XINO \
  -DUKERNEL_PLATFORM=qemu

cmake --build build-xino -j"$(nproc)"
```

[[Up](build.md)]
