#!/bin/bash

make busybox-update-config
make linux-update-config
make savedefconfig

echo 'BR2_DEFCONFIG="$(BR2_EXTERNAL)/configs/coreboot_defconfig"' >> ../builddir/configs/coreboot_defconfig

echo 'Saved!'
echo
