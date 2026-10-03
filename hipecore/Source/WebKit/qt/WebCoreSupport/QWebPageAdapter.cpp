/*
 * Copyright (C) 2015 The Qt Company Ltd.
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
 *
 */

#include "config.h"
#include "QWebPageAdapter.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSParser.h"
#include "Chrome.h"
#include "ChromeClientQt.h"
#include "ClientRect.h"
#include "ContextMenu.h"
#include "ContextMenuClientQt.h"
#include "ContextMenuController.h"
#include "CompositeEditCommand.h"
#include "DocumentLoader.h"
#include "DragClientQt.h"
#include "DragController.h"
#include "DragData.h"
#include "Editor.h"
#include "EditorClientQt.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "FrameLoadRequest.h"
#include "FrameLoaderClientQt.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLFormElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "HitTestResult.h"
#include "InitWebCoreQt.h"
#include "LocalizedStrings.h"
#include "MIMETypeRegistry.h"
#include "MainFrame.h"
#include "MemoryCache.h"
#include "NetworkingContext.h"
#include "NodeList.h"
#include "PageConfiguration.h"
#include "PageGroup.h"
#include "Pasteboard.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformTouchEvent.h"
#include "PlatformWheelEvent.h"
#include "ProgressTracker.h"
#include "ProgressTrackerClientQt.h"
#include "QWebFrameAdapter.h"
#include "RenderTextControl.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "Settings.h"
#include "TextIterator.h"
#include "TypingCommand.h"
#include "UndoStepQt.h"
#include "UserContentController.h"
#include "UserGestureIndicator.h"
#include "VisitedLinkStoreQt.h"
#include "WebDatabaseProvider.h"
#include "WebEventConversion.h"
#include "WebStorageNamespaceProvider.h"
#include "WindowFeatures.h"
#include "qwebpluginfactory.h"
#include "qwebsettings.h"
#include <Page.h>
#include <QBitArray>
#include <QGuiApplication>
#include <QMimeData>
#include <QMouseEvent>
#include <QStyleHints>
#include <QTextCharFormat>
#include <QTouchEvent>
#include <QWheelEvent>

// from text/qfont.cpp
QT_BEGIN_NAMESPACE
QT_END_NAMESPACE

using namespace WebCore;

bool QWebPageAdapter::drtRun = false;

typedef QWebPageAdapter::MenuItemDescription MenuItem;

static inline DragOperation dropActionToDragOp(Qt::DropActions actions)
{
    unsigned result = 0;
    if (actions & Qt::CopyAction)
        result |= DragOperationCopy;
    // DragOperationgeneric represents InternetExplorer's equivalent of Move operation,
    // hence it should be considered as "move"
    if (actions & Qt::MoveAction)
        result |= (DragOperationMove | DragOperationGeneric);
    if (actions & Qt::LinkAction)
        result |= DragOperationLink;
    if (result == (DragOperationCopy | DragOperationMove | DragOperationGeneric | DragOperationLink))
        result = DragOperationEvery;
    return (DragOperation)result;
}

static inline Qt::DropAction dragOpToDropAction(unsigned actions)
{
    Qt::DropAction result = Qt::IgnoreAction;
    if (actions & DragOperationCopy)
        result = Qt::CopyAction;
    else if (actions & DragOperationMove)
        result = Qt::MoveAction;
    // DragOperationgeneric represents InternetExplorer's equivalent of Move operation,
    // hence it should be considered as "move"
    else if (actions & DragOperationGeneric)
        result = Qt::MoveAction;
    else if (actions & DragOperationLink)
        result = Qt::LinkAction;
    return result;
}

static inline QWebPageAdapter::VisibilityState webCoreVisibilityStateToWebPageVisibilityState(WebCore::PageVisibilityState state)
{
    switch (state) {
    case WebCore::PageVisibilityStatePrerender:
        return QWebPageAdapter::VisibilityStatePrerender;
    case WebCore::PageVisibilityStateVisible:
        return QWebPageAdapter::VisibilityStateVisible;
    case WebCore::PageVisibilityStateHidden:
        return QWebPageAdapter::VisibilityStateHidden;
    default:
        ASSERT(false);
        return QWebPageAdapter::VisibilityStateHidden;
    }
}

// FIXME: Find a better place
static UserContentController& userContentProvider()
{
    static NeverDestroyed<Ref<UserContentController>> s_userContentProvider(UserContentController::create());
    return s_userContentProvider.get();
}

QWebPageAdapter::QWebPageAdapter()
    : settings(0)
    , page(0)
    , pluginFactory(0)
    , clickCausedFocus(false)
    , mousePressed(false)
    , m_useNativeVirtualKeyAsDOMKey(false)
    , m_totalBytes(0)
    , m_bytesReceived()
{
    WebCore::initializeWebCoreQt();
}

void QWebPageAdapter::initializeWebCorePage()
{
    PageConfiguration pageConfiguration;
    pageConfiguration.chromeClient = new ChromeClientQt(this);
    pageConfiguration.contextMenuClient = new ContextMenuClientQt();
    pageConfiguration.editorClient = new EditorClientQt(this);
    pageConfiguration.dragClient = new DragClientQt(pageConfiguration.chromeClient);
    pageConfiguration.loaderClientForMainFrame = new FrameLoaderClientQt();
    pageConfiguration.progressTrackerClient = new ProgressTrackerClientQt(this);
    pageConfiguration.databaseProvider = &WebDatabaseProvider::singleton();
    pageConfiguration.storageNamespaceProvider = WebStorageNamespaceProvider::create();
    pageConfiguration.userContentController = &userContentProvider();
    pageConfiguration.visitedLinkStore = &VisitedLinkStoreQt::singleton();
    page = new Page(pageConfiguration);

    // By default each page is put into their own unique page group, which affects popup windows
    // and visited links. Page groups (per process only) is a feature making it possible to use
    // separate settings for each group, so that for instance an integrated browser/email reader
    // can use different settings for displaying HTML pages and HTML email. To make QtWebKit work
    // as expected out of the box, we use a default group similar to what other ports are doing.
    page->setGroupName("Default Group");

    page->addLayoutMilestones(DidFirstVisuallyNonEmptyLayout);

    // <meta http-equiv> does nothing in a client's document (no refresh, no content security policy...).
    page->settings().setHttpEquivEnabled(false);

    settings = new QWebSettings(page);
}

