/*
 * Copyright (C) 2006, 2007, 2008, 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2025-2026 General Development Systems
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DOMWindow.h"

#include "BeforeUnloadEvent.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "ContentExtensionActions.h"
#include "ContentExtensionRule.h"
#include "Crypto.h"
#include "DOMStringList.h"
#include "DOMTokenList.h"
#include "DOMURL.h"
#include "DOMWindowCSS.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "Element.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "ExceptionCodePlaceholder.h"
#include "FloatRect.h"
#include "FocusController.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLFrameOwnerElement.h"
#include "MainFrame.h"
#include "MediaQueryList.h"
#include "MediaQueryMatcher.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "PageGroup.h"
#include "PageTransitionEvent.h"
#include "PlatformScreen.h"
#include "ResourceLoadInfo.h"
#include "RuntimeEnabledFeatures.h"
#include "SecurityOrigin.h"
#include "SecurityPolicy.h"
#include "Settings.h"
#include "StyleMedia.h"
#include "StyleResolver.h"
#include "UserGestureIndicator.h"
#include "SuddenTermination.h"
#include "URL.h"
#include "WindowFeatures.h"
#include "WindowFocusAllowedIndicator.h"
#include <algorithm>
#include <memory>
#include <wtf/CurrentTime.h>
#include <wtf/MainThread.h>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/text/Base64.h>
#include <wtf/text/WTFString.h>

#if ENABLE(REQUEST_ANIMATION_FRAME)
#include "RequestAnimationFrameCallback.h"
#endif

#if ENABLE(GAMEPAD)
#include "GamepadManager.h"
#endif

namespace WebCore {


typedef HashCountedSet<DOMWindow*> DOMWindowSet;

static DOMWindowSet& windowsWithUnloadEventListeners()
{
    static NeverDestroyed<DOMWindowSet> windowsWithUnloadEventListeners;
    return windowsWithUnloadEventListeners;
}

static DOMWindowSet& windowsWithBeforeUnloadEventListeners()
{
    static NeverDestroyed<DOMWindowSet> windowsWithBeforeUnloadEventListeners;
    return windowsWithBeforeUnloadEventListeners;
}

static void addUnloadEventListener(DOMWindow* domWindow)
{
    if (windowsWithUnloadEventListeners().add(domWindow).isNewEntry)
        domWindow->disableSuddenTermination();
}

static void removeUnloadEventListener(DOMWindow* domWindow)
{
    if (windowsWithUnloadEventListeners().remove(domWindow))
        domWindow->enableSuddenTermination();
}

static void removeAllUnloadEventListeners(DOMWindow* domWindow)
{
    if (windowsWithUnloadEventListeners().removeAll(domWindow))
        domWindow->enableSuddenTermination();
}

static void addBeforeUnloadEventListener(DOMWindow* domWindow)
{
    if (windowsWithBeforeUnloadEventListeners().add(domWindow).isNewEntry)
        domWindow->disableSuddenTermination();
}

static void removeBeforeUnloadEventListener(DOMWindow* domWindow)
{
    if (windowsWithBeforeUnloadEventListeners().remove(domWindow))
        domWindow->enableSuddenTermination();
}

static void removeAllBeforeUnloadEventListeners(DOMWindow* domWindow)
{
    if (windowsWithBeforeUnloadEventListeners().removeAll(domWindow))
        domWindow->enableSuddenTermination();
}

static bool allowsBeforeUnloadListeners(DOMWindow* window)
{
    ASSERT_ARG(window, window);
    Frame* frame = window->frame();
    if (!frame)
        return false;
    if (!frame->page())
        return false;
    return frame->isMainFrame();
}

bool DOMWindow::dispatchAllPendingBeforeUnloadEvents()
{
    DOMWindowSet& set = windowsWithBeforeUnloadEventListeners();
    if (set.isEmpty())
        return true;

    static bool alreadyDispatched = false;
    ASSERT(!alreadyDispatched);
    if (alreadyDispatched)
        return true;

    Vector<Ref<DOMWindow>> windows;
    windows.reserveInitialCapacity(set.size());
    for (auto& window : set)
        windows.uncheckedAppend(*window.key);

    for (auto& window : windows) {
        if (!set.contains(window.ptr()))
            continue;

        Frame* frame = window->frame();
        if (!frame)
            continue;

        if (!frame->loader().shouldClose())
            return false;

        window->enableSuddenTermination();
    }

    alreadyDispatched = true;
    return true;
}

unsigned DOMWindow::pendingUnloadEventListeners() const
{
    return windowsWithUnloadEventListeners().count(const_cast<DOMWindow*>(this));
}

void DOMWindow::dispatchAllPendingUnloadEvents()
{
    DOMWindowSet& set = windowsWithUnloadEventListeners();
    if (set.isEmpty())
        return;

    static bool alreadyDispatched = false;
    ASSERT(!alreadyDispatched);
    if (alreadyDispatched)
        return;

    Vector<Ref<DOMWindow>> windows;
    windows.reserveInitialCapacity(set.size());
    for (auto& keyValue : set)
        windows.uncheckedAppend(*keyValue.key);

    for (auto& window : windows) {
        if (!set.contains(window.ptr()))
            continue;

        window->dispatchEvent(PageTransitionEvent::create(eventNames().pagehideEvent, false), window->document());
        window->dispatchEvent(Event::create(eventNames().unloadEvent, false, false), window->document());

        window->enableSuddenTermination();
    }

    alreadyDispatched = true;
}

// This function:
// 1) Validates the pending changes are not changing any value to NaN; in that case keep original value.
// 2) Constrains the window rect to the minimum window size and no bigger than the float rect's dimensions.
// 3) Constrains the window rect to within the top and left boundaries of the available screen rect.
// 4) Constrains the window rect to within the bottom and right boundaries of the available screen rect.
// 5) Translate the window rect coordinates to be within the coordinate space of the screen.
FloatRect DOMWindow::adjustWindowRect(Page* page, const FloatRect& pendingChanges)
{
    ASSERT(page);

    FloatRect screen = screenAvailableRect(page->mainFrame().view());
    FloatRect window = page->chrome().windowRect();

    // Make sure we're in a valid state before adjusting dimensions.
    ASSERT(std::isfinite(screen.x()));
    ASSERT(std::isfinite(screen.y()));
    ASSERT(std::isfinite(screen.width()));
    ASSERT(std::isfinite(screen.height()));
    ASSERT(std::isfinite(window.x()));
    ASSERT(std::isfinite(window.y()));
    ASSERT(std::isfinite(window.width()));
    ASSERT(std::isfinite(window.height()));

    // Update window values if new requested values are not NaN.
    if (!std::isnan(pendingChanges.x()))
        window.setX(pendingChanges.x());
    if (!std::isnan(pendingChanges.y()))
        window.setY(pendingChanges.y());
    if (!std::isnan(pendingChanges.width()))
        window.setWidth(pendingChanges.width());
    if (!std::isnan(pendingChanges.height()))
        window.setHeight(pendingChanges.height());

    FloatSize minimumSize = page->chrome().client().minimumWindowSize();
    window.setWidth(std::min(std::max(minimumSize.width(), window.width()), screen.width()));
    window.setHeight(std::min(std::max(minimumSize.height(), window.height()), screen.height()));

    // Constrain the window position within the valid screen area.
    window.setX(std::max(screen.x(), std::min(window.x(), screen.maxX() - window.width())));
    window.setY(std::max(screen.y(), std::min(window.y(), screen.maxY() - window.height())));

    return window;
}

bool DOMWindow::allowPopUp(Frame* firstFrame)
{
    ASSERT(firstFrame);

    if (UserGestureIndicator::processingUserGesture())
        return true;

    return firstFrame->settings().javaScriptCanOpenWindowsAutomatically();
}

bool DOMWindow::allowPopUp()
{
    return m_frame && allowPopUp(m_frame);
}

bool DOMWindow::canShowModalDialog(const Frame* frame)
{
    if (!frame)
        return false;

    // Override support for layout testing purposes.
    if (auto* document = frame->document()) {
        if (auto* window = document->domWindow()) {
            if (window->m_canShowModalDialogOverride)
                return window->m_canShowModalDialogOverride.value();
        }
    }

    auto* page = frame->page();
    return page ? page->chrome().canRunModal() : false;
}

void DOMWindow::setCanShowModalDialogOverride(bool allow)
{
    m_canShowModalDialogOverride = allow;
}

DOMWindow::DOMWindow(Document* document)
    : ContextDestructionObserver(document)
    , FrameDestructionObserver(document->frame())
    , m_shouldPrintWhenFinishedLoading(false)
    , m_lastPageStatus(PageStatusNone)
    , m_weakPtrFactory(this)
#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS)
    , m_touchEventListenerCount(0)
#endif
#if ENABLE(GAMEPAD)
    , m_gamepadEventListenerCount(0)
#endif
{
    ASSERT(frame());
    ASSERT(DOMWindow::document());
}

void DOMWindow::didSecureTransitionTo(Document* document)
{
    observeContext(document);
}

DOMWindow::~DOMWindow()
{
#ifndef NDEBUG
    ASSERT(!m_crypto);
    ASSERT(!m_media);
#endif

    willDestroyDocumentInFrame();

    // We don't want to risk any of these objects hanging around after we've been destroyed.
    resetDOMWindowProperties();

    removeAllUnloadEventListeners(this);
    removeAllBeforeUnloadEventListeners(this);

#if ENABLE(GAMEPAD)
    if (m_gamepadEventListenerCount)
        GamepadManager::singleton().unregisterDOMWindow(this);
#endif
}

DOMWindow* DOMWindow::toDOMWindow()
{
    return this;
}

PassRefPtr<MediaQueryList> DOMWindow::matchMedia(const String& media)
{
    return document() ? document()->mediaQueryMatcher().matchMedia(media) : 0;
}

Page* DOMWindow::page()
{
    return frame() ? frame()->page() : 0;
}

void DOMWindow::frameDestroyed()
{
    Ref<DOMWindow> protectedThis(*this);

    willDestroyDocumentInFrame();
    FrameDestructionObserver::frameDestroyed();
    resetDOMWindowProperties();
}

void DOMWindow::willDetachPage()
{
    //no-op
}

void DOMWindow::willDestroyDocumentInFrame()
{
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to willDestroyGlobalObjectInFrame.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (auto& property : properties)
        property->willDestroyGlobalObjectInFrame();
}

void DOMWindow::willDetachDocumentFromFrame()
{
    // It is necessary to copy m_properties to a separate vector because the DOMWindowProperties may
    // unregister themselves from the DOMWindow as a result of the call to willDetachGlobalObjectFromFrame.
    Vector<DOMWindowProperty*> properties;
    copyToVector(m_properties, properties);
    for (auto& property : properties)
        property->willDetachGlobalObjectFromFrame();
}

#if ENABLE(GAMEPAD)
void DOMWindow::incrementGamepadEventListenerCount()
{
    if (++m_gamepadEventListenerCount == 1)
        GamepadManager::singleton().registerDOMWindow(this);
}

void DOMWindow::decrementGamepadEventListenerCount()
{
    ASSERT(m_gamepadEventListenerCount);

    if (!--m_gamepadEventListenerCount)
        GamepadManager::singleton().unregisterDOMWindow(this);
}
#endif

void DOMWindow::registerProperty(DOMWindowProperty* property)
{
    m_properties.add(property);
}

void DOMWindow::unregisterProperty(DOMWindowProperty* property)
{
    m_properties.remove(property);
}

void DOMWindow::resetUnlessSuspendedForDocumentSuspension()
{
    willDestroyDocumentInFrame();
    resetDOMWindowProperties();
}

void DOMWindow::resetDOMWindowProperties()
{
    m_properties.clear();

    m_crypto = nullptr;
    m_media = nullptr;
}

bool DOMWindow::isCurrentlyDisplayedInFrame() const
{
    return m_frame && m_frame->document()->domWindow() == this;
}

#if ENABLE(ORIENTATION_EVENTS)
int DOMWindow::orientation() const
{
    if (!m_frame)
        return 0;

    return m_frame->orientation();
}
#endif

Crypto* DOMWindow::crypto() const
{
    // FIXME: Why is crypto not available when the window is not currently displayed in a frame?
    if (!isCurrentlyDisplayedInFrame())
        return 0;
    if (!m_crypto)
        m_crypto = Crypto::create(*document());
    return m_crypto.get();
}

PageConsoleClient* DOMWindow::console() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    return m_frame->page() ? &m_frame->page()->console() : nullptr;
}

Element* DOMWindow::frameElement() const
{
    if (!m_frame)
        return 0;

    return m_frame->ownerElement();
}

void DOMWindow::focus(ScriptExecutionContext* context)
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    bool allowFocus = WindowFocusAllowedIndicator::windowFocusAllowed() || !m_frame->settings().windowFocusRestricted();
    if (context) {
        ASSERT(isMainThread());
        Document& activeDocument = downcast<Document>(*context);
        if (opener() && opener() != this && activeDocument.domWindow() == opener())
            allowFocus = true;
    }

    // If we're a top level window, bring the window to the front.
    if (m_frame->isMainFrame() && allowFocus)
        page->chrome().focus();

    if (!m_frame)
        return;

    // Clear the current frame's focused node if a new frame is about to be focused.
    Frame* focusedFrame = page->focusController().focusedFrame();
    if (focusedFrame && focusedFrame != m_frame)
        focusedFrame->document()->setFocusedElement(nullptr);

    m_frame->eventHandler().focusDocumentView();
}

void DOMWindow::blur()
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (m_frame->settings().windowFocusRestricted())
        return;

    if (!m_frame->isMainFrame())
        return;

    page->chrome().unfocus();
}

void DOMWindow::close(ScriptExecutionContext* context)
{
    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    if (!m_frame->isMainFrame())
        return;

    if (context) {
        ASSERT(isMainThread());
        if (!downcast<Document>(*context).canNavigate(m_frame))
            return;
    }

    bool allowScriptsToCloseWindows = m_frame->settings().allowScriptsToCloseWindows();

    if (!m_frame->loader().shouldClose())
        return;

    page->setIsClosing();
    page->chrome().closeWindowSoon();
}

void DOMWindow::print()
{
    if (!m_frame)
        return;

    auto* page = m_frame->page();
    if (!page)
        return;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.print is not allowed while unloading a page.");
        return;
    }

    if (m_frame->loader().activeDocumentLoader()->isLoading()) {
        m_shouldPrintWhenFinishedLoading = true;
        return;
    }
    m_shouldPrintWhenFinishedLoading = false;
    page->chrome().print(m_frame);
}

void DOMWindow::stop()
{
    if (!m_frame)
        return;

    // We must check whether the load is complete asynchronously, because we might still be parsing
    // the document until the callstack unwinds.
    m_frame->loader().stopForUserCancel(true);
}

void DOMWindow::alert(const String& message)
{
    if (!m_frame)
        return;

    auto* page = m_frame->page();
    if (!page)
        return;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.alert is not allowed while unloading a page.");
        return;
    }

    m_frame->document()->updateStyleIfNeeded();

    page->chrome().runJavaScriptAlert(m_frame, message);
}

bool DOMWindow::confirm(const String& message)
{
    if (!m_frame)
        return false;
    
    auto* page = m_frame->page();
    if (!page)
        return false;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.confirm is not allowed while unloading a page.");
        return false;
    }

    m_frame->document()->updateStyleIfNeeded();

    return page->chrome().runJavaScriptConfirm(m_frame, message);
}

String DOMWindow::prompt(const String& message, const String& defaultValue)
{
    if (!m_frame)
        return String();

    auto* page = m_frame->page();
    if (!page)
        return String();

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.prompt is not allowed while unloading a page.");
        return String();
    }

    m_frame->document()->updateStyleIfNeeded();

    String returnValue;
    if (page->chrome().runJavaScriptPrompt(m_frame, message, defaultValue, returnValue))
        return returnValue;

    return String();
}

String DOMWindow::btoa(const String& stringToEncode, ExceptionCode& ec)
{
    if (stringToEncode.isNull())
        return String();

    if (!stringToEncode.containsOnlyLatin1()) {
        ec = INVALID_CHARACTER_ERR;
        return String();
    }

    return base64Encode(stringToEncode.latin1());
}

String DOMWindow::atob(const String& encodedString, ExceptionCode& ec)
{
    if (encodedString.isNull())
        return String();

    if (!encodedString.containsOnlyLatin1()) {
        ec = INVALID_CHARACTER_ERR;
        return String();
    }

    Vector<char> out;
    if (!base64Decode(encodedString, out, Base64ValidatePadding | Base64IgnoreSpacesAndNewLines)) {
        ec = INVALID_CHARACTER_ERR;
        return String();
    }

    return String(out.data(), out.size());
}

bool DOMWindow::find(const String& string, bool caseSensitive, bool backwards, bool wrap, bool /*wholeWord*/, bool /*searchInFrames*/, bool /*showDialog*/) const
{
    if (!isCurrentlyDisplayedInFrame())
        return false;

    // FIXME (13016): Support wholeWord, searchInFrames and showDialog.    
    FindOptions options = (backwards ? Backwards : 0) | (caseSensitive ? 0 : CaseInsensitive) | (wrap ? WrapAround : 0);
    return m_frame->editor().findString(string, options);
}

