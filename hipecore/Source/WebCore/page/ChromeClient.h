/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple, Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef ChromeClient_h
#define ChromeClient_h

#include "AXObjectCache.h"
#include "Cursor.h"
#include "DisplayRefreshMonitor.h"
#include "FocusDirection.h"
#include "FrameLoader.h"
#include "GraphicsContext.h"
#include "HTMLMediaElementEnums.h"
#include "HostWindow.h"
#include "LayerFlushThrottleState.h"
#include "MediaProducer.h"
#include "PageThrottler.h"
#include "PopupMenu.h"
#include "PopupMenuClient.h"
#include "RenderEmbeddedObject.h"
#include "ScrollTypes.h"
#include "ScrollingCoordinator.h"
#include "SearchPopupMenu.h"
#include "WebCoreKeyboardUIMode.h"
#include "ConsoleTypes.h"
#include <wtf/Forward.h>
#include <wtf/Vector.h>

#if ENABLE(MEDIA_SESSION)
namespace WebCore {
class MediaSessionMetadata;
}
#endif

OBJC_CLASS NSResponder;

namespace WebCore {

class AccessibilityObject;
class ColorChooser;
class ColorChooserClient;
class DateTimeChooser;
class DateTimeChooserClient;
class Element;
class FloatRect;
class Frame;
class GraphicsContext3D;
class GraphicsLayer;
class GraphicsLayerFactory;
class HTMLInputElement;
class HTMLMediaElement;
class HTMLVideoElement;
class HitTestResult;
class IntRect;
class NavigationAction;
class Node;
class Page;
class PopupMenuClient;
class SecurityOrigin;
class ViewportConstraints;
class Widget;

#if ENABLE(VIDEO) && USE(GSTREAMER)
class MediaPlayerRequestInstallMissingPluginsCallback;
#endif

struct DateTimeChooserParameters;
struct FrameLoadRequest;
struct GraphicsDeviceAdapter;
struct ViewportArguments;
struct WindowFeatures;

class WEBCORE_EXPORT ChromeClient {
public:
    virtual void chromeDestroyed() = 0;

    virtual void setWindowRect(const FloatRect&) = 0;
    virtual FloatRect windowRect() = 0;

    virtual FloatRect pageRect() = 0;

    virtual void focus() = 0;
    virtual void unfocus() = 0;

    virtual bool canTakeFocus(FocusDirection) = 0;
    virtual void takeFocus(FocusDirection) = 0;

    virtual void focusedElementChanged(Element*) = 0;
    virtual void focusedFrameChanged(Frame*) = 0;

    // The Frame pointer provides the ChromeClient with context about which
    // Frame wants to create the new Page. Also, the newly created window
    // should not be shown to the user until the ChromeClient of the newly
    // created Page has its show method called.
    // The FrameLoadRequest parameter is only for ChromeClient to check if the
    // request could be fulfilled. The ChromeClient should not load the request.
    virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&, const NavigationAction&) = 0;
    virtual void show() = 0;

    virtual bool canRunModal() = 0;
    virtual void runModal() = 0;

    virtual void setToolbarsVisible(bool) = 0;
    virtual bool toolbarsVisible() = 0;

    virtual void setStatusbarVisible(bool) = 0;
    virtual bool statusbarVisible() = 0;

    virtual void setScrollbarsVisible(bool) = 0;
    virtual bool scrollbarsVisible() = 0;

    virtual void setMenubarVisible(bool) = 0;
    virtual bool menubarVisible() = 0;

    virtual void setResizable(bool) = 0;

    virtual void addMessageToConsole(MessageSource, MessageLevel, const String& message, unsigned lineNumber, unsigned columnNumber, const String& sourceID) = 0;

    virtual bool canRunBeforeUnloadConfirmPanel() = 0;
    virtual bool runBeforeUnloadConfirmPanel(const String& message, Frame*) = 0;

    virtual void closeWindowSoon() = 0;

    virtual void runJavaScriptAlert(Frame*, const String&) = 0;
    virtual bool runJavaScriptConfirm(Frame*, const String&) = 0;
    virtual bool runJavaScriptPrompt(Frame*, const String& message, const String& defaultValue, String& result) = 0;
    virtual void setStatusbarText(const String&) = 0;
    virtual KeyboardUIMode keyboardUIMode() = 0;

