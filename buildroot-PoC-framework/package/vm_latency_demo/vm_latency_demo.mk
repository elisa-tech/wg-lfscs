################################################################################
#
# vm_latency_demo
#
################################################################################

VM_LATENCY_DEMO_VERSION = 1.0
VM_LATENCY_DEMO_PKGDIR = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
VM_LATENCY_DEMO_SITE = $(VM_LATENCY_DEMO_PKGDIR)/src
VM_LATENCY_DEMO_SITE_METHOD = local

define VM_LATENCY_DEMO_BUILD_CMDS
	echo VM_LATENCY_DEMO_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -O0 -Wall -Wextra -std=c11 \
		-o $(@D)/vm_latency_demo \
		$(@D)/vm_latency_demo.c
endef

define VM_LATENCY_DEMO_INSTALL_TARGET_CMDS
	echo VM_LATENCY_DEMO_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/vm_latency_demo \
		$(TARGET_DIR)/usr/bin/vm_latency_demo
endef

$(eval $(generic-package))