bool DOMWindow::offscreenBuffering() const
{
    return true;
}

int DOMWindow::outerHeight() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().height());
}

int DOMWindow::outerWidth() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().width());
}

int DOMWindow::innerHeight() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    return view->mapFromLayoutToCSSUnits(static_cast<int>(view->unobscuredContentRectIncludingScrollbars().height()));
}

int DOMWindow::innerWidth() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    return view->mapFromLayoutToCSSUnits(static_cast<int>(view->unobscuredContentRectIncludingScrollbars().width()));
}

int DOMWindow::screenX() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().x());
}

int DOMWindow::screenY() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return static_cast<int>(page->chrome().windowRect().y());
}

int DOMWindow::scrollX() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    int scrollX = view->contentsScrollPosition().x();
    if (!scrollX)
        return 0;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    return view->mapFromLayoutToCSSUnits(view->contentsScrollPosition().x());
}

int DOMWindow::scrollY() const
{
    if (!m_frame)
        return 0;

    FrameView* view = m_frame->view();
    if (!view)
        return 0;

    int scrollY = view->contentsScrollPosition().y();
    if (!scrollY)
        return 0;

    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    return view->mapFromLayoutToCSSUnits(view->contentsScrollPosition().y());
}

bool DOMWindow::closed() const
{
    return !m_frame;
}

