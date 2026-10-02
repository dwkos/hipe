/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004-2016 Apple Inc. All rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Google Inc.
 * Copyright (C) 2025-2026 General Development Systems
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

#include "config.h"
#include "Frame.h"

#include "AnimationController.h"
#include "ApplyStyleCommand.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "CachedCSSStyleSheet.h"
#include "CachedResourceLoader.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMWindow.h"
#include "DocumentType.h"
#include "Editor.h"
#include "EditorClient.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FloatQuad.h"
#include "FocusController.h"
#include "FrameDestructionObserver.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "HTMLDocument.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include "MainFrame.h"
#include "MathMLNames.h"
#include "MediaFeatureNames.h"
#include "NodeList.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "PageGroup.h"
#include "RenderLayerCompositor.h"
#include "RenderTableCell.h"
#include "RenderText.h"
#include "RenderTextControl.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGDocument.h"
#include "SVGDocumentExtensions.h"
#include "SVGNames.h"
#include "ScrollingCoordinator.h"
#include "Settings.h"
#include "StyleProperties.h"
#include "TextResourceDecoder.h"
#include "UserContentController.h"
#include "UserContentURLPattern.h"
#include "UserTypingGestureIndicator.h"
#include "VisibleUnits.h"
#include "WebKitFontFamilyNames.h"
#include "XLinkNames.h"
#include "XMLNSNames.h"
#include "XMLNames.h"
#include "htmlediting.h"
#include "markup.h"
#include <wtf/RefCountedLeakCounter.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, frameCounter, ("Frame"));

static inline Frame* parentFromOwnerElement(HTMLFrameOwnerElement* ownerElement)
{
    if (!ownerElement)
        return 0;
    return ownerElement->document().frame();
}

static inline float parentPageZoomFactor(Frame* frame)
{
    Frame* parent = frame->tree().parent();
    if (!parent)
        return 1;
    return parent->pageZoomFactor();
}

static inline float parentTextZoomFactor(Frame* frame)
{
    Frame* parent = frame->tree().parent();
    if (!parent)
        return 1;
    return parent->textZoomFactor();
}

Frame::Frame(Page& page, HTMLFrameOwnerElement* ownerElement, FrameLoaderClient& frameLoaderClient)
    : m_mainFrame(ownerElement ? page.mainFrame() : static_cast<MainFrame&>(*this))
    , m_page(&page)
    , m_settings(&page.settings())
    , m_treeNode(*this, parentFromOwnerElement(ownerElement))
    , m_loader(*this, frameLoaderClient)
    , m_navigationScheduler(*this)
    , m_ownerElement(ownerElement)
    , m_editor(std::make_unique<Editor>(*this))
    , m_selection(std::make_unique<FrameSelection>(this))
    , m_animationController(std::make_unique<AnimationController>(*this))
    , m_pageZoomFactor(parentPageZoomFactor(this))
    , m_textZoomFactor(parentTextZoomFactor(this))
    , m_activeDOMObjectsAndAnimationsSuspendedCount(0)
    , m_eventHandler(std::make_unique<EventHandler>(*this))
{
    AtomicString::init();
    HTMLNames::init();
    QualifiedName::init();
    MediaFeatureNames::init();
    SVGNames::init();
    XLinkNames::init();
    MathMLNames::init();
    XMLNSNames::init();
    XMLNames::init();
    WebKitFontFamilyNames::init();

    if (ownerElement) {
        m_mainFrame.selfOnlyRef();
        page.incrementSubframeCount();
        ownerElement->setContentFrame(this);
    }

#ifndef NDEBUG
    frameCounter.increment();
#endif

    // Pause future ActiveDOMObjects if this frame is being created while the page is in a paused state.
    Frame* parent = parentFromOwnerElement(ownerElement);
    if (parent && parent->activeDOMObjectsAndAnimationsSuspended())
        suspendActiveDOMObjectsAndAnimations();
}

Ref<Frame> Frame::create(Page* page, HTMLFrameOwnerElement* ownerElement, FrameLoaderClient* client)
{
    ASSERT(page);
    ASSERT(client);
    return adoptRef(*new Frame(*page, ownerElement, *client));
}

Frame::~Frame()
{
    setView(nullptr);
    loader().cancelAndClear();

    // FIXME: We should not be doing all this work inside the destructor

#ifndef NDEBUG
    frameCounter.decrement();
#endif

    disconnectOwnerElement();

    while (auto* destructionObserver = m_destructionObservers.takeAny())
        destructionObserver->frameDestroyed();

    if (!isMainFrame())
        m_mainFrame.selfOnlyDeref();
}

void Frame::addDestructionObserver(FrameDestructionObserver* observer)
{
    m_destructionObservers.add(observer);
}

