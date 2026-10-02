include(platform/ImageDecoders.cmake)
include(platform/Linux.cmake)
include(platform/TextureMapper.cmake)

if (NOT USE_LIBJPEG)
    list(REMOVE_ITEM WebCore_SOURCES
        platform/image-decoders/jpeg/JPEGImageDecoder.cpp
    )
endif ()

if (JPEG_DEFINITIONS)
    add_definitions(${JPEG_DEFINITIONS})
endif ()

# hipecore: all the ${JAVASCRIPTCORE_DIR}/* and ${DERIVED_SOURCES_JAVASCRIPTCORE_DIR}
# include paths are gone - JavaScriptCore has been removed from the tree (Bucket 5.7).
# The handful of C++ typed-array headers WebCore still uses moved to
# Source/WTF/wtf/typedarrays/ and resolve via ${WTF_DIR} plus the <runtime/*>
# forwarding shims in Source/WebCore/ForwardingHeaders/runtime/.
list(APPEND WebCore_INCLUDE_DIRECTORIES
    "${THIRDPARTY_DIR}/ANGLE/"
    "${THIRDPARTY_DIR}/ANGLE/include/KHR"
    "${WEBCORE_DIR}/Modules/gamepad"
    "${WEBCORE_DIR}/dom/qt"
    "${WEBCORE_DIR}/editing/qt"
    "${WEBCORE_DIR}/history/qt"
    "${WEBCORE_DIR}/page/qt"
    "${WEBCORE_DIR}/platform/qt"
    "${WEBCORE_DIR}/platform/audio/qt"
    "${WEBCORE_DIR}/platform/graphics/egl"
    "${WEBCORE_DIR}/platform/graphics/glx"
    "${WEBCORE_DIR}/platform/graphics/gpu/qt"
    "${WEBCORE_DIR}/platform/graphics/opengl"
    "${WEBCORE_DIR}/platform/graphics/surfaces"
    "${WEBCORE_DIR}/platform/graphics/surfaces/qt"
    "${WEBCORE_DIR}/platform/graphics/qt"
    "${WEBCORE_DIR}/platform/graphics/win"
    "${WEBCORE_DIR}/platform/network/qt"
    "${WEBCORE_DIR}/platform/text/qt"
    "${WEBCORE_DIR}/platform/win"
    "${WEBCORE_DIR}/platform/graphics/x11"
    "${WTF_DIR}"
)

list(APPEND WebCore_SOURCES
    accessibility/qt/AccessibilityObjectQt.cpp

    dom/qt/GestureEvent.cpp

    editing/qt/EditorQt.cpp

    page/qt/DragControllerQt.cpp
    page/qt/EventHandlerQt.cpp
    page/qt/TouchAdjustment.cpp

    platform/KillRingNone.cpp

    platform/audio/qt/AudioBusQt.cpp

    platform/crypto/qt/CryptoDigestQt.cpp

    platform/graphics/ImageSource.cpp
    platform/graphics/PlatformDisplay.cpp
    platform/graphics/WOFFFileFormat.cpp

    platform/graphics/texmap/BitmapTextureImageBuffer.cpp
    platform/graphics/texmap/TextureMapperImageBuffer.cpp

    platform/graphics/qt/ColorQt.cpp
    platform/graphics/qt/FloatPointQt.cpp
    platform/graphics/qt/FloatRectQt.cpp
    platform/graphics/qt/FloatSizeQt.cpp
    platform/graphics/qt/FontCacheQt.cpp
    platform/graphics/qt/FontCascadeQt.cpp
    platform/graphics/qt/FontCustomPlatformDataQt.cpp
    platform/graphics/qt/FontPlatformDataQt.cpp
    platform/graphics/qt/FontQt.cpp
    platform/graphics/qt/GlyphPageTreeNodeQt.cpp
    platform/graphics/qt/GradientQt.cpp
    platform/graphics/qt/GraphicsContextQt.cpp
    platform/graphics/qt/IconQt.cpp
    platform/graphics/qt/ImageBufferDataQt.cpp
    platform/graphics/qt/ImageBufferQt.cpp
    platform/graphics/qt/ImageDecoderQt.cpp
    platform/graphics/qt/ImageQt.cpp
    platform/graphics/qt/IntPointQt.cpp
    platform/graphics/qt/IntRectQt.cpp
    platform/graphics/qt/IntSizeQt.cpp
    platform/graphics/qt/PathQt.cpp
    platform/graphics/qt/PatternQt.cpp
    platform/graphics/qt/StillImageQt.cpp
    platform/graphics/qt/TileQt.cpp
    platform/graphics/qt/TransformationMatrixQt.cpp

    platform/graphics/surfaces/qt/GraphicsSurfaceQt.cpp

    platform/graphics/x11/PlatformDisplayX11.cpp
    platform/graphics/x11/XUniqueResource.cpp

    platform/network/NetworkStorageSessionStub.cpp
    platform/network/MIMESniffing.cpp

    platform/network/qt/BlobUrlConversion.cpp
    platform/network/qt/ResourceHandleStub.cpp

    platform/qt/CursorQt.cpp
    platform/qt/DataTransferItemListQt.cpp
    platform/qt/DataTransferItemQt.cpp
    platform/qt/DragDataQt.cpp
    platform/qt/DragImageQt.cpp
    platform/qt/EventLoopQt.cpp
    platform/qt/FileSystemQt.cpp
    platform/qt/KeyedDecoderQt.cpp
    platform/qt/KeyedEncoderQt.cpp
    platform/qt/LanguageQt.cpp
    platform/qt/LocalizedStringsQt.cpp
    platform/qt/LoggingQt.cpp
    platform/qt/MainThreadSharedTimerQt.cpp
    platform/qt/MIMETypeRegistryQt.cpp
    platform/qt/PasteboardQt.cpp
    platform/qt/PlatformKeyboardEventQt.cpp
    platform/qt/PlatformScreenQt.cpp
    platform/qt/RenderThemeQt.cpp
    platform/qt/RenderThemeQtMobile.cpp
    platform/qt/ScrollViewQt.cpp
    platform/qt/ScrollbarThemeQt.cpp
    platform/qt/SharedBufferQt.cpp
    platform/qt/SoundQt.cpp
    platform/qt/URLQt.cpp
    platform/qt/WidgetQt.cpp

    platform/text/Hyphenation.cpp
    platform/text/LocaleICU.cpp

    platform/text/qt/TextBreakIteratorInternalICUQt.cpp
)

