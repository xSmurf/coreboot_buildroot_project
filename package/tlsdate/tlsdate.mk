################################################################################
#
# tlsdate
#
################################################################################

TLSDATE_VERSION = master
TLSDATE_SITE = https://github.com/ioerror/tlsdate.git
TLSDATE_LICENSE = git
TLSDATE_LICENSE_FILES = LICENSE
TLSDATE_CFLAGS = $(TARGET_CFLAGS) -I$(@D)
TLSDATE_CXXFLAGS = $(TARGET_CXXFLAGS) -I$(@D)

define TLSDATE_BUILD_CMDS
   $(MAKE) $(TARGET_CONFIGURE_OPTS) HOSTCC="$(TARGET_CC)" CXXFLAGS="$(TLSDATE_CXXFLAGS)" CFLAGS="$(TLSDATE_CFLAGS)" -C $(@D)
endef

define TLSDATE_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -D $(@D)/tlsdate $(TARGET_DIR)/usr/sbin/tlsdate
	$(INSTALL) -m 0755 -D $(@D)/etc/tlsdated.conf $(TARGET_DIR)/etc/tlsdated.conf
#	$(INSTALL) -m 0755 -D $(@D)/init/tlsdated-cros.conf $(TARGET_DIR)/etc/init.d/tlsdate
endef

$(eval $(generic-package))