void Frame::removeDestructionObserver(FrameDestructionObserver* observer)
{
    m_destructionObservers.remove(observer);
}

void Frame::setView(RefPtr<FrameView>&& view)
{
    // We the custom scroll bars as early as possible to prevent m_doc->detach()
    // from messing with the view such that its scroll bars won't be torn down.
    // FIXME: We should revisit this.
    if (m_view)
        m_view->prepareForDetach();

    // Prepare for destruction now, so any unload event handlers get run and the DOMWindow is
    // notified. If we wait until the view is destroyed, then things won't be hooked up enough for
    // these calls to work.
    if (!view && m_doc && m_doc->pageCacheState() != Document::InPageCache)
        m_doc->prepareForDestruction();
    
    if (m_view)
        m_view->unscheduleRelayout();
    
    // This may be called during destruction, so need to do a null check.
    if (m_eventHandler)
        m_eventHandler->clear();

    m_view = WTFMove(view);

    // Only one form submission is allowed per view of a part.
    // Since this part may be getting reused as a result of being
    // pulled from the back/forward cache, reset this flag.
    loader().resetMultipleFormSubmissionProtection();
}

void Frame::setDocument(RefPtr<Document>&& newDocument)
{
    ASSERT(!newDocument || newDocument->frame() == this);

    if (m_documentIsBeingReplaced)
        return;

    m_documentIsBeingReplaced = true;
    
    if (m_doc && m_doc->pageCacheState() != Document::InPageCache)
        m_doc->prepareForDestruction();

    m_doc = newDocument.copyRef();
    ASSERT(!m_doc || m_doc->domWindow());
    ASSERT(!m_doc || m_doc->domWindow()->frame() == this);

    // Don't use m_doc because it can be overwritten and we want to guarantee
    // that the document is not destroyed during this function call.
    if (newDocument)
        newDocument->didBecomeCurrentDocumentInFrame();

    m_documentIsBeingReplaced = false;
}

#if ENABLE(ORIENTATION_EVENTS)
void Frame::orientationChanged()
{
    Vector<Ref<Frame>> frames;
    for (Frame* frame = this; frame; frame = frame->tree().traverseNext())
        frames.append(*frame);

    for (auto& frame : frames) {
        if (Document* document = frame->document())
            document->dispatchWindowEvent(Event::create(eventNames().orientationchangeEvent, false, false));
    }
}

int Frame::orientation() const
{
    if (m_page)
        return m_page->chrome().client().deviceOrientation();
    return 0;
}
#endif // ENABLE(ORIENTATION_EVENTS)

// hipecore: searchForLabelsBeforeElement / searchForLabelsAboveCell / matchLabelsAgainstElement
// (the form-autofill "guess this field's purpose from nearby label text" heuristic, backed by
// JSC/Yarr regexes) removed - it had no callers and hipecore does no autofill.

// hipecore: setPrinting() / shouldUsePrintingLayout() / resizePageRectsKeepingRatio()
// removed along with PrintContext - hipecore never enters print-media layout. Document
// keeps an always-false printing() flag (nothing sets it), so the residual
// document().printing() checks scattered through rendering simply stay on their
// screen-media path.

RenderView* Frame::contentRenderer() const
{
    return document() ? document()->renderView() : nullptr;
}

RenderWidget* Frame::ownerRenderer() const
{
    HTMLFrameOwnerElement* ownerElement = m_ownerElement;
    if (!ownerElement)
        return nullptr;
    auto* object = ownerElement->renderer();
    if (!object)
        return nullptr;
    // FIXME: If <object> is ever fixed to disassociate itself from frames
    // that it has started but canceled, then this can turn into an ASSERT
    // since m_ownerElement would be 0 when the load is canceled.
    // https://bugs.webkit.org/show_bug.cgi?id=18585
    if (!is<RenderWidget>(*object))
        return nullptr;
    return downcast<RenderWidget>(object);
}

Frame* Frame::frameForWidget(const Widget* widget)
{
    ASSERT_ARG(widget, widget);

    if (RenderWidget* renderer = RenderWidget::find(widget))
        return renderer->frameOwnerElement().document().frame();

    // Assume all widgets are either a FrameView or owned by a RenderWidget.
    // FIXME: That assumption is not right for scroll bars!
    return &downcast<FrameView>(*widget).frame();
}

void Frame::clearTimers(FrameView *view, Document *document)
{
    if (view) {
        view->unscheduleRelayout();
        view->frame().animation().suspendAnimationsForDocument(document);
        view->frame().eventHandler().stopAutoscrollTimer();
    }
}

void Frame::clearTimers()
{
    clearTimers(m_view.get(), document());
}