QWebPageAdapter::~QWebPageAdapter()
{
    delete page;
    delete settings;
}

void QWebPageAdapter::deletePage()
{
    // Before we delete the page, detach the mainframe's loader
    FrameLoader& loader = mainFrameAdapter().frame->loader();
    loader.detachFromParent();
    delete page;
    page = 0;
}

QWebPageAdapter* QWebPageAdapter::kit(Page* page)
{
    return static_cast<ChromeClientQt&>(page->chrome().client()).m_webPage;
}


void QWebPageAdapter::registerUndoStep(WTF::PassRefPtr<WebCore::UndoStep> step, const void* group)
{
    createUndoStep(QSharedPointer<UndoStepQt>(new UndoStepQt(step)), group);
    pruneUndoStacks();
}

WebCore::Frame* QWebPageAdapter::frameForUndoStep(WebCore::UndoStep* step) const
{
    // Every UndoStep WebCore creates is an EditCommandComposition (Editor registers
    // CompositeEditCommand::ensureComposition()), which records the editable root it acted on.
    EditCommandComposition* composition = static_cast<EditCommandComposition*>(step);
    Element* root = composition->endingRootEditableElement();
    if (!root)
        root = composition->startingRootEditableElement();
    if (root)
        return root->document().frame();
    return &page->focusController().focusedOrMainFrame();
}

const void* QWebPageAdapter::focusedUndoGroup() const
{
    return page->focusController().focusedOrMainFrame().document();
}

void QWebPageAdapter::noteFocusedFrame(WebCore::Frame* frame)
{
    if (!frame)
        return;
    // Forget frames that no longer exist, then record frame for itself and its ancestors.
    QSet<const void*> liveFrames;
    for (Frame* f = &page->mainFrame(); f; f = f->tree().traverseNext())
        liveFrames.insert(f);
    for (auto it = lastFocusedFrames.begin(); it != lastFocusedFrames.end(); ) {
        if (liveFrames.contains(it.key()))
            ++it;
        else
            it = lastFocusedFrames.erase(it);
    }
    for (Frame* f = frame; f; f = f->tree().parent())
        lastFocusedFrames[f] = frame;
}

WebCore::Frame* QWebPageAdapter::lastFocusedFrameWithin(WebCore::Frame* frame) const
{
    if (!frame)
        return nullptr;
    const void* last = lastFocusedFrames.value(frame);
    if (!last)
        return nullptr;
    // Only return a frame that still exists inside frame's subtree (the recorded one may have gone).
    for (Frame* f = frame; f; f = f->tree().traverseNext(frame)) {
        if (f == last)
            return f;
    }
    return nullptr;
}

void QWebPageAdapter::pruneUndoStacks()
{
    QSet<const void*> liveGroups;
    for (Frame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext())
        liveGroups.insert(frame->document());
    removeUndoStacksExcept(liveGroups);
}

void QWebPageAdapter::setVisibilityState(VisibilityState state)
{
    if (!page)
        return;
    page->setIsVisible(state == VisibilityStateVisible);
    if (state == VisibilityStatePrerender)
        page->setIsPrerender();
}

QWebPageAdapter::VisibilityState QWebPageAdapter::visibilityState() const
{
    return webCoreVisibilityStateToWebPageVisibilityState(page->visibilityState());
}

void QWebPageAdapter::setPluginsVisible(bool visible)
{
    if (!page)
        return;
    page->pluginVisibilityChanged(visible);
}

bool QWebPageAdapter::hasSelection() const
{
    Frame& frame = page->focusController().focusedOrMainFrame();
    return frame.selection().selection().selectionType() != VisibleSelection::NoSelection;
}

QString QWebPageAdapter::selectedText() const
{
    Frame& frame = page->focusController().focusedOrMainFrame();
    if (frame.selection().selection().selectionType() == VisibleSelection::NoSelection)
        return QString();
    return frame.editor().selectedText();
}

QString QWebPageAdapter::selectedHtml() const
{
    RefPtr<Range> range = page->focusController().focusedOrMainFrame().editor().selectedRange();
    if (!range)
        return QString();
    return range->toHTML();
}

bool QWebPageAdapter::isContentEditable() const
{
    return page->isEditable();
}

void QWebPageAdapter::setContentEditable(bool editable)
{
    page->setEditable(editable);
    page->setTabKeyCyclesThroughElements(!editable);

    Frame* frame = mainFrameAdapter().frame;
    if (editable) {
        frame->editor().applyEditingStyleToBodyElement();
        // FIXME: mac port calls this if there is no selectedDOMRange
        // frame->setSelectionFromNone();
    }

}

bool QWebPageAdapter::findText(const QString& subString, FindFlag options)
{
    ::WebCore::FindOptions webCoreFindOptions = 0;

    if (!(options & FindCaseSensitively))
        webCoreFindOptions |= WebCore::CaseInsensitive;

    if (options & FindBackward)
        webCoreFindOptions |= WebCore::Backwards;

    if (options & FindWrapsAroundDocument)
        webCoreFindOptions |= WebCore::WrapAround;

    if (options & FindAtWordBeginningsOnly)
        webCoreFindOptions |= WebCore::AtWordStarts;

    if (options & TreatMedialCapitalAsWordBeginning)
        webCoreFindOptions |= WebCore::TreatMedialCapitalAsWordStart;

    if (options & FindBeginsInSelection)
        webCoreFindOptions |= WebCore::StartInSelection;

    if (options & FindAtWordEndingsOnly)
        webCoreFindOptions |= WebCore::AtWordEnds;

    if (options & HighlightAllOccurrences) {
        if (subString.isEmpty()) {
            page->unmarkAllTextMatches();
            return true;
        }
        return page->markAllMatchesForText(subString, webCoreFindOptions, /*shouldHighlight*/ true, /*limit*/ 0);
    }

    if (subString.isEmpty()) {
        page->mainFrame().selection().clear();
        Frame* frame = page->mainFrame().tree().firstChild();
        while (frame) {
            frame->selection().clear();
            frame = frame->tree().traverseNextWithWrap(false);
        }
    }

    return page->findString(subString, webCoreFindOptions);
}

