################################################################################
#
# connectproxy
#
################################################################################

CONNECTPROXY_VERSION = 1.104
CONNECTPROXY_SOURCE = $(CONNECTPROXY_VERSION).tar.gz
CONNECTPROXY_SITE = https://bitbucket.org/gotoh/connect/get/
CONNECTPROXY_LICENSE = GPLv2

define CONNECTPROXY_BUILD_CMDS
   $(MAKE) $(TARGET_CONFIGURE_OPTS) -C $(@D)
endef

define CONNECTPROXY_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -D $(@D)/connect $(TARGET_DIR)/usr/bin/connect
endef

$(eval $(generic-package))

