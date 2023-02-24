#!/bin/bash

make clean && make mrproper
export PATH=$(pwd)/toolchain/prebuilts_clang_host_linux-x86_clang-r383902-main/bin:$PATH
export CROSS_COMPILE=$(pwd)/toolchain/aarch64-linux-android-4.9-master/bin/aarch64-linux-android-
export CC=$(pwd)/toolchain/prebuilts_clang_host_linux-x86_clang-r383902-main/bin/clang
export CLANG_TRIPLE=aarch64-linux-gnu-
export ARCH=arm64

export KCFLAGS=-w
export CONFIG_SECTION_MISMATCH_WARN_ONLY=y

make -C $(pwd) O=$(pwd)/out KCFLAGS=-w CONFIG_SECTION_MISMATCH_WARN_ONLY=y physwizz_defconfig
make -C $(pwd) O=$(pwd)/out KCFLAGS=-w CONFIG_SECTION_MISMATCH_WARN_ONLY=y -j16

cp out/arch/arm64/boot/Image $(pwd)/arch/arm64/boot/Image