unsigned DOMWindow::length() const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    return m_frame->tree().scopedChildCount();
}

String DOMWindow::name() const
{
    if (!m_frame)
        return String();

    return m_frame->tree().name();
}

void DOMWindow::setName(const String& string)
{
    if (!m_frame)
        return;

    m_frame->tree().setName(string);
}

void DOMWindow::setStatus(const String& string) 
{
    m_status = string;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    ASSERT(m_frame->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    page->chrome().setStatusbarText(m_frame, m_status);
} 
    
void DOMWindow::setDefaultStatus(const String& string) 
{
    m_defaultStatus = string;

    if (!m_frame)
        return;

    Page* page = m_frame->page();
    if (!page)
        return;

    ASSERT(m_frame->document()); // Client calls shouldn't be made when the frame is in inconsistent state.
    page->chrome().setStatusbarText(m_frame, m_defaultStatus);
}

DOMWindow* DOMWindow::self() const
{
    if (!m_frame)
        return 0;

    return m_frame->document()->domWindow();
}

DOMWindow* DOMWindow::opener() const
{
    if (!m_frame)
        return 0;

    Frame* opener = m_frame->loader().opener();
    if (!opener)
        return 0;

    return opener->document()->domWindow();
}

DOMWindow* DOMWindow::parent() const
{
    if (!m_frame)
        return 0;

    Frame* parent = m_frame->tree().parent();
    if (parent)
        return parent->document()->domWindow();

    return m_frame->document()->domWindow();
}

DOMWindow* DOMWindow::top() const
{
    if (!m_frame)
        return 0;

    Page* page = m_frame->page();
    if (!page)
        return 0;

    return m_frame->tree().top().document()->domWindow();
}

Document* DOMWindow::document() const
{
    ScriptExecutionContext* context = ContextDestructionObserver::scriptExecutionContext();
    return downcast<Document>(context);
}

PassRefPtr<StyleMedia> DOMWindow::styleMedia() const
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;
    if (!m_media)
        m_media = StyleMedia::create(m_frame);
    return m_media.get();
}