void QWebPageAdapter::adjustPointForClicking(QMouseEvent* ev)
{
#if ENABLE(TOUCH_ADJUSTMENT)
    QtPlatformPlugin platformPlugin;
    std::unique_ptr<QWebTouchModifier> touchModifier = platformPlugin.createTouchModifier();
    if (!touchModifier)
        return;

    unsigned topPadding = touchModifier->hitTestPaddingForTouch(QWebTouchModifier::Up);
    unsigned rightPadding = touchModifier->hitTestPaddingForTouch(QWebTouchModifier::Right);
    unsigned bottomPadding = touchModifier->hitTestPaddingForTouch(QWebTouchModifier::Down);
    unsigned leftPadding = touchModifier->hitTestPaddingForTouch(QWebTouchModifier::Left);

    touchModifier = nullptr;

    if (!topPadding && !rightPadding && !bottomPadding && !leftPadding)
        return;

    FrameView* view = page->mainFrame().view();
    ASSERT(view);
    if (view->scrollbarAtPoint(ev->pos()))
        return;

    IntRect touchRect(ev->pos().x() - leftPadding, ev->pos().y() - topPadding, leftPadding + rightPadding, topPadding + bottomPadding);
    IntPoint adjustedPoint;
    Node* adjustedNode;
    bool foundClickableNode = page->mainFrame().eventHandler().bestClickableNodeForTouchPoint(touchRect.center(), touchRect.size(), adjustedPoint, adjustedNode);
    if (!foundClickableNode)
        return;

    *ev = QMouseEvent(ev->type(), QPoint(adjustedPoint), ev->globalPos(), ev->button(), ev->buttons(), ev->modifiers());
#else
    Q_UNUSED(ev);
#endif
}

void QWebPageAdapter::mouseMoveEvent(QMouseEvent* ev)
{
    WebCore::Frame* frame = mainFrameAdapter().frame;
    if (!frame->view())
        return;
    if (ev->buttons() == Qt::NoButton)
        mousePressed = false;

    bool accepted = frame->eventHandler().mouseMoved(convertMouseEvent(ev, 0));
    ev->setAccepted(accepted);
}

void QWebPageAdapter::mousePressEvent(QMouseEvent* ev)
{
    WebCore::Frame* frame = mainFrameAdapter().frame;
    if (!frame->view())
        return;

    RefPtr<WebCore::Node> oldNode;
    Frame* focusedFrame = page->focusController().focusedFrame();
    if (Document* focusedDocument = focusedFrame ? focusedFrame->document() : 0)
        oldNode = focusedDocument->focusedElement();

    if (tripleClickTimer.isActive()
        && (ev->pos() - tripleClick).manhattanLength() < qGuiApp->styleHints()->startDragDistance()) {
        mouseTripleClickEvent(ev);
        return;
    }

    bool accepted = false;
    PlatformMouseEvent mev = convertMouseEvent(ev, 1);
    // ignore the event if we can't map Qt's mouse buttons to WebCore::MouseButton
    if (mev.button() != NoButton)
        mousePressed = accepted = frame->eventHandler().handleMousePressEvent(mev);
    ev->setAccepted(accepted);

    RefPtr<WebCore::Node> newNode;
    focusedFrame = page->focusController().focusedFrame();
    if (Document* focusedDocument = focusedFrame ? focusedFrame->document() : 0)
        newNode = focusedDocument->focusedElement();

    if (newNode && oldNode != newNode)
        clickCausedFocus = true;
}

void QWebPageAdapter::mouseDoubleClickEvent(QMouseEvent *ev)
{
    WebCore::Frame* frame = mainFrameAdapter().frame;
    if (!frame->view())
        return;

    bool accepted = false;
    PlatformMouseEvent mev = convertMouseEvent(ev, 2);
    // ignore the event if we can't map Qt's mouse buttons to WebCore::MouseButton
    if (mev.button() != NoButton)
        accepted = frame->eventHandler().handleMousePressEvent(mev);
    ev->setAccepted(accepted);

    tripleClickTimer.start(qGuiApp->styleHints()->mouseDoubleClickInterval(), handle());
    tripleClick = QPointF(ev->pos()).toPoint();
}

void QWebPageAdapter::mouseTripleClickEvent(QMouseEvent *ev)
{
    WebCore::Frame* frame = mainFrameAdapter().frame;
    if (!frame->view())
        return;

    bool accepted = false;
    PlatformMouseEvent mev = convertMouseEvent(ev, 3);
    // ignore the event if we can't map Qt's mouse buttons to WebCore::MouseButton
    if (mev.button() != NoButton)
        accepted = frame->eventHandler().handleMousePressEvent(mev);
    ev->setAccepted(accepted);
}

void QWebPageAdapter::mouseReleaseEvent(QMouseEvent *ev)
{
    WebCore::Frame* frame = mainFrameAdapter().frame;
    if (!frame->view())
        return;

    bool accepted = false;
    PlatformMouseEvent mev = convertMouseEvent(ev, 0);
    // ignore the event if we can't map Qt's mouse buttons to WebCore::MouseButton
    if (mev.button() != NoButton)
        accepted = frame->eventHandler().handleMouseReleaseEvent(mev);
    ev->setAccepted(accepted);

    if (ev->buttons() == Qt::NoButton)
        mousePressed = false;

    handleSoftwareInputPanel(ev->button(), QPointF(ev->pos()).toPoint());
}

void QWebPageAdapter::handleSoftwareInputPanel(Qt::MouseButton button, const QPoint& pos)
{
    Frame* frame = page->focusController().focusedFrame();
    if (!frame)
        return;

    if (client && client->inputMethodEnabled()
        && frame->document()->focusedElement()
            && button == Qt::LeftButton && qGuiApp->property("autoSipEnabled").toBool()) {
        if (!clickCausedFocus || requestSoftwareInputPanel()) {
            HitTestResult result = frame->eventHandler().hitTestResultAtPoint(frame->view()->windowToContents(pos));
            if (result.isContentEditable()) {
                QEvent event(QEvent::RequestSoftwareInputPanel);
                QGuiApplication::sendEvent(client->ownerWidget(), &event);
            }
        }
    }

    clickCausedFocus = false;
}

#ifndef QT_NO_WHEELEVENT
void QWebPageAdapter::wheelEvent(QEvent* ev, const QPoint& position, const QPoint& globalPosition, const QPoint& angleDelta, Qt::KeyboardModifiers modifiers, int wheelScrollLines)
{
    WebCore::Frame* frame = mainFrameAdapter().frame;
    if (!frame->view())
        return;

    PlatformWheelEvent pev = convertWheelEvent(position, globalPosition, angleDelta, modifiers, wheelScrollLines);
    bool accepted = frame->eventHandler().handleWheelEvent(pev);
    ev->setAccepted(accepted);
}
#endif // QT_NO_WHEELEVENT

