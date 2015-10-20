# Coreboot Buildroot

This repostory contains [buildroot](http://buildroot.uclibc.org/) configurations, patches, and additions meant to build a useful linux+busybox distribution which can be embedded on the flash memory of ThinkPad X220/X230 machines running [Coreboot](https://www.coreboot.org).

It includes three different configuration: the default config `coreboot_defconfig`, a minimal config `coreboot_minimal_defconfig` which can fit on the X230 without hardware modifications (but lacks some extra goodies), and a grsec configuration for both of these.

Some of the coreboot utility (ie: cbfstool, ifdtool, and nvramtool) packages are built from a local source. you will need to edit this path in `packages/coreboot/*/*.mk`

Building cbfstool currently requires a patch in the local coreboot tree which has not yet been added upstream:

```
diff --git a/util/cbfstool/Makefile.inc b/util/cbfstool/Makefile.inc
index 976f0c2..8e04670 100644
--- a/util/cbfstool/Makefile.inc
+++ b/util/cbfstool/Makefile.inc
@@ -45,7 +45,7 @@ TOOLCFLAGS ?= -std=c99 -Werror -Wall -Wextra
 TOOLCFLAGS += -Wcast-qual -Wmissing-prototypes -Wredundant-decls -Wshadow
 TOOLCFLAGS += -Wstrict-prototypes -Wwrite-strings
 TOOLCPPFLAGS ?= -D_DEFAULT_SOURCE # memccpy() from string.h
-TOOLCPPFLAGS += -D_XOPEN_SOURCE=700 # strdup() from string.h
+TOOLCPPFLAGS += -D_XOPEN_SOURCE=700 -D__USE_XOPEN_EXTENDED # strdup() from string.h
 TOOLCPPFLAGS += -I$(top)/util/cbfstool/flashmap
 TOOLCPPFLAGS += -I$(top)/util/cbfstool
 TOOLCPPFLAGS += -I$(objutil)/cbfstool
diff --git a/util/cbfstool/cbfs_image.c b/util/cbfstool/cbfs_image.c
index c40bd66..36438e9 100644
--- a/util/cbfstool/cbfs_image.c
+++ b/util/cbfstool/cbfs_image.c
@@ -24,6 +24,7 @@
 #include <stdlib.h>
 #include <string.h>
 #include <strings.h>
+#include <unistd.h>
 
 #include "common.h"
 #include "cbfs_image.h"
```

To use the project with Buildroot:

```
$ git clone https://github.com/xsmurf/coreboot_buildroot_project.git
$ export BR_PROJECT=$PWD/coreboot_buildroot_project
$ wget <buildroot-release.tgz>
$ tar -xjf <buildroot-release.tgz>
$ cd <buildroot-release>
$ make BR2_EXTERNAL=$BR_PROJECT coreboot_defconfig
$ make
```

After making changes in buildroot with `make menuconfig`, `make linux-menuconfig`, or `make busybox-menuconfig`, you use the included `$BR_PROJECT/scripts/save.sh` to export your changes back to the project.


-------


**WARNING:** This is work in progress with no guarantees given of any sorts. This may destroy your system, make flames shoot out of the battery and accidentally kill a puppy.