QTWEBKIT_GENERATE_MOC_FILES_CPP(WebCore
    platform/qt/MainThreadSharedTimerQt.cpp
)

if (COMPILER_IS_GCC_OR_CLANG)
    set_source_files_properties(
        platform/graphics/qt/ImageBufferDataQt.cpp
    PROPERTIES
        COMPILE_FLAGS "-frtti -UQT_NO_DYNAMIC_CAST"
    )

    set_source_files_properties(
        platform/network/qt/BlobUrlConversion.cpp
    PROPERTIES
        COMPILE_FLAGS "-fexceptions -UQT_NO_EXCEPTIONS"
    )
endif ()

if (ENABLE_GAMEPAD_DEPRECATED)
    list(APPEND WebCore_SOURCES
        platform/qt/GamepadsQt.cpp
    )
    QTWEBKIT_GENERATE_MOC_FILES_CPP(WebCore platform/qt/GamepadsQt.cpp)
endif ()

if (ENABLE_GRAPHICS_CONTEXT_3D)
    list(APPEND WebCore_SOURCES
        platform/graphics/qt/GraphicsContext3DQt.cpp
    )
endif ()

if (ENABLE_SMOOTH_SCROLLING)
    list(APPEND WebCore_SOURCES
        platform/ScrollAnimationSmooth.cpp
        platform/ScrollAnimatorSmooth.cpp
    )
endif ()

# Do it in the WebCore to support SHARED_CORE since WebKitWidgets won't load WebKit in that case.
# This should match the opposite statement in WebKit/PlatformQt.cmake
if (SHARED_CORE)
    qt5_add_resources(WebCore_SOURCES
        WebCore.qrc
    )
endif ()

list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
    ${HYPHEN_INCLUDE_DIR}
    ${LIBXML2_INCLUDE_DIR}
    ${LIBXSLT_INCLUDE_DIR}
    ${Qt5Core_INCLUDE_DIRS}
    ${Qt5Gui_INCLUDE_DIRS}
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
)

list(APPEND WebCore_LIBRARIES
    ${LIBXML2_LIBRARIES}
    ${LIBXSLT_LIBRARIES}
    ${Qt5Core_LIBRARIES}
    ${Qt5Gui_LIBRARIES}
    ${X11_X11_LIB}
    ${ZLIB_LIBRARIES}
)

if (QT_STATIC_BUILD)
    list(APPEND WebCore_LIBRARIES
        ${STATIC_LIB_DEPENDENCIES}
    )
endif ()

list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
#    ${WEBCORE_DIR}/css/mediaControlsGtk.css
#    ${WEBCORE_DIR}/css/mediaControlsQt.css
#    ${WEBCORE_DIR}/css/mediaControlsQtFullscreen.css
    ${WEBCORE_DIR}/css/mobileThemeQt.css
    ${WEBCORE_DIR}/css/themeQtNoListboxes.css
)

