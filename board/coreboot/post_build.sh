#!/bin/bash
set -e

TARGET_DIR=$1
BOARD_DIR="$BR2_EXTERNAL/board/coreboot"

# Copy the rootfs additions
if [ -d "$BOARD_DIR/rootfs-additions" ]; then
	echo "Copying rootfs additions ..."
	rsync -va $BOARD_DIR/rootfs-additions/* $TARGET_DIR/
else
	echo "No rootfs additions found !"
fi

# Disable dropbear server
if [ -e $TARGET_DIR/etc/init.d/S50dropbear ]; then
	echo "Disabling dropbear server ..."
	chmod a-x $TARGET_DIR/etc/init.d/S50dropbear
fi

# Enable virtual terminals in inittab
echo "Enabling virtual terminal 2..12 ..."
for ii in {12..2}; do
	sed -E -i 's/GENERIC_SERIAL/GENERIC_SERIAL\ntty'$ii'::askfirst:-\/bin\/login/' "$TARGET_DIR/etc/inittab"
done