PassRefPtr<CSSStyleDeclaration> DOMWindow::getComputedStyle(Element* element, const String& pseudoElt) const
{
    if (!element)
        return nullptr;

    return CSSComputedStyleDeclaration::create(element, false, pseudoElt);
}

PassRefPtr<CSSRuleList> DOMWindow::getMatchedCSSRules(Element* element, const String& pseudoElement, bool authorOnly) const
{
    if (!isCurrentlyDisplayedInFrame())
        return 0;

    unsigned colonStart = pseudoElement[0] == ':' ? (pseudoElement[1] == ':' ? 2 : 1) : 0;
    CSSSelector::PseudoElementType pseudoType = CSSSelector::parsePseudoElementType(pseudoElement.substringSharingImpl(colonStart));
    if (pseudoType == CSSSelector::PseudoElementUnknown && !pseudoElement.isEmpty())
        return 0;

    unsigned rulesToInclude = StyleResolver::AuthorCSSRules;
    if (!authorOnly)
        rulesToInclude |= StyleResolver::UAAndUserCSSRules;
    if (m_frame->settings().crossOriginCheckInGetMatchedCSSRulesDisabled())
        rulesToInclude |= StyleResolver::CrossOriginCSSRules;

    PseudoId pseudoId = CSSSelector::pseudoId(pseudoType);

    auto matchedRules = m_frame->document()->ensureStyleResolver().pseudoStyleRulesForElement(element, pseudoId, rulesToInclude);
    if (matchedRules.isEmpty())
        return 0;

    RefPtr<StaticCSSRuleList> ruleList = StaticCSSRuleList::create();
    for (auto& rule : matchedRules)
        ruleList->rules().append(rule->createCSSOMWrapper());

    return ruleList.release();
}

