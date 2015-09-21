################################################################################
#
# nvramtool
#
################################################################################

NVRAMTOOL_VERSION = 2.1
NVRAMTOOL_SITE = /opt/local/src/coreboot/x220/util/nvramtool
NVRAMTOOL_SITE_METHOD = local
NVRAMTOOL_LICENSE = GPLv2
NVRAMTOOL_LICENSE_FILES = COPYING
NVRAMTOOL_CFLAGS = $(TARGET_CFLAGS) -I$(@D)
NVRAMTOOL_CXXFLAGS = $(TARGET_CXXFLAGS) -I$(@D)

define NVRAMTOOL_BUILD_CMDS
   $(MAKE) $(TARGET_CONFIGURE_OPTS) HOSTCC="$(TARGET_CC)" CXXFLAGS="$(NVRAMTOOL_CXXFLAGS)" CFLAGS="$(NVRAMTOOL_CFLAGS)" -C $(@D)
endef

define NVRAMTOOL_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -D $(@D)/nvramtool $(TARGET_DIR)/usr/sbin/nvramtool
endef

$(eval $(generic-package))
