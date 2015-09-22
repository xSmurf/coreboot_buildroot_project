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
echo "Disabling dropbear server ..."
chmod a-x $TARGET_DIR/etc/init.d/S50dropbear

# Enable virtual terminals in inittab
echo "Enabling virtual terminal 2..8 ..."
sed -E -i 's/GENERIC_SERIAL/GENERIC_SERIAL\ntty8::askfirst:-\/bin\/login/' "$TARGET_DIR/etc/inittab"
sed -E -i 's/GENERIC_SERIAL/GENERIC_SERIAL\ntty7::askfirst:-\/bin\/login/' "$TARGET_DIR/etc/inittab"
sed -E -i 's/GENERIC_SERIAL/GENERIC_SERIAL\ntty6::askfirst:-\/bin\/login/' "$TARGET_DIR/etc/inittab"
sed -E -i 's/GENERIC_SERIAL/GENERIC_SERIAL\ntty5::askfirst:-\/bin\/login/' "$TARGET_DIR/etc/inittab"
sed -E -i 's/GENERIC_SERIAL/GENERIC_SERIAL\ntty4::askfirst:-\/bin\/login/' "$TARGET_DIR/etc/inittab"
sed -E -i 's/GENERIC_SERIAL/GENERIC_SERIAL\ntty3::askfirst:-\/bin\/login/' "$TARGET_DIR/etc/inittab"
sed -E -i 's/GENERIC_SERIAL/GENERIC_SERIAL\ntty2::askfirst:-\/bin\/login/' "$TARGET_DIR/etc/inittab"