double DOMWindow::devicePixelRatio() const
{
    if (!m_frame)
        return 0.0;

    Page* page = m_frame->page();
    if (!page)
        return 0.0;

    return page->deviceScaleFactor();
}

void DOMWindow::scrollBy(int x, int y) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    FrameView* view = m_frame->view();
    if (!view)
        return;

    IntSize scaledOffset(view->mapFromCSSToLayoutUnits(x), view->mapFromCSSToLayoutUnits(y));
    view->setContentsScrollPosition(view->contentsScrollPosition() + scaledOffset);
}

void DOMWindow::scrollTo(int x, int y) const
{
    if (!isCurrentlyDisplayedInFrame())
        return;

    RefPtr<FrameView> view = m_frame->view();
    if (!view)
        return;

    if (!x && !y && view->contentsScrollPosition() == IntPoint(0, 0))
        return;

    document()->updateLayoutIgnorePendingStylesheets();

    IntPoint layoutPos(view->mapFromCSSToLayoutUnits(x), view->mapFromCSSToLayoutUnits(y));
    view->setContentsScrollPosition(layoutPos);
}

bool DOMWindow::allowedToChangeWindowGeometry() const
{
    if (!m_frame)
        return false;
    if (!m_frame->page())
        return false;
    if (!m_frame->isMainFrame())
        return false;
    // Prevent web content from tricking the user into initiating a drag.
    if (m_frame->eventHandler().mousePressed())
        return false;
    return true;
}

void DOMWindow::moveBy(float x, float y) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    Page* page = m_frame->page();
    FloatRect fr = page->chrome().windowRect();
    FloatRect update = fr;
    update.move(x, y);
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    page->chrome().setWindowRect(adjustWindowRect(page, update));
}

void DOMWindow::moveTo(float x, float y) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    Page* page = m_frame->page();
    FloatRect fr = page->chrome().windowRect();
    FloatRect sr = screenAvailableRect(page->mainFrame().view());
    fr.setLocation(sr.location());
    FloatRect update = fr;
    update.move(x, y);
    // Security check (the spec talks about UniversalBrowserWrite to disable this check...)
    page->chrome().setWindowRect(adjustWindowRect(page, update));
}

void DOMWindow::resizeBy(float x, float y) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    Page* page = m_frame->page();
    FloatRect fr = page->chrome().windowRect();
    FloatSize dest = fr.size() + FloatSize(x, y);
    FloatRect update(fr.location(), dest);
    page->chrome().setWindowRect(adjustWindowRect(page, update));
}

void DOMWindow::resizeTo(float width, float height) const
{
    if (!allowedToChangeWindowGeometry())
        return;

    Page* page = m_frame->page();
    FloatRect fr = page->chrome().windowRect();
    FloatSize dest = FloatSize(width, height);
    FloatRect update(fr.location(), dest);
    page->chrome().setWindowRect(adjustWindowRect(page, update));
}

// hipecore: window.setTimeout/setInterval/clearTimeout/clearInterval removed with the
// DOMTimer/ScheduledAction stack - they only ever ran JS callbacks or eval'd code strings.

#if ENABLE(REQUEST_ANIMATION_FRAME)
int DOMWindow::requestAnimationFrame(PassRefPtr<RequestAnimationFrameCallback> callback)
{
    callback->m_useLegacyTimeBase = false;
    if (Document* d = document())
        return d->requestAnimationFrame(callback);
    return 0;
}

int DOMWindow::webkitRequestAnimationFrame(PassRefPtr<RequestAnimationFrameCallback> callback)
{
    callback->m_useLegacyTimeBase = true;
    if (Document* d = document())
        return d->requestAnimationFrame(callback);
    return 0;
}

void DOMWindow::cancelAnimationFrame(int id)
{
    if (Document* d = document())
        d->cancelAnimationFrame(id);
}
#endif

DOMWindowCSS* DOMWindow::css()
{
    if (!m_css)
        m_css = DOMWindowCSS::create();
    return m_css.get();
}

bool DOMWindow::isSameSecurityOriginAsMainFrame() const
{
    if (!m_frame || !m_frame->page() || !document())
        return false;

    if (m_frame->isMainFrame())
        return true;

    Document* mainFrameDocument = m_frame->mainFrame().document();

    if (mainFrameDocument && document()->securityOrigin()->canAccess(mainFrameDocument->securityOrigin()))
        return true;

    return false;
}

bool DOMWindow::addEventListener(const AtomicString& eventType, RefPtr<EventListener>&& listener, bool useCapture)
{
    if (!EventTarget::addEventListener(eventType, WTFMove(listener), useCapture))
        return false;

    if (Document* document = this->document()) {
        document->addListenerTypeIfNeeded(eventType);
        if (eventNames().isWheelEventType(eventType))
            document->didAddWheelEventHandler(*document);
        else if (eventNames().isTouchEventType(eventType))
            document->didAddTouchEventHandler(*document);
    }

    if (eventType == eventNames().unloadEvent)
        addUnloadEventListener(this);
    else if (eventType == eventNames().beforeunloadEvent && allowsBeforeUnloadListeners(this))
        addBeforeUnloadEventListener(this);
#if ENABLE(IOS_TOUCH_EVENTS)
    else if (eventNames().isTouchEventType(eventType))
        ++m_touchEventListenerCount;
#endif
#if ENABLE(IOS_GESTURE_EVENTS)
    else if (eventNames().isGestureEventType(eventType))
        ++m_touchEventListenerCount;
#endif
#if ENABLE(GAMEPAD)
    else if (eventNames().isGamepadEventType(eventType))
        incrementGamepadEventListenerCount();
#endif

    return true;
}

