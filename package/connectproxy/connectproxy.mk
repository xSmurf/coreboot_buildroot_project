################################################################################
#
# connectproxy
#
################################################################################

CONNECTPROXY_VERSION = 1.100
CONNECTPROXY_SOURCE = $(CONNECTPROXY_VERSION).tar.gz
CONNECTPROXY_SITE = https://bitbucket.org/gotoh/connect/get/
CONNECTPROXY_LICENSE = GPLv2
CONNECTPROXY_CFLAGS = $(TARGET_CFLAGS) -I$(@D)
CONNECTPROXY_CXXFLAGS = $(TARGET_CXXFLAGS) -I$(@D)

define CONNECTPROXY_BUILD_CMDS
   $(MAKE) $(TARGET_CONFIGURE_OPTS) HOSTCC="$(TARGET_CC)" CXXFLAGS="$(CONNECTPROXY_CXXFLAGS)" CFLAGS="$(CONNECTPROXY_CFLAGS)" -C $(@D)
endef

define CONNECTPROXY_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -D $(@D)/connect $(TARGET_DIR)/usr/bin/connect
endef

$(eval $(generic-package))