    // Methods used by HostWindow.
    virtual bool supportsImmediateInvalidation() { return false; }
    virtual void invalidateRootView(const IntRect&) = 0;
    virtual void invalidateContentsAndRootView(const IntRect&) = 0;
    virtual void invalidateContentsForSlowScroll(const IntRect&) = 0;
    virtual void scroll(const IntSize&, const IntRect&, const IntRect&) = 0;
#if USE(COORDINATED_GRAPHICS)
    virtual void delegatedScrollRequested(const IntPoint&) = 0;
#endif
    virtual IntPoint screenToRootView(const IntPoint&) const = 0;
    virtual IntRect rootViewToScreen(const IntRect&) const = 0;
    virtual PlatformPageClient platformPageClient() const = 0;
    virtual void scrollbarsModeDidChange() const = 0;
#if ENABLE(CURSOR_SUPPORT)
    virtual void setCursor(const Cursor&) = 0;
    virtual void setCursorHiddenUntilMouseMoves(bool) = 0;
#endif
#if ENABLE(REQUEST_ANIMATION_FRAME) && !USE(REQUEST_ANIMATION_FRAME_TIMER)
    virtual void scheduleAnimation() = 0;
#endif
    // End methods used by HostWindow.

    virtual FloatSize screenSize() const { return const_cast<ChromeClient*>(this)->windowRect().size(); }
    virtual FloatSize availableScreenSize() const { return const_cast<ChromeClient*>(this)->windowRect().size(); }

    virtual void dispatchViewportPropertiesDidChange(const ViewportArguments&) const { }

    virtual void contentsSizeChanged(Frame*, const IntSize&) const = 0;
    virtual void scrollRectIntoView(const IntRect&) const { }; // Currently only Mac has a non empty implementation.

    virtual bool shouldUnavailablePluginMessageBeButton(RenderEmbeddedObject::PluginUnavailabilityReason) const { return false; }
    virtual void unavailablePluginButtonClicked(Element*, RenderEmbeddedObject::PluginUnavailabilityReason) const { }
    virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags) = 0;

    virtual void setToolTip(const String&, TextDirection) = 0;

    virtual void print(Frame*) = 0;

    virtual Color underlayColor() const { return Color(); }

    virtual void pageExtendedBackgroundColorDidChange(Color) const { }

#if ENABLE(DASHBOARD_SUPPORT)
    virtual void annotatedRegionsChanged();
#endif

    virtual bool shouldReplaceWithGeneratedFileForUpload(const String& path, String& generatedFilename);
    virtual String generateReplacementFile(const String& path);

#if ENABLE(IOS_TOUCH_EVENTS)
    virtual void didPreventDefaultForEvent() = 0;
#endif

    virtual std::chrono::milliseconds eventThrottlingDelay() { return std::chrono::milliseconds::zero(); };

#if ENABLE(ORIENTATION_EVENTS)
    virtual int deviceOrientation() const = 0;
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    virtual std::unique_ptr<ColorChooser> createColorChooser(ColorChooserClient*, const Color&) = 0;
#endif        
    virtual void elementDidFocus(const Node*) { };
    virtual void elementDidBlur(const Node*) { };
    virtual void elementDidRefocus(const Node*) { };
    
    virtual bool shouldPaintEntireContents() const { return false; }
    virtual bool hasStablePageScaleFactor() const { return true; }

    // Allows ports to customize the type of graphics layers created by this page.
    virtual GraphicsLayerFactory* graphicsLayerFactory() const { return nullptr; }

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    virtual RefPtr<DisplayRefreshMonitor> createDisplayRefreshMonitor(PlatformDisplayID) const { return nullptr; }
#endif

    // Pass 0 as the GraphicsLayer to detatch the root layer.
    virtual void attachRootGraphicsLayer(Frame*, GraphicsLayer*) = 0;
    virtual void attachViewOverlayGraphicsLayer(Frame*, GraphicsLayer*) = 0;
    // Sets a flag to specify that the next time content is drawn to the window,
    // the changes appear on the screen in synchrony with updates to GraphicsLayers.
    virtual void setNeedsOneShotDrawingSynchronization() = 0;
    // Sets a flag to specify that the view needs to be updated, so we need
    // to do an eager layout before the drawing.
    virtual void scheduleCompositingLayerFlush() = 0;
    // Returns whether or not the client can render the composited layer,
    // regardless of the settings.
    virtual bool allowsAcceleratedCompositing() const { return true; }

    enum CompositingTrigger {
        ThreeDTransformTrigger = 1 << 0,
        VideoTrigger = 1 << 1,
        PluginTrigger = 1 << 2,
        CanvasTrigger = 1 << 3,
        AnimationTrigger = 1 << 4,
        FilterTrigger = 1 << 5,
        ScrollableInnerFrameTrigger = 1 << 6,
        AnimatedOpacityTrigger = 1 << 7,
        AllTriggers = 0xFFFFFFFF
    };
    typedef unsigned CompositingTriggerFlags;

    // Returns a bitfield indicating conditions that can trigger the compositor.
    virtual CompositingTriggerFlags allowedCompositingTriggers() const { return static_cast<CompositingTriggerFlags>(AllTriggers); }
    
    // Returns true if layer tree updates are disabled.
    virtual bool layerTreeStateIsFrozen() const { return false; }

    virtual bool adjustLayerFlushThrottling(LayerFlushThrottleState::Flags) { return false; }

    virtual PassRefPtr<ScrollingCoordinator> createScrollingCoordinator(Page*) const { return nullptr; }