bool DOMWindow::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    if (!EventTarget::removeEventListener(eventType, listener, useCapture))
        return false;

    if (Document* document = this->document()) {
        if (eventNames().isWheelEventType(eventType))
            document->didRemoveWheelEventHandler(*document);
        else if (eventNames().isTouchEventType(eventType))
            document->didRemoveTouchEventHandler(*document);
    }

    if (eventType == eventNames().unloadEvent)
        removeUnloadEventListener(this);
    else if (eventType == eventNames().beforeunloadEvent && allowsBeforeUnloadListeners(this))
        removeBeforeUnloadEventListener(this);
#if ENABLE(IOS_TOUCH_EVENTS)
    else if (eventNames().isTouchEventType(eventType)) {
        ASSERT(m_touchEventListenerCount > 0);
        --m_touchEventListenerCount;
    }
#endif
#if ENABLE(IOS_GESTURE_EVENTS)
    else if (eventNames().isGestureEventType(eventType)) {
        ASSERT(m_touchEventListenerCount > 0);
        --m_touchEventListenerCount;
    }
#endif
#if ENABLE(GAMEPAD)
    else if (eventNames().isGamepadEventType(eventType))
        decrementGamepadEventListenerCount();
#endif

    return true;
}

void DOMWindow::dispatchLoadEvent()
{
    Ref<Event> loadEvent = Event::create(eventNames().loadEvent, false, false);
    if (m_frame && m_frame->loader().documentLoader() && !m_frame->loader().documentLoader()->timing().loadEventStart()) {
        // The DocumentLoader (and thus its DocumentLoadTiming) might get destroyed while dispatching
        // the event, so protect it to prevent writing the end time into freed memory.
        RefPtr<DocumentLoader> documentLoader = m_frame->loader().documentLoader();
        DocumentLoadTiming& timing = documentLoader->timing();
        timing.markLoadEventStart();
        dispatchEvent(loadEvent, document());
        timing.markLoadEventEnd();
    } else
        dispatchEvent(loadEvent, document());

    // For load events, send a separate load event to the enclosing frame only.
    // This is a DOM extension and is independent of bubbling/capturing rules of
    // the DOM.
    Element* ownerElement = m_frame ? m_frame->ownerElement() : nullptr;
    if (ownerElement)
        ownerElement->dispatchEvent(Event::create(eventNames().loadEvent, false, false));
}

bool DOMWindow::dispatchEvent(Event& event, EventTarget* target)
{
    Ref<EventTarget> protect(*this);

    // Pausing a page may trigger pagehide and pageshow events. WebCore also implicitly fires these
    // events when closing a WebView. Here we keep track of the state of the page to prevent duplicate,
    // unbalanced events per the definition of the pageshow event:
    // <http://www.whatwg.org/specs/web-apps/current-work/multipage/history.html#event-pageshow>.
    if (event.eventInterface() == PageTransitionEventInterfaceType) {
        if (event.type() == eventNames().pageshowEvent) {
            if (m_lastPageStatus == PageStatusShown)
                return true; // Event was previously dispatched; do not fire a duplicate event.
            m_lastPageStatus = PageStatusShown;
        } else if (event.type() == eventNames().pagehideEvent) {
            if (m_lastPageStatus == PageStatusHidden)
                return true; // Event was previously dispatched; do not fire a duplicate event.
            m_lastPageStatus = PageStatusHidden;
        }
    }

    event.setTarget(target ? target : this);
    event.setCurrentTarget(this);
    event.setEventPhase(Event::AT_TARGET);

    bool result = fireEventListeners(event);
    return result;
}

void DOMWindow::removeAllEventListeners()
{
    EventTarget::removeAllEventListeners();

#if ENABLE(IOS_TOUCH_EVENTS) || ENABLE(IOS_GESTURE_EVENTS)
    m_touchEventListenerCount = 0;
#endif

#if ENABLE(TOUCH_EVENTS)
    if (Document* document = this->document())
        document->didRemoveEventTargetNode(*document);
#endif

    removeAllUnloadEventListeners(this);
    removeAllBeforeUnloadEventListeners(this);
}

void DOMWindow::captureEvents()
{
    // Not implemented.
}

void DOMWindow::releaseEvents()
{
    // Not implemented.
}

void DOMWindow::finishedLoading()
{
    if (m_shouldPrintWhenFinishedLoading) {
        m_shouldPrintWhenFinishedLoading = false;
        if (m_frame->loader().activeDocumentLoader()->mainDocumentError().isNull())
            print();
    }
}

void DOMWindow::printErrorMessage(const String& message)
{
    if (message.isEmpty())
        return;

    if (PageConsoleClient* pageConsole = console())
        pageConsole->addMessage(MessageSource::JS, MessageLevel::Error, message);
}

