#!/bin/bash
set -e

TARGET_DIR=$1
BOARD_DIR="$BR2_EXTERNAL/board/coreboot"

error_exit() {
	if [ $1 -gt 0 ]; then
		echo "Error occured !"
		exit $1
	fi
}

# Copy the rootfs additions
if [ -d "$BOARD_DIR/rootfs-additions" ]; then
	echo "Copying rootfs additions ..."
	rsync -va $BOARD_DIR/rootfs-additions/* $TARGET_DIR/
	error_exit $?
else
	echo "No rootfs additions found !"
fi

# Disable dropbear server
if [ -e $TARGET_DIR/etc/init.d/S50dropbear ]; then
	echo "Disabling dropbear server ..."
	chmod a-x $TARGET_DIR/etc/init.d/S50dropbear
	error_exit $?
fi

# Enable virtual terminals in inittab
echo "Enabling virtual terminal 2..12 ..."
for ii in {12..2}; do
	sed -E -i 's/GENERIC_SERIAL/GENERIC_SERIAL\ntty'$ii'::askfirst:-\/bin\/login/' "$TARGET_DIR/etc/inittab"
	error_exit $?
done

# Set pax flax (tentative)
echo "Setting paxflag on libcrypto ..."
/sbin/paxctl -c $TARGET_DIR/usr/lib/libcrypto.so.1.0.0
error_exit $?
/sbin/paxctl -m $TARGET_DIR/usr/lib/libcrypto.so.1.0.0
error_exit $?

# Add symlink to gpg2 for convenience
echo "Symlinking gpg2 ..."
ln -s /usr/bin/gpg2 $TARGET_DIR/usr/bin/gpg
