################################################################################
#
# intelmetool
#
################################################################################

INTELMETOOL_VERSION = main
INTELMETOOL_SITE = git://github.com/zamaudio/intelmetool.git
ITELMETOOL_SITE_METHOD = git
INTELMETOOL_LICENSE = GPLv2

define INTELMETOOL_BUILD_CMDS
   $(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D)
endef

define INTELMETOOL_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -D $(@D)/intelmetool $(TARGET_DIR)/usr/sbin/intelmetool
endef

$(eval $(generic-package))

