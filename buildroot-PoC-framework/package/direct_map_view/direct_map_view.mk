################################################################################
#
# DIRECT_MAP_VIEW
#
################################################################################

DIRECT_MAP_VIEW_VERSION = 1.0
DIRECT_MAP_VIEW_PKGDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
DIRECT_MAP_VIEW_SITE = $(DIRECT_MAP_VIEW_PKGDIR)/src
DIRECT_MAP_VIEW_SITE_METHOD = local

define DIRECT_MAP_VIEW_BUILD_CMDS
	echo DIRECT_MAP_VIEW_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -O0 -Wall -Wextra -std=c11 \
		-o $(@D)/direct_map_view \
		$(@D)/direct_map_view.c
endef

define DIRECT_MAP_VIEW_INSTALL_TARGET_CMDS
	echo DIRECT_MAP_VIEW_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/direct_map_view \
		$(TARGET_DIR)/usr/bin/direct_map_view
endef

$(eval $(generic-package))