#if ENABLE(DRAG_SUPPORT)

Qt::DropAction QWebPageAdapter::dragEntered(const QMimeData *data, const QPoint &pos, Qt::DropActions possibleActions)
{
    DragData dragData(data, pos, QCursor::pos(), dropActionToDragOp(possibleActions));
    return dragOpToDropAction(page->dragController().dragEntered(dragData));
}

void QWebPageAdapter::dragLeaveEvent()
{
    DragData dragData(0, IntPoint(), QCursor::pos(), DragOperationNone);
    page->dragController().dragExited(dragData);
}

Qt::DropAction QWebPageAdapter::dragUpdated(const QMimeData *data, const QPoint &pos, Qt::DropActions possibleActions)
{
    DragData dragData(data, pos, QCursor::pos(), dropActionToDragOp(possibleActions));
    return dragOpToDropAction(page->dragController().dragUpdated(dragData));
}

bool QWebPageAdapter::performDrag(const QMimeData *data, const QPoint &pos, Qt::DropActions possibleActions)
{
    DragData dragData(data, pos, QCursor::pos(), dropActionToDragOp(possibleActions));
    return page->dragController().performDragOperation(dragData);
}

#endif // ENABLE(DRAG_SUPPORT)

// Inserts text at the focused frame's selection as typing would: it replaces the selected text, joins
// the open typing step of the undo history, and does nothing outside editable content. Newlines
// become line breaks (see TypingCommand::insertTextWithLineBreaks()). Empty text deletes the selected
// text, and does nothing if there is only a caret.
void QWebPageAdapter::insertText(const QString& text)
{
    WebCore::Frame& frame = page->focusController().focusedOrMainFrame();
    WebCore::Editor& editor = frame.editor();
    if (!editor.canEdit() || !frame.document())
        return;

    if (editor.hasComposition())
        editor.confirmComposition();

    if (text.isEmpty())
        TypingCommand::deleteSelection(*frame.document());
    else {
        String string = text;
        string.replace("\r\n", "\n");
        string.replace('\r', '\n');
        TypingCommand::insertText(*frame.document(), string, TypingCommand::InsertNewlinesAsLineBreaks);
    }

    page->focusController().focusedOrMainFrame().selection().revealSelection(ScrollAlignment::alignCenterIfNeeded);
}

void QWebPageAdapter::inputMethodEvent(QInputMethodEvent *ev)
{
    WebCore::Frame& frame = page->focusController().focusedOrMainFrame();
    WebCore::Editor& editor = frame.editor();

    if (!editor.canEdit()) {
        ev->ignore();
        return;
    }

    Node* node = 0;
    if (frame.selection().selection().rootEditableElement())
        node = frame.selection().selection().rootEditableElement()->deprecatedShadowAncestorNode();

    Vector<CompositionUnderline> underlines;
    bool hasSelection = false;

    for (int i = 0; i < ev->attributes().size(); ++i) {
        const QInputMethodEvent::Attribute& a = ev->attributes().at(i);
        switch (a.type) {
        case QInputMethodEvent::TextFormat: {
            QTextCharFormat textCharFormat = a.value.value<QTextFormat>().toCharFormat();
            QColor qcolor = textCharFormat.underlineColor();
            underlines.append(CompositionUnderline(qMin(a.start, (a.start + a.length)), qMax(a.start, (a.start + a.length)), Color(makeRGBA(qcolor.red(), qcolor.green(), qcolor.blue(), qcolor.alpha())), false));
            break;
        }
        case QInputMethodEvent::Cursor: {
            frame.selection().setCaretVisible(a.length); // if length is 0 cursor is invisible
            if (a.length > 0) {
                RenderObject* caretRenderer = frame.selection().caretRendererWithoutUpdatingLayout();
                if (caretRenderer) {
                    QColor qcolor = a.value.value<QColor>();
                    caretRenderer->style().setColor(Color(makeRGBA(qcolor.red(), qcolor.green(), qcolor.blue(), qcolor.alpha())));
                }
            }
            break;
        }
        case QInputMethodEvent::Selection: {
            hasSelection = true;
            // A selection in the inputMethodEvent is always reflected in the visible text
            if (node) {
                if (is<HTMLTextFormControlElement>(node))
                    downcast<HTMLTextFormControlElement>(node)->setSelectionRange(qMin(a.start, (a.start + a.length)), qMax(a.start, (a.start + a.length)));
            }

            if (!ev->preeditString().isEmpty())
                editor.setComposition(ev->preeditString(), underlines, qMin(a.start, (a.start + a.length)), qMax(a.start, (a.start + a.length)));
            else {
                // If we are in the middle of a composition, an empty pre-edit string and a selection of zero
                // cancels the current composition
                if (editor.hasComposition() && !(a.start + a.length))
                    editor.setComposition(QString(), underlines, 0, 0);
            }
            break;
        }
        default:
            break;
        }
    }

    if (node && ev->replacementLength() > 0) {
        int cursorPos = frame.selection().selection().extent().offsetInContainerNode();
        int start = cursorPos + ev->replacementStart();
        if (is<HTMLTextFormControlElement>(node))
            downcast<HTMLTextFormControlElement>(node)->setSelectionRange(start, start + ev->replacementLength());
        // Commit regardless of whether commitString is empty, to get rid of selection.
        editor.confirmComposition(ev->commitString());
    } else if (!ev->commitString().isEmpty()) {
        if (editor.hasComposition())
            editor.confirmComposition(ev->commitString());
        else
            editor.insertText(ev->commitString(), 0);
    } else if (!hasSelection && !ev->preeditString().isEmpty())
        editor.setComposition(ev->preeditString(), underlines, 0, 0);
    else if (ev->preeditString().isEmpty() && editor.hasComposition())
        editor.confirmComposition(String());

    ev->accept();
}

