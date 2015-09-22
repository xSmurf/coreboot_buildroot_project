################################################################################
#
# connect-proxy
#
################################################################################

CONNECTPROXY_VERSION = master
CONNECTPROXY_SITE = https://bitbucket.org/gotoh/connect
CONNECTPROXY_SITE_METHOD = git
CONNECTPROXY_LICENSE = GPLv2
CONNECTPROXY_CFLAGS = $(TARGET_CFLAGS) -I$(@D)
CONNECTPROXY_CXXFLAGS = $(TARGET_CXXFLAGS) -I$(@D)

define CONNECTPROXY_BUILD_CMDS
   $(MAKE) $(TARGET_CONFIGURE_OPTS) HOSTCC="$(TARGET_CC)" CXXFLAGS="$(CONNECTPROXY_CXXFLAGS)" CFLAGS="$(CONNECTPROXY_CFLAGS)" -C $(@D)
endef

define CONNECTPROXY_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -D $(@D)/connect-proxy $(TARGET_DIR)/usr/bin/connect-proxy
endef

$(eval $(generic-package))

