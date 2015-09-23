################################################################################
#
# ifdtool
#
################################################################################

IFDTOOL_VERSION = 1.2
IFDTOOL_SITE = /opt/local/src/coreboot/x220/util/ifdtool
IFDTOOL_SITE_METHOD = local
IFDTOOL_LICENSE = GPLv2
IFDTOOL_LICENSE_FILES = COPYING
IFDTOOL_CFLAGS = $(TARGET_CFLAGS) -I$(@D)

define IFDTOOL_BUILD_CMDS
	$(TARGET_CONFIGURE_OPTS) $(TARGET_CC) $(@D)/ifdtool.c $(IFDTOOL_CFLAGS) -o $(@D)/ifdtool
endef

define IFDTOOL_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -D $(@D)/ifdtool $(TARGET_DIR)/usr/bin/ifdtool
endef

$(eval $(generic-package))