QVariant QWebPageAdapter::inputMethodQuery(Qt::InputMethodQuery property) const
{
    Frame* frame = page->focusController().focusedFrame();
    if (!frame)
        return QVariant();

    WebCore::Editor& editor = frame->editor();

    RenderObject* renderer = 0;
    RenderTextControl* renderTextControl = 0;

    if (frame->selection().selection().rootEditableElement())
        renderer = frame->selection().selection().rootEditableElement()->deprecatedShadowAncestorNode()->renderer();

    if (renderer && renderer->isTextControl())
        renderTextControl = downcast<RenderTextControl>(renderer);

    switch (property) {
    case Qt::ImCursorRectangle: {
        WebCore::FrameView* view = frame->view();
        if (view && view->needsLayout()) {
            // We can't access absoluteCaretBounds() while the view needs to layout.
            return QVariant();
        }
        return QVariant(view->contentsToWindow(frame->selection().absoluteCaretBounds()));
    }
    case Qt::ImFont: {
        if (renderTextControl) {
            RenderStyle& renderStyle = renderTextControl->style();
            return QVariant(QFont(renderStyle.fontCascade().syntheticFont()));
        }
        return QVariant(QFont());
    }
    case Qt::ImCursorPosition: {
        if (editor.hasComposition())
            return QVariant(frame->selection().selection().end().offsetInContainerNode());
        return QVariant(frame->selection().selection().extent().offsetInContainerNode());
    }
    case Qt::ImSurroundingText: {
        if (renderTextControl) {
            QString text = renderTextControl->textFormControlElement().value();
            RefPtr<Range> range = editor.compositionRange();
            if (range)
                text.remove(range->startPosition().offsetInContainerNode(), TextIterator::rangeLength(range.get()));
            return QVariant(text);
        }
        return QVariant();
    }
    case Qt::ImCurrentSelection: {
        if (!editor.hasComposition() && renderTextControl) {
            int start = frame->selection().selection().start().offsetInContainerNode();
            int end = frame->selection().selection().end().offsetInContainerNode();
            if (end > start)
                return QVariant(QString(renderTextControl->textFormControlElement().value()).mid(start, end - start));
        }
        return QVariant();

    }
    case Qt::ImAnchorPosition: {
        if (editor.hasComposition())
            return QVariant(frame->selection().selection().start().offsetInContainerNode());
        return QVariant(frame->selection().selection().base().offsetInContainerNode());
    }
    case Qt::ImMaximumTextLength: {
        if (frame->selection().selection().isContentEditable()) {
            if (frame->document() && frame->document()->focusedElement()) {
                if (is<HTMLInputElement>(frame->document()->focusedElement())) {
                    HTMLInputElement* inputElement = downcast<HTMLInputElement>(frame->document()->focusedElement());
                    return QVariant(inputElement->maxLength());
                }
            }
            return QVariant(HTMLInputElement::maximumLength);
        }
        return QVariant(0);
    }
    default:
        return QVariant();
    }
}

typedef struct {
    const char* name;
    double deferredRepaintDelay;
    double initialDeferredRepaintDelayDuringLoading;
    double maxDeferredRepaintDelayDuringLoading;
    double deferredRepaintDelayIncrementDuringLoading;
} QRepaintThrottlingPreset;

void QWebPageAdapter::dynamicPropertyChangeEvent(QObject* obj, QDynamicPropertyChangeEvent* event)
{
    if (event->propertyName() == "_q_viewMode") {
        page->setViewMode(Page::stringToViewMode(obj->property("_q_viewMode").toString()));
// FIXME:
//  CustomHTMLTokenizerChunkSize -> private setting defaultParserChunkSize
//  CustomHTMLTokenizerTimeDelay -> Settings::maxParseDuration
//  RepaintThrottling -> "Nowadays we throttle layer flushes" (r162837)
#if 0
    } else if (event->propertyName() == "_q_HTMLTokenizerChunkSize") {
        int chunkSize = obj->property("_q_HTMLTokenizerChunkSize").toInt();
        page->setCustomHTMLTokenizerChunkSize(chunkSize);
    } else if (event->propertyName() == "_q_HTMLTokenizerTimeDelay") {
        double timeDelay = obj->property("_q_HTMLTokenizerTimeDelay").toDouble();
        page->setCustomHTMLTokenizerTimeDelay(timeDelay);
    } else if (event->propertyName() == "_q_RepaintThrottlingDeferredRepaintDelay") {
        double p = obj->property("_q_RepaintThrottlingDeferredRepaintDelay").toDouble();
        FrameView::setRepaintThrottlingDeferredRepaintDelay(p);
    } else if (event->propertyName() == "_q_RepaintThrottlingnInitialDeferredRepaintDelayDuringLoading") {
        double p = obj->property("_q_RepaintThrottlingnInitialDeferredRepaintDelayDuringLoading").toDouble();
        FrameView::setRepaintThrottlingnInitialDeferredRepaintDelayDuringLoading(p);
    } else if (event->propertyName() == "_q_RepaintThrottlingMaxDeferredRepaintDelayDuringLoading") {
        double p = obj->property("_q_RepaintThrottlingMaxDeferredRepaintDelayDuringLoading").toDouble();
        FrameView::setRepaintThrottlingMaxDeferredRepaintDelayDuringLoading(p);
    } else if (event->propertyName() == "_q_RepaintThrottlingDeferredRepaintDelayIncrementDuringLoading") {
        double p = obj->property("_q_RepaintThrottlingDeferredRepaintDelayIncrementDuringLoading").toDouble();
        FrameView::setRepaintThrottlingDeferredRepaintDelayIncrementDuringLoading(p);
    } else if (event->propertyName() == "_q_RepaintThrottlingPreset") {
        static const QRepaintThrottlingPreset presets[] = {
            {   "NoThrottling",     0,      0,      0,      0 },
            {   "Legacy",       0.025,      0,    2.5,    0.5 },
            {   "Minimal",       0.01,      0,      1,    0.2 },
            {   "Medium",       0.025,      1,      5,    0.5 },
            {   "Heavy",          0.1,      2,     10,      1 }
        };

        QString p = obj->property("_q_RepaintThrottlingPreset").toString();
        for (size_t i = 0; i < sizeof(presets) / sizeof(presets[0]); i++) {
            if (p == QLatin1String(presets[i].name)) {
                FrameView::setRepaintThrottlingDeferredRepaintDelay(presets[i].deferredRepaintDelay);
                FrameView::setRepaintThrottlingnInitialDeferredRepaintDelayDuringLoading(presets[i].initialDeferredRepaintDelayDuringLoading);
                FrameView::setRepaintThrottlingMaxDeferredRepaintDelayDuringLoading(presets[i].maxDeferredRepaintDelayDuringLoading);
                FrameView::setRepaintThrottlingDeferredRepaintDelayIncrementDuringLoading(presets[i].deferredRepaintDelayIncrementDuringLoading);
                break;
            }
        }
#endif
    } else if (event->propertyName() == "_q_deadDecodedDataDeletionInterval") {
        double interval = obj->property("_q_deadDecodedDataDeletionInterval").toDouble();
        MemoryCache::singleton().setDeadDecodedDataDeletionInterval(
                std::chrono::milliseconds { static_cast<std::chrono::milliseconds::rep>(interval * 1000) });
    }  else if (event->propertyName() == "_q_useNativeVirtualKeyAsDOMKey") {
        m_useNativeVirtualKeyAsDOMKey = obj->property("_q_useNativeVirtualKeyAsDOMKey").toBool();
    }
}

