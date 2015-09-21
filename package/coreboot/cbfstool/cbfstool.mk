################################################################################
#
# cbfstool
#
################################################################################

CBFSTOOL_VERSION = 0.1
CBFSTOOL_SITE = /opt/local/src/coreboot/x220/util/cbfstool
CBFSTOOL_SITE_METHOD = local
CBFSTOOL_LICENSE = GPLv2
CBFSTOOL_LICENSE_FILES = COPYING
CBFSTOOL_CFLAGS = $(TARGET_CFLAGS) -std=c99 -I$(@D) -I$(@D)/flashmap
CBFSTOOL_CPPFLAGS = $(TARGET_CPPFLAGS) -std=c99 -I$(@D) -I$(@D)/flashmap
CBFSTOOL_LDFLAGS = $(TARGET_LDFLAGS) -std=c99 -I$(@D) -I$(@D)/flashmap

define CBFSTOOL_BUILD_CMDS
   $(MAKE) $(TARGET_CONFIGURE_OPTS) LDFLAGS="$(CBFSTOOL_LDFLAGS)" HOSTCPP="$(TARGET_CPP)" HOSTCC="$(TARGET_CC)" CPPFLAGS="$(CBFSTOOL_CPPFLAFS)" CFLAGS="$(CBFSTOOL_CFLAGS)" -C $(@D) cbfstool
endef

define CBFSTOOL_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -D $(@D)/cbfstool $(TARGET_DIR)/usr/sbin/cbfstool
endef

$(eval $(generic-package))