void Frame::willDetachPage()
{
    if (Frame* parent = tree().parent())
        parent->loader().checkLoadComplete();

    for (auto& observer : m_destructionObservers)
        observer->willDetachPage();

    // FIXME: It's unclear as to why this is called more than once, but it is,
    // so page() could be NULL.
    if (page() && page()->focusController().focusedFrame() == this)
        page()->focusController().setFocusedFrame(nullptr);

    if (page() && page()->scrollingCoordinator() && m_view)
        page()->scrollingCoordinator()->willDestroyScrollableArea(*m_view);
}

void Frame::disconnectOwnerElement()
{
    if (m_ownerElement) {
        m_ownerElement->clearContentFrame();
        if (m_page)
            m_page->decrementSubframeCount();
    }
    m_ownerElement = nullptr;
}

String Frame::displayStringModifiedByEncoding(const String& str) const
{
    return document() ? document()->displayStringModifiedByEncoding(str) : str;
}

VisiblePosition Frame::visiblePositionForPoint(const IntPoint& framePoint) const
{
    HitTestResult result = eventHandler().hitTestResultAtPoint(framePoint, HitTestRequest::ReadOnly | HitTestRequest::Active);
    Node* node = result.innerNonSharedNode();
    if (!node)
        return VisiblePosition();
    auto renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();
    VisiblePosition visiblePos = renderer->positionForPoint(result.localPoint(), nullptr);
    if (visiblePos.isNull())
        visiblePos = firstPositionInOrBeforeNode(node);
    return visiblePos;
}

Document* Frame::documentAtPoint(const IntPoint& point)
{
    if (!view())
        return 0;

    IntPoint pt = view()->windowToContents(point);
    HitTestResult result = HitTestResult(pt);

    if (contentRenderer())
        result = eventHandler().hitTestResultAtPoint(pt);
    return result.innerNode() ? &result.innerNode()->document() : 0;
}

RefPtr<Range> Frame::rangeForPoint(const IntPoint& framePoint)
{
    VisiblePosition position = visiblePositionForPoint(framePoint);
    if (position.isNull())
        return nullptr;

    Position deepPosition = position.deepEquivalent();
    Text* containerText = deepPosition.containerText();
    if (!containerText || !containerText->renderer() || containerText->renderer()->style().userSelect() == SELECT_NONE)
        return nullptr;

    VisiblePosition previous = position.previous();
    if (previous.isNotNull()) {
        RefPtr<Range> previousCharacterRange = makeRange(previous, position);
        LayoutRect rect = editor().firstRectForRange(previousCharacterRange.get());
        if (rect.contains(framePoint))
            return previousCharacterRange;
    }

    VisiblePosition next = position.next();
    if (RefPtr<Range> nextCharacterRange = makeRange(position, next)) {
        LayoutRect rect = editor().firstRectForRange(nextCharacterRange.get());
        if (rect.contains(framePoint))
            return nextCharacterRange;
    }

    return nullptr;
}

void Frame::createView(const IntSize& viewportSize, const Color& backgroundColor, bool transparent,
    const IntSize& fixedLayoutSize, const IntRect& fixedVisibleContentRect,
    bool useFixedLayout, ScrollbarMode horizontalScrollbarMode, bool horizontalLock,
    ScrollbarMode verticalScrollbarMode, bool verticalLock)
{
    ASSERT(m_page);

    bool isMainFrame = this->isMainFrame();

    if (isMainFrame && view())
        view()->setParentVisible(false);

    setView(nullptr);

    RefPtr<FrameView> frameView;
    if (isMainFrame) {
        frameView = FrameView::create(*this, viewportSize);
        frameView->setFixedLayoutSize(fixedLayoutSize);
#if USE(COORDINATED_GRAPHICS)
        frameView->setFixedVisibleContentRect(fixedVisibleContentRect);
#else
        UNUSED_PARAM(fixedVisibleContentRect);
#endif
        frameView->setUseFixedLayout(useFixedLayout);
    } else
        frameView = FrameView::create(*this);

    frameView->setScrollbarModes(horizontalScrollbarMode, verticalScrollbarMode, horizontalLock, verticalLock);

    setView(frameView.copyRef());

    if (backgroundColor.isValid())
        frameView->updateBackgroundRecursively(backgroundColor, transparent);

    if (isMainFrame)
        frameView->setParentVisible(true);

    if (ownerRenderer())
        ownerRenderer()->setWidget(frameView);

    if (HTMLFrameOwnerElement* owner = ownerElement())
        view()->setCanHaveScrollbars(owner->scrollingMode() != ScrollbarAlwaysOff);
}