#define MAP_ACTION_FROM_VALUE(Name, Value) \
    case Value: return QWebPageAdapter::Name

static int adapterActionForContextMenuAction(WebCore::ContextMenuAction action)
{
    if (action >= ContextMenuItemBaseCustomTag && action <= ContextMenuItemLastCustomTag)
        return action;

    switch (action) {
        FOR_EACH_MAPPED_MENU_ACTION(MAP_ACTION_FROM_VALUE, SEMICOLON_SEPARATOR);
    default:
        break;
    }
    return QWebPageAdapter::NoAction;
}

QList<MenuItem> descriptionForPlatformMenu(const Vector<ContextMenuItem>& items, Page* page)
{
    QList<MenuItem> itemDescriptions;
    if (!items.size())
        return itemDescriptions;
    for (const auto& item : items) {
        MenuItem description;
        switch (item.type()) {
        case WebCore::CheckableActionType: /* fall through */
        case WebCore::ActionType: {
            int action = adapterActionForContextMenuAction(item.action());
            if (action > QWebPageAdapter::NoAction) {
                description.type = MenuItem::Action;
                description.action = action;
                ContextMenuItem it(item);
                page->contextMenuController().checkOrEnableIfNeeded(it);
                if (it.enabled())
                    description.traits |= MenuItem::Enabled;
                if (item.type() == WebCore::CheckableActionType) {
                    description.traits |= MenuItem::Checkable;
                    if (it.checked())
                        description.traits |= MenuItem::Checked;
                }
                description.title = item.title();
            }
            break;
        }
        case WebCore::SeparatorType:
            description.type = MenuItem::Separator;
            break;
        case WebCore::SubmenuType: {
            description.type = MenuItem::SubMenu;
            description.subMenu = descriptionForPlatformMenu(item.subMenuItems(), page);
            description.title = item.title();
            // Don't append empty submenu descriptions.
            if (description.subMenu.isEmpty())
                continue;
        }
        }
        if (description.type > MenuItem::NoType)
            itemDescriptions.append(description);
    }
    return itemDescriptions;
}

QWebHitTestResultPrivate* QWebPageAdapter::updatePositionDependentMenuActions(const QPoint& pos, QBitArray* visitedWebActions)
{
    ASSERT(visitedWebActions);
    WebCore::Frame& focusedFrame = page->focusController().focusedOrMainFrame();
    HitTestResult result = focusedFrame.eventHandler().hitTestResultAtPoint(focusedFrame.view()->windowToContents(pos));
    page->contextMenuController().setHitTestResult(result);

    WebCore::ContextMenu* webcoreMenu = page->contextMenuController().contextMenu();
    QList<MenuItem> itemDescriptions;
    if (client && webcoreMenu)
        itemDescriptions = descriptionForPlatformMenu(webcoreMenu->items(), page);
        
    if (result.scrollbar())
        return 0;
    return new QWebHitTestResultPrivate(result);
}

static void extractContentTypeFromHash(const HashSet<String, ASCIICaseInsensitiveHash>& types, QStringList& list)
{
    for (auto& type : types)
        list << type;
}

QStringList QWebPageAdapter::supportedContentTypes() const
{
    QStringList mimeTypes;

    extractContentTypeFromHash(MIMETypeRegistry::getSupportedImageMIMETypes(), mimeTypes);
    extractContentTypeFromHash(MIMETypeRegistry::getSupportedNonImageMIMETypes(), mimeTypes);

    return mimeTypes;
}

void QWebPageAdapter::_q_cleanupLeakMessages()
{
#ifndef NDEBUG
    // Need this to make leak messages accurate.
    MemoryCache::singleton().setCapacities(0, 0, 0);
#endif
}

void QWebPageAdapter::_q_onLoadProgressChanged(int)
{
    m_totalBytes = page->progress().totalPageAndResourceBytesToLoad();
    m_bytesReceived = page->progress().totalBytesReceived();
}

bool QWebPageAdapter::supportsContentType(const QString& mimeType) const
{
    const String type = mimeType.toLower();
    if (MIMETypeRegistry::isSupportedImageMIMEType(type))
        return true;

    if (MIMETypeRegistry::isSupportedNonImageMIMEType(type))
        return true;

    return false;
}

void QWebPageAdapter::updateActionInternal(QWebPageAdapter::MenuAction action, const char* commandName, bool* enabled, bool* checked)
{
    WebCore::FrameLoader& loader = mainFrameAdapter().frame->loader();
    WebCore::Editor& editor = page->focusController().focusedOrMainFrame().editor();

    switch (action) {
    case QWebPageAdapter::SetTextDirectionDefault:
    case QWebPageAdapter::SetTextDirectionLeftToRight:
    case QWebPageAdapter::SetTextDirectionRightToLeft:
        *enabled = editor.canEdit();
        *checked = false;
        break;
    default: {

        // if it's an editor command, let its logic determine state
        if (commandName) {
            Editor::Command command = editor.command(commandName);
            *enabled = command.isEnabled();
            if (*enabled)
                *checked = command.state() != FalseTriState;
            else
                *checked = false;
        }
        break;
    }
    }
}

#if ENABLE(VIDEO)
static WebCore::HTMLMediaElement* mediaElement(WebCore::Node* innerNonSharedNode)
{
    if (!innerNonSharedNode)
        return 0;

    if (!(innerNonSharedNode->renderer() && innerNonSharedNode->renderer()->isMedia()))
        return 0;

    if (innerNonSharedNode->hasTagName(WebCore::HTMLNames::videoTag) || innerNonSharedNode->hasTagName(WebCore::HTMLNames::audioTag))
        return downcast<HTMLMediaElement>(innerNonSharedNode);
    return 0;
}
#endif

