#!/bin/bash

make busybox-update-config
make linux-update-config
make savedefconfig


for fconf in ../builddir/configs/*_defconfig; do
	fgrep -q BR2_DEFCONFIG "$fconf"
	if [ $? -gt 0 ]; then
		echo 'BR2_DEFCONFIG="$(BR2_EXTERNAL)/configs/'${fconf##*/}'"' >> "$fconf"
		#echo 'BR2_DEFCONFIG="$(BR2_EXTERNAL)/configs/coreboot_defconfig"' >> ../builddir/configs/coreboot_defconfig
	fi
done
echo 'Saved!'
echo