String Frame::layerTreeAsText(LayerTreeFlags flags) const
{
    document()->updateLayout();

    if (!contentRenderer())
        return String();

    return contentRenderer()->compositor().layerTreeAsText(flags);
}

String Frame::trackedRepaintRectsAsText() const
{
    if (!m_view)
        return String();
    return m_view->trackedRepaintRectsAsText();
}

void Frame::setPageZoomFactor(float factor)
{
    setPageAndTextZoomFactors(factor, m_textZoomFactor);
}

void Frame::setTextZoomFactor(float factor)
{
    setPageAndTextZoomFactors(m_pageZoomFactor, factor);
}

void Frame::setPageAndTextZoomFactors(float pageZoomFactor, float textZoomFactor)
{
    if (m_pageZoomFactor == pageZoomFactor && m_textZoomFactor == textZoomFactor)
        return;

    Page* page = this->page();
    if (!page)
        return;

    Document* document = this->document();
    if (!document)
        return;

    m_editor->dismissCorrectionPanelAsIgnored();

    // Respect SVGs zoomAndPan="disabled" property in standalone SVG documents.
    // FIXME: How to handle compound documents + zoomAndPan="disabled"? Needs SVG WG clarification.
    if (is<SVGDocument>(*document) && !downcast<SVGDocument>(*document).zoomAndPanEnabled())
        return;

    if (m_pageZoomFactor != pageZoomFactor) {
        if (FrameView* view = this->view()) {
            // Update the scroll position when doing a full page zoom, so the content stays in relatively the same position.
            LayoutPoint scrollPosition = view->scrollPosition();
            float percentDifference = (pageZoomFactor / m_pageZoomFactor);
            view->setScrollPosition(IntPoint(scrollPosition.x() * percentDifference, scrollPosition.y() * percentDifference));
        }
    }

    m_pageZoomFactor = pageZoomFactor;
    m_textZoomFactor = textZoomFactor;

    document->recalcStyle(Style::Force);

    for (RefPtr<Frame> child = tree().firstChild(); child; child = child->tree().nextSibling())
        child->setPageAndTextZoomFactors(m_pageZoomFactor, m_textZoomFactor);

    if (FrameView* view = this->view()) {
        if (document->renderView() && document->renderView()->needsLayout() && view->didFirstLayout())
            view->layout();
    }
}

float Frame::frameScaleFactor() const
{
    Page* page = this->page();

    // Main frame is scaled with respect to he container but inner frames are not scaled with respect to the main frame.
    if (!page || &page->mainFrame() != this || settings().delegatesPageScaling())
        return 1;

    return page->pageScaleFactor();
}

void Frame::suspendActiveDOMObjectsAndAnimations()
{
    bool wasSuspended = activeDOMObjectsAndAnimationsSuspended();

    m_activeDOMObjectsAndAnimationsSuspendedCount++;

    if (wasSuspended)
        return;

    // FIXME: Suspend/resume calls will not match if the frame is navigated, and gets a new document.
    clearTimers(); // Suspends animations and pending relayouts.
    if (m_doc)
        m_doc->suspendScheduledTasks(ActiveDOMObject::PageWillBeSuspended);
}

void Frame::resumeActiveDOMObjectsAndAnimations()
{
    if (!activeDOMObjectsAndAnimationsSuspended())
        return;

    m_activeDOMObjectsAndAnimationsSuspendedCount--;

    if (activeDOMObjectsAndAnimationsSuspended())
        return;

    if (!m_doc)
        return;

    // FIXME: Suspend/resume calls will not match if the frame is navigated, and gets a new document.
    m_doc->resumeScheduledTasks(ActiveDOMObject::PageWillBeSuspended);

    // Frame::clearTimers() suspended animations and pending relayouts.
    animation().resumeAnimationsForDocument(m_doc.get());
    if (m_view)
        m_view->scheduleRelayout();
}

void Frame::deviceOrPageScaleFactorChanged()
{
    for (RefPtr<Frame> child = tree().firstChild(); child; child = child->tree().nextSibling())
        child->deviceOrPageScaleFactorChanged();

    if (RenderView* root = contentRenderer())
        root->compositor().deviceOrPageScaleFactorChanged();
}

bool Frame::isURLAllowed(const URL& url) const
{
    // HipeCore doesn't do same-URL self-reference detection: every nested frame Hipe creates
    // shares the same synthetic "about:blank"-derived URL by design (see loadURLIntoChildFrame),
    // which would otherwise trip this check as a false-positive redirect loop. The frame-count
    // cap below is the real safeguard against unbounded frame creation.
    UNUSED_PARAM(url);
    return m_page->subframeCount() < Page::maxNumberOfFrames;
}

} // namespace WebCore
