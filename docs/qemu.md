# QEMU Instructions

## Prerequisites

```bash
sudo apt update
sudo apt install qemu-system-arm gdb-multiarch
```

## Run

```bash
qemu-system-aarch64 -M virt -cpu cortex-a53 -m 128M -nographic -serial mon:stdio \
	-device loader,file=build-xino/ukernel/ukernel.bin,addr=0x40200000,force-raw=on \
	-device loader,addr=0x40200000,cpu-num=0
```
