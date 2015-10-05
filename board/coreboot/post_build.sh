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
	echo "==> Copying rootfs additions ..."
	rsync -va $BOARD_DIR/rootfs-additions/* $TARGET_DIR/
	error_exit $?
else
	echo "==> No rootfs additions found !"
fi

# Disable dropbear server
if [ -e $TARGET_DIR/etc/init.d/S50dropbear ]; then
	echo -n "==> Disable Dropbear server: [Y/n] "
	read DROPBEARSERVER
	if [ "$DROPBEARSERVER" != "n" ]; then
		echo "==> Disabling dropbear server ..."
		#chmod a-x $TARGET_DIR/etc/init.d/S50dropbear
		mkdir -p $TARGET_DIR/etc/init.disabled
		mv -f $TARGET_DIR/etc/init.d/S50dropbear $TARGET_DIR/etc/init.disabled/
		error_exit $?
	fi
fi

# Disable gpe01 fix
if [ -e $TARGET_DIR/etc/init.d/S00gpefix ]; then
	echo -n "==> Disable GPE01 fix: [y/N] "
	read DISABLEGPEFIX
	if [ "$DISABLEGPEFIX" = "y" ]; then
		echo "==> Disabling gpe01 fix ..."
		#chmod a-x $TARGET_DIR/etc/init.d/S00gpefix
		mkdir -p $TARGET_DIR/etc/init.disabled
		mv -f $TARGET_DIR/etc/init.d/S00gpefix $TARGET_DIR/etc/init.disabled/
		error_exit $?
	fi
fi

# Enable virtual terminals in inittab
if [ ! $(fgrep tty12 "$TARGET_DIR/etc/inittab") ]; then
	echo "==> Enabling virtual terminal 2..12 ..."
	for ii in {12..2}; do
		sed -E -i 's/GENERIC_SERIAL/GENERIC_SERIAL\ntty'$ii'::askfirst:-\/bin\/login/' "$TARGET_DIR/etc/inittab"
		error_exit $?
	done
else
	echo "==> Virtual terminals already enabled."
fi

# Set pax flax (tentative)
if [ -e "$TARGET_DIR/usr/lib/libcrypto.so.1.0.0" ]; then
	echo "==> Setting paxflag on libcrypto ..."
	/sbin/paxctl -c $TARGET_DIR/usr/lib/libcrypto.so.1.0.0
	error_exit $?
	/sbin/paxctl -m $TARGET_DIR/usr/lib/libcrypto.so.1.0.0
	error_exit $?
fi

# Add symlink to gpg2 for convenience
if [ ! -e "$TARGET_DIR/usr/bin/gpg" ]; then
	echo "==> Symlinking gpg2 ..."
	ln -s /usr/bin/gpg2 $TARGET_DIR/usr/bin/gpg
fi
