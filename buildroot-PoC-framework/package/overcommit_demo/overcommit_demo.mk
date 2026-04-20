################################################################################
#
# overcommit_demo
#
################################################################################

OVERCOMMIT_DEMO_VERSION = 1.0
OVERCOMMIT_DEMO_PKGDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
OVERCOMMIT_DEMO_SITE = $(OVERCOMMIT_DEMO_PKGDIR)/src
OVERCOMMIT_DEMO_SITE_METHOD = local

define OVERCOMMIT_DEMO_BUILD_CMDS
	echo OVERCOMMIT_DEMO_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -O0 -Wall -Wextra -std=c11 \
		-o $(@D)/overcommit_demo \
		$(@D)/overcommit_demo.c
endef

define OVERCOMMIT_DEMO_INSTALL_TARGET_CMDS
	echo OVERCOMMIT_DEMO_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/overcommit_demo \
		$(TARGET_DIR)/usr/bin/overcommit_demo
endef

$(eval $(generic-package))
