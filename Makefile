#
# Copyright (C) 2011 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ahans
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/ahans
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=+libcurl
  TITLE:=Alis Home Automation Server
endef

define Package/ahans/description
 This package contains the server for Alis home automation network
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TARGET_CFLAGS) -Wall" \
		LDFLAGS="$(TARGET_LDFLAGS) -L$(STAGING_DIR)/root-brcm47xx/usr/lib -lcurl -lpthread -lmicrohttpd -lstdc++"
endef

define Package/ahans/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ahans $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,ahans))