if (ENABLE_OPENGL)
    list(APPEND WebCore_SOURCES
        platform/graphics/opengl/Extensions3DOpenGLCommon.cpp
        platform/graphics/opengl/GraphicsContext3DOpenGLCommon.cpp
        platform/graphics/opengl/TemporaryOpenGLSetting.cpp

        platform/graphics/qt/QFramebufferPaintDevice.cpp
    )

    if (${Qt5Gui_OPENGL_IMPLEMENTATION} STREQUAL GLESv2)
        list(APPEND WebCore_SOURCES
            platform/graphics/opengl/Extensions3DOpenGLES.cpp
            platform/graphics/opengl/GraphicsContext3DOpenGLES.cpp
        )
        list(APPEND WebCore_LIBRARIES
            ${Qt5Gui_EGL_LIBRARIES}
            ${Qt5Gui_OPENGL_LIBRARIES}
        )
    else ()
        list(APPEND WebCore_SOURCES
            platform/graphics/opengl/Extensions3DOpenGL.cpp
            platform/graphics/opengl/GraphicsContext3DOpenGL.cpp
        )
    endif ()
endif ()

if (USE_GLIB)
    list(APPEND WebCore_SYSTEM_INCLUDE_DIRECTORIES
        ${GIO_UNIX_INCLUDE_DIRS}
        ${GLIB_INCLUDE_DIRS}
    )
    list(APPEND WebCore_LIBRARIES
        ${GLIB_GIO_LIBRARIES}
        ${GLIB_GOBJECT_LIBRARIES}
        ${GLIB_LIBRARIES}
    )
endif ()

if (USE_GSTREAMER)
    include(platform/GStreamer.cmake)
    list(APPEND WebCore_SOURCES
        platform/graphics/gstreamer/ImageGStreamerQt.cpp
    )
endif ()

if (USE_MEDIA_FOUNDATION)
    list(APPEND WebCore_SOURCES
        platform/graphics/win/MediaPlayerPrivateMediaFoundation.cpp
    )
    list(APPEND WebCore_LIBRARIES
        mfuuid
        strmbase
    )
endif ()

if (ENABLE_VIDEO)
    # The <video>/<audio> control shadow DOM is styled by this UA stylesheet.
    list(APPEND WebCore_USER_AGENT_STYLE_SHEETS
        ${WEBCORE_DIR}/Modules/mediacontrols/mediaControlsBase.css
    )
    # mediaControlsBase.js / mediaControlsLocalizedStrings.js (the JS-built controls)
    # were removed with ENABLE_MEDIA_CONTROLS_SCRIPT (Bucket 4 Phase 4) - hipecore
    # uses the C++ MediaControlElements + RenderThemeQt::paintMedia* controls instead.
endif ()

# Build the include path with duplicates removed
list(REMOVE_DUPLICATES WebCore_SYSTEM_INCLUDE_DIRECTORIES)

# TODO: Think how to unify fwd headers handling throughout WebKit
set(WebCore_FORWARDING_HEADERS_DIRECTORIES
    dom
    html
    loader
    page
    platform
    rendering
    storage

    bindings/js

    platform/graphics
    platform/network
    platform/text

    platform/network/qt
)

WEBKIT_CREATE_FORWARDING_HEADERS(WebCore DIRECTORIES ${WebCore_FORWARDING_HEADERS_DIRECTORIES} FILES ${WebCore_FORWARDING_HEADERS_FILES})

# From PlatformWin.cmake

if (WIN32)
    # Eliminate C2139 errors
    if (MSVC)
        add_compile_options(/D_ENABLE_EXTENDED_ALIGNED_STORAGE)
    endif ()

    if (${WTF_LIBRARY_TYPE} MATCHES STATIC)
        # hipecore: was gated on JavaScriptCore_LIBRARY_TYPE (Bucket 5.6/5.7).
        add_definitions(-DSTATICALLY_LINKED_WITH_WTF)
    endif ()

    list(APPEND WebCore_SOURCES
        platform/win/SystemInfo.cpp
    )
endif ()

if (APPLE)
    list(APPEND WebCore_SOURCES
        platform/VNodeTracker.cpp

        platform/cf/SharedBufferCF.cpp
    )
endif ()

# hipecore: upstream Qt/EFL builds generated this by parsing
# Source/WebKit/mac/Configurations/Version.xcconfig (Apple's Xcode build
# config for the Mac port) with a Perl script, so all the CMake-based ports
# shared one canonical version number with the Xcode-based Mac/iOS builds.
# hipecore has no Mac/Xcode build to share a version with: qWebKitVersion() reports
# hipecore's own version (PROJECT_VERSION in Source/cmake/OptionsQt.cmake).
file(WRITE ${DERIVED_SOURCES_WEBCORE_DIR}/WebKitVersion.h
"/* THIS FILE IS GENERATED BY CMAKE, DO NOT EDIT. See PlatformQt.cmake. */
#ifndef WebKitVersion_h
#define WebKitVersion_h

#define WEBKIT_MAJOR_VERSION ${PROJECT_VERSION_MAJOR}
#define WEBKIT_MINOR_VERSION ${PROJECT_VERSION_MINOR}

#endif // WebKitVersion_h
")
list(APPEND WebCore_SOURCES ${DERIVED_SOURCES_WEBCORE_DIR}/WebKitVersion.h)
