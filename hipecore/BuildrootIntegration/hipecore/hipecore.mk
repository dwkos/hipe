################################################################################
#
# hipecore
#
################################################################################

HIPECORE_VERSION = 0.6.0

HIPECORE_SITE = /path/to/hipecore   #<---- EDIT THIS!
HIPECORE_SITE_METHOD = local
HIPECORE_DEPENDENCIES = \
	host-bison host-gperf host-python3 gstreamer1 \
	gst1-plugins-base icu jpeg libpng libxml2 libxslt \
	webp woff2
HIPECORE_INSTALL_STAGING = YES

HIPECORE_LICENSE_FILES = Source/WebCore/LICENSE-LGPL-2 Source/WebCore/LICENSE-LGPL-2.1

HIPECORE_LICENSE = LGPL-2.1+, BSD-3-Clause, BSD-2-Clause
# Source files contain references to LGPL_EXCEPTION.txt but it is not included
# in the archive.
HIPECORE_LICENSE_FILES += LICENSE.LGPLv21

ifeq ($(BR2_PACKAGE_QT5BASE_OPENGL),y)
HIPECORE_CONF_OPTS += -DENABLE_OPENGL=ON \
	-DENABLE_GRAPHICS_CONTEXT_3D=ON
else
HIPECORE_CONF_OPTS += -DENABLE_OPENGL=OFF 
endif

#ifeq ($(BR2_PACKAGE_QT5BASE_EGLFS),y)
HIPECORE_DEPENDENCIES += libgles libegl
#endif

ifeq ($(BR2_PACKAGE_QT5BASE_XCB),y)
HIPECORE_DEPENDENCIES += xlib_libXcomposite xlib_libXext xlib_libXrender
endif

ifeq ($(BR2_PACKAGE_QT5DECLARATIVE),y)
HIPECORE_DEPENDENCIES += qt5declarative
endif

ifeq ($(BR2_PACKAGE_LIBEXECINFO),y)
HIPECORE_DEPENDENCIES += libexecinfo
endif

define HIPECORE_POST_EXTRACT_CMDS
    $(SED) 's|list(APPEND WebCore_LIBRARIES GLESv2 EGL)|list(APPEND WebCore_LIBRARIES "$(STAGING_DIR)/usr/lib/libGLESv2.so.2.0.0" "$(STAGING_DIR)/usr/lib/libEGL.so.1")|' $(@D)/Source/WebCore/CMakeLists.txt
endef

HIPECORE_CONF_ENV += \
    LDFLAGS="-L$(STAGING_DIR)/usr/lib -Wl,-rpath-link,$(STAGING_DIR)/usr/lib -l:libGLESv2.so.2.0.0 -l:libEGL.so.1"

HIPECORE_CONF_OPTS += \
    -DPORT=Qt \
	-DCMAKE_BUILD_TYPE=Release \
    -DENABLE_TOOLS=OFF \
    -DUSE_OPENGL_ES_2=ON \
    -DUSE_EGL=ON \
    -DCMAKE_LIBRARY_PATH="$(STAGING_DIR)/usr/lib" \
    -DCMAKE_INCLUDE_PATH="$(STAGING_DIR)/usr/include" \
    -DCMAKE_EXE_LINKER_FLAGS="-L$(STAGING_DIR)/usr/lib -lGLESv2 -lEGL" \
    -DCMAKE_MODULE_LINKER_FLAGS="-L$(STAGING_DIR)/usr/lib -lGLESv2 -lEGL" \
    -DCMAKE_SHARED_LINKER_FLAGS="-L$(STAGING_DIR)/usr/lib -lGLESv2 -lEGL"

$(eval $(cmake-package))
