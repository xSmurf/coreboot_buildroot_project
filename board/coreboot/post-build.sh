TARGETDIR=$1

# Set root password to ’root’. Password generated with
# mkpasswd, from the ’whois’ package in Debian/Ubuntu.
#sed -i ’s%^root::%root:8kfIfYHmcyQEE:%’ $TARGETDIR/etc/shadow

# Application/log file mount point
#mkdir -p $TARGETDIR/applog
#grep -q "^/dev/mtdblock7" $TARGET_DIR/etc/fstab || echo "/dev/mtdblock7\t\t/applog\tjffs2\tdefaults\t\t0\t0" >> $TARGETDIR/etc/fstab

# Copy the rootfs additions
if [ -d $BOARDDIR/rootfs-additions ]; then
	echo "Copying rootfs additions..."
	cp -a $BOARDDIR/rootfs-additions/* $TARGETDIR/
fi