String DOMWindow::crossDomainAccessErrorMessage(const DOMWindow& activeWindow)
{
    const URL& activeWindowURL = activeWindow.document()->url();
    if (activeWindowURL.isNull())
        return String();

    ASSERT(!activeWindow.document()->securityOrigin()->canAccess(document()->securityOrigin()));

    // FIXME: This message, and other console messages, have extra newlines. Should remove them.
    SecurityOrigin* activeOrigin = activeWindow.document()->securityOrigin();
    SecurityOrigin* targetOrigin = document()->securityOrigin();
    String message = "Blocked a frame with origin \"" + activeOrigin->toString() + "\" from accessing a frame with origin \"" + targetOrigin->toString() + "\". ";

    // Sandbox errors: Use the origin of the frames' location, rather than their actual origin (since we know that at least one will be "null").
    URL activeURL = activeWindow.document()->url();
    URL targetURL = document()->url();
    if (document()->isSandboxed(SandboxOrigin) || activeWindow.document()->isSandboxed(SandboxOrigin)) {
        message = "Blocked a frame at \"" + SecurityOrigin::create(activeURL).get().toString() + "\" from accessing a frame at \"" + SecurityOrigin::create(targetURL).get().toString() + "\". ";
        if (document()->isSandboxed(SandboxOrigin) && activeWindow.document()->isSandboxed(SandboxOrigin))
            return "Sandbox access violation: " + message + " Both frames are sandboxed and lack the \"allow-same-origin\" flag.";
        if (document()->isSandboxed(SandboxOrigin))
            return "Sandbox access violation: " + message + " The frame being accessed is sandboxed and lacks the \"allow-same-origin\" flag.";
        return "Sandbox access violation: " + message + " The frame requesting access is sandboxed and lacks the \"allow-same-origin\" flag.";
    }

    // Protocol errors: Use the URL's protocol rather than the origin's protocol so that we get a useful message for non-heirarchal URLs like 'data:'.
    if (targetOrigin->protocol() != activeOrigin->protocol())
        return message + " The frame requesting access has a protocol of \"" + activeURL.protocol() + "\", the frame being accessed has a protocol of \"" + targetURL.protocol() + "\". Protocols must match.\n";

    // 'document.domain' errors.
    if (targetOrigin->domainWasSetInDOM() && activeOrigin->domainWasSetInDOM())
        return message + "The frame requesting access set \"document.domain\" to \"" + activeOrigin->domain() + "\", the frame being accessed set it to \"" + targetOrigin->domain() + "\". Both must set \"document.domain\" to the same value to allow access.";
    if (activeOrigin->domainWasSetInDOM())
        return message + "The frame requesting access set \"document.domain\" to \"" + activeOrigin->domain() + "\", but the frame being accessed did not. Both must set \"document.domain\" to the same value to allow access.";
    if (targetOrigin->domainWasSetInDOM())
        return message + "The frame being accessed set \"document.domain\" to \"" + targetOrigin->domain() + "\", but the frame requesting access did not. Both must set \"document.domain\" to the same value to allow access.";

    // Default.
    return message + "Protocols, domains, and ports must match.";
}

bool DOMWindow::isInsecureScriptAccess(DOMWindow& activeWindow, const String& urlString)
{
    if (!protocolIsJavaScript(urlString))
        return false;

    // If this DOMWindow isn't currently active in the Frame, then there's no
    // way we should allow the access.
    // FIXME: Remove this check if we're able to disconnect DOMWindow from
    // Frame on navigation: https://bugs.webkit.org/show_bug.cgi?id=62054
    if (isCurrentlyDisplayedInFrame()) {
        // FIXME: Is there some way to eliminate the need for a separate "activeWindow == this" check?
        if (&activeWindow == this)
            return false;

        // FIXME: The name canAccess seems to be a roundabout way to ask "can execute script".
        // Can we name the SecurityOrigin function better to make this more clear?
        if (activeWindow.document()->securityOrigin()->canAccess(document()->securityOrigin()))
            return false;
    }

    printErrorMessage(crossDomainAccessErrorMessage(activeWindow));
    return true;
}

RefPtr<Frame> DOMWindow::createWindow(const String& urlString, const AtomicString& frameName, const WindowFeatures& windowFeatures, DOMWindow& activeWindow, Frame& firstFrame, Frame& openerFrame, std::function<void (DOMWindow&)> prepareDialogFunction)
{
    Frame* activeFrame = activeWindow.frame();
    if (!activeFrame)
        return nullptr;

    Document* activeDocument = activeWindow.document();
    if (!activeDocument)
        return nullptr;

    URL completedURL = urlString.isEmpty() ? URL(ParsedURLString, emptyString()) : firstFrame.document()->completeURL(urlString);
    if (!completedURL.isEmpty() && !completedURL.isValid()) {
        // Don't expose client code to invalid URLs.
        activeWindow.printErrorMessage("Unable to open a window with invalid URL '" + completedURL.string() + "'.\n");
        return nullptr;
    }

    // For whatever reason, Firefox uses the first frame to determine the outgoingReferrer. We replicate that behavior here.
    String referrer = SecurityPolicy::generateReferrerHeader(firstFrame.document()->referrerPolicy(), completedURL, firstFrame.loader().outgoingReferrer());

    ResourceRequest request(completedURL, referrer);
    FrameLoader::addHTTPOriginIfNeeded(request, firstFrame.loader().outgoingOrigin());
    FrameLoadRequest frameRequest(activeDocument->securityOrigin(), request, frameName, LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, ReplaceDocumentIfJavaScriptURL, activeDocument->shouldOpenExternalURLsPolicyToPropagate());

    // We pass the opener frame for the lookupFrame in case the active frame is different from
    // the opener frame, and the name references a frame relative to the opener frame.
    bool created;
    RefPtr<Frame> newFrame = WebCore::createWindow(*activeFrame, openerFrame, frameRequest, windowFeatures, created);
    if (!newFrame)
        return nullptr;

    newFrame->loader().setOpener(&openerFrame);
    newFrame->page()->setOpenedByDOM();

    if (newFrame->document()->domWindow()->isInsecureScriptAccess(activeWindow, completedURL))
        return newFrame;

    if (prepareDialogFunction)
        prepareDialogFunction(*newFrame->document()->domWindow());

    if (created) {
        ResourceRequest resourceRequest(completedURL, referrer, UseProtocolCachePolicy);
        FrameLoadRequest frameRequest(activeWindow.document()->securityOrigin(), resourceRequest, "_self", LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, AllowNavigationToInvalidURL::Yes, NewFrameOpenerPolicy::Allow, activeDocument->shouldOpenExternalURLsPolicyToPropagate());
        newFrame->loader().changeLocation(frameRequest);
    } else if (!urlString.isEmpty()) {
        LockHistory lockHistory = UserGestureIndicator::processingUserGesture() ? LockHistory::No : LockHistory::Yes;
        newFrame->navigationScheduler().scheduleLocationChange(activeWindow.document(), activeWindow.document()->securityOrigin(), completedURL, referrer, lockHistory, LockBackForwardList::No);
    }

    // Navigating the new frame could result in it being detached from its page by a navigation policy delegate.
    if (!newFrame->page())
        return nullptr;

    return newFrame;
}