void QWebPageAdapter::triggerAction(QWebPageAdapter::MenuAction action, QWebHitTestResultPrivate* hitTestResult, const char* commandName, bool endToEndReload)
{
    Frame& frame = page->focusController().focusedOrMainFrame();
    Editor& editor = frame.editor();

    // Convenience
    QWebHitTestResultPrivate hitTest;
    if (!hitTestResult)
        hitTestResult = &hitTest;

    switch (action) {
    case SetTextDirectionDefault:
        editor.setBaseWritingDirection(NaturalWritingDirection);
        break;
    case SetTextDirectionLeftToRight:
        editor.setBaseWritingDirection(LeftToRightWritingDirection);
        break;
    case SetTextDirectionRightToLeft:
        editor.setBaseWritingDirection(RightToLeftWritingDirection);
        break;
#if ENABLE(VIDEO)
    case ToggleMediaControls:
        if (HTMLMediaElement* mediaElt = mediaElement(hitTestResult->innerNonSharedNode))
            mediaElt->setControls(!mediaElt->controls());
        break;
    case ToggleMediaLoop:
        if (HTMLMediaElement* mediaElt = mediaElement(hitTestResult->innerNonSharedNode))
            mediaElt->setLoop(!mediaElt->loop());
        break;
    case ToggleMediaPlayPause:
        if (HTMLMediaElement* mediaElt = mediaElement(hitTestResult->innerNonSharedNode))
            mediaElt->togglePlayState();
        break;
    case ToggleMediaMute:
        if (HTMLMediaElement* mediaElt = mediaElement(hitTestResult->innerNonSharedNode))
            mediaElt->setMuted(!mediaElt->muted());
        break;
    case ToggleVideoFullscreen:
        if (HTMLMediaElement* mediaElt = mediaElement(hitTestResult->innerNonSharedNode)) {
            if (mediaElt->isVideo() && mediaElt->supportsFullscreen(HTMLMediaElementEnums::VideoFullscreenModeStandard)) {
                UserGestureIndicator indicator(DefinitelyProcessingUserGesture);
                mediaElt->toggleFullscreenState();
            }
        }
        break;
#endif
    default:
        if (commandName)
            editor.command(commandName).execute();
        break;
    }
}

void QWebPageAdapter::triggerCustomAction(int action, const QString &title)
{
    if (action >= ContextMenuItemBaseCustomTag && action <= ContextMenuItemLastCustomTag)
        page->contextMenuController().contextMenuItemSelected(static_cast<ContextMenuAction>(action), title);
    else
        ASSERT_NOT_REACHED();
}


QString QWebPageAdapter::contextMenuItemTagForAction(QWebPageAdapter::MenuAction action, bool* checkable) const
{
    ASSERT(checkable);
    switch (action) {
    case CopyImageToClipboard:
        return contextMenuItemTagCopyImageToClipboard();
    case Cut:
        return contextMenuItemTagCut();
    case Copy:
        return contextMenuItemTagCopy();
    case Paste:
        return contextMenuItemTagPaste();
    case SelectAll:
        return contextMenuItemTagSelectAll();

    case SetTextDirectionDefault:
        return contextMenuItemTagDefaultDirection();
    case SetTextDirectionLeftToRight:
        *checkable = true;
        return contextMenuItemTagLeftToRight();
    case SetTextDirectionRightToLeft:
        *checkable = true;
        return contextMenuItemTagRightToLeft();

    case ToggleBold:
        *checkable = true;
        return contextMenuItemTagBold();
    case ToggleItalic:
        *checkable = true;
        return contextMenuItemTagItalic();
    case ToggleUnderline:
        *checkable = true;
        return contextMenuItemTagUnderline();
    case ToggleMediaControls:
        *checkable = true;
        return contextMenuItemTagShowMediaControls();
    case ToggleMediaLoop:
        *checkable = true;
        return contextMenuItemTagToggleMediaLoop();
    case ToggleMediaPlayPause:
        return contextMenuItemTagMediaPlayPause();
    case ToggleMediaMute:
        *checkable = true;
        return contextMenuItemTagMediaMute();
    case ToggleVideoFullscreen:
        return contextMenuItemTagToggleVideoFullscreen();

    default:
        ASSERT_NOT_REACHED();
        return QString();
    }
}

QObject* QWebPageAdapter::currentFrame() const
{
    Frame& frame = page->focusController().focusedOrMainFrame();
    return QWebFrameAdapter::kit(&frame)->handle();
}

bool QWebPageAdapter::hasFocusedNode() const
{
    bool hasFocus = false;
    Frame* frame = page->focusController().focusedFrame();
    if (frame) {
        Document* document = frame->document();
        hasFocus = document && document->focusedElement();
    }
    return hasFocus;
}

void QWebPageAdapter::setDevicePixelRatio(float devicePixelRatio)
{
    page->setDeviceScaleFactor(devicePixelRatio);
}

float QWebPageAdapter::devicePixelRatio()
{
    return page->deviceScaleFactor();
}

bool QWebPageAdapter::isPlayingAudio() const
{
    return page->mediaState() & MediaProducer::IsPlayingAudio;
}

bool QWebPageAdapter::handleKeyEvent(QKeyEvent *ev)
{
    Frame& frame = page->focusController().focusedOrMainFrame();
    return frame.eventHandler().keyEvent(PlatformKeyboardEvent(ev, m_useNativeVirtualKeyAsDOMKey));
}

