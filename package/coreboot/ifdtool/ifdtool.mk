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
IFDTOOL_CXXFLAGS = $(TARGET_CXXFLAGS) -I$(@D)

define IFDTOOL_BUILD_CMDS
   $(MAKE) $(TARGET_CONFIGURE_OPTS) HOSTCC="$(TARGET_CC)" CXXFLAGS="$(IFDTOOL_CXXFLAGS)" CFLAGS="$(IFDTOOL_CFLAGS)" -C $(@D)
endef

define IFDTOOL_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -D $(@D)/ifdtool $(TARGET_DIR)/usr/bin/ifdtool
endef

$(eval $(generic-package))