PassRefPtr<DOMWindow> DOMWindow::open(const String& urlString, const AtomicString& frameName, const String& windowFeaturesString,
    DOMWindow& activeWindow, DOMWindow& firstWindow)
{
    if (!isCurrentlyDisplayedInFrame())
        return nullptr;

    Document* activeDocument = activeWindow.document();
    if (!activeDocument)
        return nullptr;

    Frame* firstFrame = firstWindow.frame();
    if (!firstFrame)
        return nullptr;

#if ENABLE(CONTENT_EXTENSIONS)
    if (firstFrame->document()
        && firstFrame->mainFrame().page()
        && firstFrame->mainFrame().page()->userContentController()
        && firstFrame->mainFrame().document()
        && firstFrame->mainFrame().document()->loader()) {
        ResourceLoadInfo resourceLoadInfo = {firstFrame->document()->completeURL(urlString), firstFrame->mainFrame().document()->url(), ResourceType::Popup};
        Vector<ContentExtensions::Action> actions = firstFrame->mainFrame().page()->userContentController()->actionsForResourceLoad(resourceLoadInfo, *firstFrame->mainFrame().document()->loader());
        for (const ContentExtensions::Action& action : actions) {
            if (action.type() == ContentExtensions::ActionType::BlockLoad)
                return nullptr;
        }
    }
#endif

    if (!firstWindow.allowPopUp()) {
        // Because FrameTree::findFrameForNavigation() returns true for empty strings, we must check for empty frame names.
        // Otherwise, illegitimate window.open() calls with no name will pass right through the popup blocker.
        if (frameName.isEmpty() || !m_frame->loader().findFrameForNavigation(frameName, activeDocument))
            return nullptr;
    }

    // Get the target frame for the special cases of _top and _parent.
    // In those cases, we schedule a location change right now and return early.
    Frame* targetFrame = nullptr;
    if (frameName == "_top")
        targetFrame = &m_frame->tree().top();
    else if (frameName == "_parent") {
        if (Frame* parent = m_frame->tree().parent())
            targetFrame = parent;
        else
            targetFrame = m_frame;
    }
    if (targetFrame) {
        if (!activeDocument->canNavigate(targetFrame))
            return nullptr;

        URL completedURL = firstFrame->document()->completeURL(urlString);

        if (targetFrame->document()->domWindow()->isInsecureScriptAccess(activeWindow, completedURL))
            return targetFrame->document()->domWindow();

        if (urlString.isEmpty())
            return targetFrame->document()->domWindow();

        // For whatever reason, Firefox uses the first window rather than the active window to
        // determine the outgoing referrer. We replicate that behavior here.
        LockHistory lockHistory = UserGestureIndicator::processingUserGesture() ? LockHistory::No : LockHistory::Yes;
        targetFrame->navigationScheduler().scheduleLocationChange(activeDocument, activeDocument->securityOrigin(), completedURL, firstFrame->loader().outgoingReferrer(),
            lockHistory, LockBackForwardList::No);
        return targetFrame->document()->domWindow();
    }

    RefPtr<Frame> result = createWindow(urlString, frameName, parseWindowFeatures(windowFeaturesString), activeWindow, *firstFrame, *m_frame);
    return result ? result->document()->domWindow() : nullptr;
}

void DOMWindow::showModalDialog(const String& urlString, const String& dialogFeaturesString, DOMWindow& activeWindow, DOMWindow& firstWindow, std::function<void (DOMWindow&)> prepareDialogFunction)
{
    if (!isCurrentlyDisplayedInFrame())
        return;
    Frame* activeFrame = activeWindow.frame();
    if (!activeFrame)
        return;
    Frame* firstFrame = firstWindow.frame();
    if (!firstFrame)
        return;

    auto* page = m_frame->page();
    if (!page)
        return;

    if (!page->arePromptsAllowed()) {
        printErrorMessage("Use of window.showModalDialog is not allowed while unloading a page.");
        return;
    }

    if (!canShowModalDialog(m_frame) || !firstWindow.allowPopUp())
        return;

    RefPtr<Frame> dialogFrame = createWindow(urlString, emptyAtom, parseDialogFeatures(dialogFeaturesString, screenAvailableRect(m_frame->view())), activeWindow, *firstFrame, *m_frame, WTFMove(prepareDialogFunction));
    if (!dialogFrame)
        return;
    dialogFrame->page()->chrome().runModal();
}

void DOMWindow::enableSuddenTermination()
{
    if (Page* page = this->page())
        page->chrome().enableSuddenTermination();
}

void DOMWindow::disableSuddenTermination()
{
    if (Page* page = this->page())
        page->chrome().disableSuddenTermination();
}

} // namespace WebCore
