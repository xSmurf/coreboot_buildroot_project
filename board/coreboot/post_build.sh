#!/bin/bash
set -e

TARGET_DIR=$1
BOARD_DIR="$BR2_EXTERNAL/board/coreboot"

# Copy the rootfs additions
if [ -d "$BOARD_DIR/rootfs-additions" ]; then
	echo "Copying rootfs additions..."
	rsync -va $BOARD_DIR/rootfs-additions/* $TARGET_DIR/
else
	echo "No rootfs additions found..."
fi

# Disable dropbear server
echo "Disabling dropbear server..."
chmod a-x $TARGET_DIR/etc/init.d/S50dropbear