bool QWebPageAdapter::handleScrolling(QKeyEvent *ev)
{
    Frame& frame = page->focusController().focusedOrMainFrame();
    WebCore::ScrollDirection direction;
    WebCore::ScrollGranularity granularity;

#ifndef QT_NO_SHORTCUT
    if (ev == QKeySequence::MoveToNextPage) {
        granularity = WebCore::ScrollByPage;
        direction = WebCore::ScrollDown;
    } else if (ev == QKeySequence::MoveToPreviousPage) {
        granularity = WebCore::ScrollByPage;
        direction = WebCore::ScrollUp;
    } else
#endif // QT_NO_SHORTCUT
    if ((ev->key() == Qt::Key_Up && ev->modifiers() & Qt::ControlModifier) || ev->key() == Qt::Key_Home) {
        granularity = WebCore::ScrollByDocument;
        direction = WebCore::ScrollUp;
    } else if ((ev->key() == Qt::Key_Down && ev->modifiers() & Qt::ControlModifier) || ev->key() == Qt::Key_End) {
        granularity = WebCore::ScrollByDocument;
        direction = WebCore::ScrollDown;
    } else {
        switch (ev->key()) {
        case Qt::Key_Up:
            granularity = WebCore::ScrollByLine;
            direction = WebCore::ScrollUp;
            break;
        case Qt::Key_Down:
            granularity = WebCore::ScrollByLine;
            direction = WebCore::ScrollDown;
            break;
        case Qt::Key_Left:
            granularity = WebCore::ScrollByLine;
            direction = WebCore::ScrollLeft;
            break;
        case Qt::Key_Right:
            granularity = WebCore::ScrollByLine;
            direction = WebCore::ScrollRight;
            break;
        default:
            return false;
        }
    }

    return frame.eventHandler().scrollRecursively(direction, granularity);
}

void QWebPageAdapter::focusInEvent(QFocusEvent *)
{
    FocusController& focusController = page->focusController();
    focusController.setActive(true);
    focusController.setFocused(true);
    if (!focusController.focusedFrame())
        focusController.setFocusedFrame(mainFrameAdapter().frame);
}

void QWebPageAdapter::focusOutEvent(QFocusEvent *)
{
    // only set the focused frame inactive so that we stop painting the caret
    // and the focus frame. But don't tell the focus controller so that upon
    // focusInEvent() we can re-activate the frame.
    FocusController& focusController = page->focusController();
    // Call setFocused first so that window.onblur doesn't get called twice
    focusController.setFocused(false);
    focusController.setActive(false);
}

bool QWebPageAdapter::handleShortcutOverrideEvent(QKeyEvent* event)
{
    WebCore::Frame& frame = page->focusController().focusedOrMainFrame();
    WebCore::Editor& editor = frame.editor();
    if (!editor.canEdit())
        return false;
    if (event->modifiers() == Qt::NoModifier
        || event->modifiers() == Qt::ShiftModifier
        || event->modifiers() == Qt::KeypadModifier) {
        if (event->key() < Qt::Key_Escape)
            event->accept();
        else {
            switch (event->key()) {
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_Delete:
            case Qt::Key_Home:
            case Qt::Key_End:
            case Qt::Key_Backspace:
            case Qt::Key_Left:
            case Qt::Key_Right:
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_Tab:
                event->accept();
            default:
                break;
            }
        }
    }
    return true;
}

bool QWebPageAdapter::touchEvent(QTouchEvent* event)
{
#if ENABLE(TOUCH_EVENTS)
    Frame* frame = mainFrameAdapter().frame;
    if (!frame->view() || !frame->document())
        return false;

    // If the document doesn't have touch-event handles, we just ignore it
    // and let QGuiApplication convert it to a mouse event.
    if (!frame->document()->hasTouchEventHandlers())
        return false;

    // Always accept the QTouchEvent so that we'll receive also TouchUpdate and TouchEnd events
    event->setAccepted(true);

    // Return whether the default action was cancelled in the JS event handler
    return frame->eventHandler().handleTouchEvent(convertTouchEvent(event));
#else
    event->ignore();
    return false;
#endif
}

bool QWebPageAdapter::swallowContextMenuEvent(QContextMenuEvent *event, QWebFrameAdapter *webFrame)
{
    // Check the first and last enum values match at least, since we cast.
    ASSERT(int(QWebPageAdapter::ScrollUp) == int(WebCore::ScrollUp));
    ASSERT(int(QWebPageAdapter::ScrollRight) == int(WebCore::ScrollRight));
    ASSERT(int(QWebPageAdapter::ScrollByLine) == int(WebCore::ScrollByLine));
    ASSERT(int(QWebPageAdapter::ScrollByDocument) == int(WebCore::ScrollByDocument));

    page->contextMenuController().clearContextMenu();

    if (webFrame) {
        Frame* frame = webFrame->frame;
        if (Scrollbar* scrollBar = frame->view()->scrollbarAtPoint(convertMouseEvent(event, 1).position())) {
            bool horizontal = (scrollBar->orientation() == HorizontalScrollbar);
            QWebPageAdapter::ScrollDirection direction = QWebPageAdapter::InvalidScrollDirection;
            QWebPageAdapter::ScrollGranularity granularity = QWebPageAdapter::InvalidScrollGranularity;
            
            /*bool scroll = handleScrollbarContextMenuEvent(event, horizontal, &direction, &granularity);
            if (!scroll)
                return true;
            if (direction == QWebPageAdapter::InvalidScrollDirection || granularity == QWebPageAdapter::InvalidScrollGranularity) {
                ScrollbarTheme& theme = scrollBar->theme();
                // Set the pressed position to the middle of the thumb so that when we
                // do move, the delta will be from the current pixel position of the
                // thumb to the new position
                int position = theme.trackPosition(*scrollBar) + theme.thumbPosition(*scrollBar) + theme.thumbLength(*scrollBar) / 2;
                scrollBar->setPressedPos(position);
                const QPoint pos = scrollBar->convertFromContainingWindow(event->pos());
                scrollBar->moveThumb(horizontal ? pos.x() : pos.y());
            } else
                scrollBar->scrollableArea().scroll(WebCore::ScrollDirection(direction), WebCore::ScrollGranularity(granularity));
            */  
           
            ///TODO: for hipecore, the above could be replaced with a special callback for
            //a scrollbar context menu, or toggle scroll-lock mode. It previously implemented
            //a scrollbar context menu (scroll up, scroll down, etc. which nobody ever used.)
            return true;
        }
    }

    WebCore::Frame& focusedFrame = page->focusController().focusedOrMainFrame();
    focusedFrame.eventHandler().sendContextMenuEvent(convertMouseEvent(event, 1));
    ContextMenu* menu = page->contextMenuController().contextMenu();
    // If the website defines its own handler then sendContextMenuEvent takes care of
    // calling/showing it and the context menu pointer will be zero. This is the case
    // on maps.google.com for example.

    return !menu;
}

void QWebPageAdapter::openNewWindow(const QUrl& url, Frame* frame)
{ //no-op
    
}
