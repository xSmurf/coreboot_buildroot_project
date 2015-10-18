# Coreboot Buildroot

This repostory contains [buildroot](http://buildroot.uclibc.org/) configurations, patches, and addition meant to build a useful linux+busybox distribution which can be embedded on the eeprom of ThinkPad X220/X230 machines running [Coreboot](https://www.coreboot.org).

It includes three different configuration: the default config `coreboot_defconfig`, a minimal config `coreboot_minimal_defconfig` which can fit on the X230 without hardware modifications (but lacks some extra goodies), and a grsec configuration for both of these.

$ git clone https://github.com/xsmurf/coreboot_buildroot_project.git
$ export BR_PROJECT=$PWD/coreboot_buildroot_project
$ wget <buildroot-release.tgz>
$ tar -xjf <buildroot-release.tgz>
$ cd <buildroot-release>
$ make BR2_EXTERNAL=$BR_PROJECT coreboot_defconfig
$ make

After making changes in buildroot with `make menuconfig`, `make linux-menuconfig`, or `make busybox-menuconfig`, you use the included `$BR_PROJECT/scripts/save.sh` to export your changes back to the project.