#if PLATFORM(WIN) && USE(AVFOUNDATION)
    virtual GraphicsDeviceAdapter* graphicsDeviceAdapter() const { return 0; }
#endif

    virtual bool supportsVideoFullscreen(HTMLMediaElementEnums::VideoFullscreenMode) { return false; }
#if ENABLE(VIDEO)
    virtual void enterVideoFullscreenForVideoElement(HTMLVideoElement&, HTMLMediaElementEnums::VideoFullscreenMode) { }
#endif
    virtual void exitVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&) { }
    virtual bool requiresFullscreenForVideoPlayback() { return false; } 

#if USE(COORDINATED_GRAPHICS)
    virtual IntRect visibleRectForTiledBackingStore() const { return IntRect(); }
#endif

#if PLATFORM(COCOA)
    virtual NSResponder *firstResponder() { return 0; }
    virtual void makeFirstResponder(NSResponder *) { }
    // Focuses on the containing view associated with this page.
    virtual void makeFirstResponder() { }
#endif

    virtual void enableSuddenTermination() { }
    virtual void disableSuddenTermination() { }

#if PLATFORM(WIN)
    virtual void setLastSetCursorToCurrentCursor() = 0;
    virtual void AXStartFrameLoad() = 0;
    virtual void AXFinishFrameLoad() = 0;
#endif

#if ENABLE(TOUCH_EVENTS)
    virtual void needTouchEvents(bool) = 0;
#endif

    virtual bool selectItemWritingDirectionIsNatural() = 0;
    virtual bool selectItemAlignmentFollowsMenuWritingDirection() = 0;
    // Checks if there is an opened popup, called by RenderMenuList::showPopup().
    virtual bool hasOpenedPopup() const = 0;
    virtual RefPtr<PopupMenu> createPopupMenu(PopupMenuClient*) const = 0;
    virtual RefPtr<SearchPopupMenu> createSearchPopupMenu(PopupMenuClient*) const = 0;

    virtual void postAccessibilityNotification(AccessibilityObject*, AXObjectCache::AXNotification) { }

    virtual void notifyScrollerThumbIsVisibleInRect(const IntRect&) { }
    virtual void recommendedScrollbarStyleDidChange(ScrollbarStyle) { }

    virtual WTF::Optional<ScrollbarOverlayStyle> preferredScrollbarOverlayStyle() { return ScrollbarOverlayStyleDefault; }

    virtual void wheelEventHandlersChanged(bool hasHandlers) = 0;
        
    virtual bool isSVGImageChromeClient() const { return false; }

#if ENABLE(POINTER_LOCK)
    virtual bool requestPointerLock() { return false; }
    virtual void requestPointerUnlock() { }
    virtual bool isPointerLocked() { return false; }
#endif

    virtual FloatSize minimumWindowSize() const { return FloatSize(100, 100); };

    virtual bool isEmptyChromeClient() const { return false; }

    virtual void didAssociateFormControls(const Vector<RefPtr<Element>>&) { };
    virtual bool shouldNotifyOnFormChanges() { return false; };

    virtual void didAddHeaderLayer(GraphicsLayer*) { }
    virtual void didAddFooterLayer(GraphicsLayer*) { }

    virtual bool shouldUseTiledBackingForFrameView(const FrameView*) const { return false; }

    virtual void isPlayingMediaDidChange(MediaProducer::MediaStateFlags, uint64_t) { }

#if ENABLE(MEDIA_SESSION)
    virtual void hasMediaSessionWithActiveMediaElementsDidChange(bool) { }
    virtual void mediaSessionMetadataDidChange(const WebCore::MediaSessionMetadata&) { }
    virtual void focusedContentMediaElementDidChange(uint64_t) { }
#endif

    virtual void setPageActivityState(PageActivityState::Flags) { }

#if ENABLE(SUBTLE_CRYPTO)
    virtual bool wrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) const { return false; }
    virtual bool unwrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) const { return false; }
#endif

#if ENABLE(SERVICE_CONTROLS)
    virtual void handleSelectionServiceClick(WebCore::FrameSelection&, const Vector<String>&, const WebCore::IntPoint&) { }
    virtual bool hasRelevantSelectionServices(bool /* isTextOnly */) const { return false; }
#endif

    virtual bool shouldDispatchFakeMouseMoveEvents() const { return true; }

    virtual void handleAutoFillButtonClick(HTMLInputElement&) { }

    virtual void imageOrMediaDocumentSizeChanged(const WebCore::IntSize&) { }

#if ENABLE(VIDEO) && USE(GSTREAMER)
    virtual void requestInstallMissingMediaPlugins(const String& /*details*/, const String& /*description*/, MediaPlayerRequestInstallMissingPluginsCallback&) { }
#endif

    virtual void didInvalidateDocumentMarkerRects() { }

protected:
    virtual ~ChromeClient() { }
};

}
#endif // ChromeClient_h
