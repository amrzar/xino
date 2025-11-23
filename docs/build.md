# Build Instructions

This guide assumes you are using an Ubuntu host.

## Prerequisites

```bash
sudo apt update
sudo apt install build-essential cmake git
sudo apt install cmake-curses-gui
```

## Environment variables

```bash
export XINO=$HOME/.local/opt/xino
```

## Build cross-compiler

Build Clang/LLVM targeting bare-metal AArch64.

```bash
export LLVM_INSTALL_DIR=$XINO/llvm

git clone --depth 1 https://github.com/llvm/llvm-project.git
cd llvm-project

cmake -S llvm -B build-llvm -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=$LLVM_INSTALL_DIR \
  -DLLVM_TARGETS_TO_BUILD=AArch64 \
  -DLLVM_ENABLE_PROJECTS="clang;lld" \
  -DLLVM_DEFAULT_TARGET_TRIPLE=aarch64-none-elf

cmake --build build-llvm -j"$(nproc)"
cmake --install build-llvm

export PATH=$LLVM_INSTALL_DIR/bin:$PATH
```

## Build sysroot

Build a minimal sysroot with headers to build the runtime libraries.

```bash
export XINO_SYSROOT=$XINO/sysroot

git clone git://git.musl-libc.org/musl
cd musl

CC=clang ./configure --target=aarch64-none-elf --prefix=$XINO_SYSROOT

make install-headers
```

## Build runtime libraries

| Component      | Role                                                                 |
| ------------- | -------------------------------------------------------------------- |
| **compiler-rt** | Implements low-level builtins (e.g. `__divti3`, `__udivdi3`) needed by Clang/LLVM. |
| **libunwind** | Handles stack unwinding for exceptions.                              |
| **libc++abi** | Provides C++ runtime support: exception handling, RTTI, `new`/`delete`, etc. |
| **libc++**    | Standard C++ library (e.g. `std::vector`, `std::string`). Optional if not using STL. |

See also the Clang toolchain documentation:
<https://clang.llvm.org/docs/Toolchain.html>

```bash
cmake -S runtimes -B build-runtimes -G "Unix Makefiles" \
  -DCMAKE_SYSTEM_NAME=Generic \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=$XINO_SYSROOT \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_AR=$LLVM_INSTALL_DIR/bin/llvm-ar \
  -DCMAKE_C_COMPILER_TARGET=aarch64-none-elf \
  -DCMAKE_CXX_COMPILER_TARGET=aarch64-none-elf \
  -DCMAKE_SYSROOT=$XINO_SYSROOT \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
  -DCMAKE_LINKER=ld.lld \
  -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi;libunwind;compiler-rt" \
  -DLLVM_USE_LINKER=lld \
  -DLIBCXX_ENABLE_THREADS=OFF \
  -DLIBCXX_ENABLE_SHARED=OFF \
  -DLIBCXX_ENABLE_FILESYSTEM=OFF \
  -DLIBCXX_ENABLE_RANDOM_DEVICE=OFF \
  -DLIBCXX_ENABLE_LOCALIZATION=OFF \
  -DLIBCXX_ENABLE_UNICODE=OFF \
  -DLIBCXX_HAS_TERMINAL_AVAILABLE=OFF \
  -DLIBCXX_ENABLE_WIDE_CHARACTERS=OFF \
  -DLIBCXX_ENABLE_MONOTONIC_CLOCK=OFF \
  -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
  -DLIBCXX_USE_COMPILER_RT=ON \
  -DLIBCXXABI_ENABLE_THREADS=OFF \
  -DLIBCXXABI_ENABLE_SHARED=OFF \
  -DLIBCXXABI_USE_COMPILER_RT=ON \
  -DLIBCXXABI_USE_LLVM_UNWINDER=ON \
  -DLIBCXXABI_BAREMETAL=ON \
  -DLIBUNWIND_ENABLE_SHARED=OFF \
  -DLIBUNWIND_ENABLE_THREADS=OFF \
  -DLIBUNWIND_IS_BAREMETAL=ON \
  -DCOMPILER_RT_BAREMETAL_BUILD=ON \
  -DCOMPILER_RT_BUILD_CRT=OFF \
  -DCOMPILER_RT_BUILD_CTX_PROFILE=OFF \
  -DCOMPILER_RT_BUILD_GWP_ASAN=OFF \
  -DCOMPILER_RT_BUILD_LIBFUZZER=OFF \
  -DCOMPILER_RT_BUILD_MEMPROF=OFF \
  -DCOMPILER_RT_BUILD_ORC=OFF \
  -DCOMPILER_RT_BUILD_PROFILE=OFF \
  -DCOMPILER_RT_BUILD_SANITIZERS=OFF \
  -DCOMPILER_RT_BUILD_XRAY=OFF \
  -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
  -DCOMPILER_RT_OS_DIR=. \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DCMAKE_C_FLAGS="-fPIC -D_SYS_SYSCALL_H -D_POSIX_SOURCE" \
  -DCMAKE_CXX_FLAGS="-fPIC -D_SYS_SYSCALL_H -D_POSIX_SOURCE -DELAST=4095" \
  -DCMAKE_ASM_FLAGS="-fPIC" \
  -Wno-dev

cmake --build build-runtimes -j"$(nproc)"
cmake --install build-runtimes
```

## Build xino

```bash
git clone https://github.com/amrzar/xino.git
cd xino
```

### Configure

```bash
cmake -S . -B build-xino -G "Unix Makefiles" \
  -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake \
  -DCMAKE_INSTALL_PREFIX=$XINO

ccmake build-xino
```

### Build

```bash
cmake --build build-xino -j"$(nproc)"
```

### Debugging

- `-DCMAKE_VERBOSE_MAKEFILE=ON` to see the full command line.
- `-DCMAKE_FIND_DEBUG_MODE=ON` to see the search paths for libraries
  and binaries.
- `-DCMAKE_BUILD_TYPE=Debug` to enable debug symbols in the final
  binary.

## Run xino

- [QEMU Platform](plat/qemu.md)
- [Radxa ROCK 5B+ Platform](plat/rock5b.md)
