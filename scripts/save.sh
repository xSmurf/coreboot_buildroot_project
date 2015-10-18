#!/bin/bash

make busybox-update-config
make linux-update-config
make savedefconfig

BDIR=$(dirname $(dirname ${0}))
for fconf in $BDIR/configs/*_defconfig; do
	fgrep -q BR2_DEFCONFIG "$fconf"
	if [ $? -gt 0 ]; then
		echo 'BR2_DEFCONFIG="$(BR2_EXTERNAL)/configs/'${fconf##*/}'"' >> "$fconf"
	fi
done
echo 'Saved!'
echo
