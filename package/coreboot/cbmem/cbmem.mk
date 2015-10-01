################################################################################
#
# cbmem
#
################################################################################

CBMEM_VERSION = 1.1
CBMEM_SITE = $(BR2_EXTERNAL)/board/coreboot/packages/cbmem
CBMEM_SITE_METHOD = local
CBMEM_LICENSE = GPLv2
CBMEM_LICENSE_FILES = COPYING
CBMEM_CFLAGS = $(TARGET_CFLAGS) -I$(@D)

define CBMEM_BUILD_CMDS
	$(MAKE) $(TARGET_CONFIGURE_OPTS) CC="$(TARGET_CC)" -C "$(@D)"
endef

define CBMEM_INSTALL_TARGET_CMDS
	$(INSTALL) -m 0755 -D $(@D)/cbmem $(TARGET_DIR)/usr/sbin/cbmem
endef

$(eval $(generic-package))
