/*
    Copyright (C) 2008, 2009, 2012 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2007 Staikos Computing Services Inc.
    Copyright (C) 2007 Apple Inc.
    Copyright (C) 2025-2026 General Development Systems

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "qwebpage.h"

#include "InitWebKitQt.h"
#include "PageClientQt.h"
#include "QGraphicsWidgetPluginImpl.h"
#include "QWebUndoCommand.h"
#include "QWidgetPluginImpl.h"
#include "QtFallbackWebPopup.h"
#include "QtPlatformPlugin.h"
#include "UndoStepQt.h"
#include "WebEventConversion.h"

#include "qwebframe.h"
#include "qwebframe_p.h"
#include "qwebkitplatformplugin.h"
#include "qwebpage_p.h"
#include "qwebsettings.h"
#include "qwebview.h"

#include <QAction>
#include <QApplication>
#include <QBasicTimer>
#include <QBitArray>
#include <QClipboard>
#include <QColorDialog>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QGestureEvent>
#include <QMenu>
#include <QPainter>
#include <QScreen>
#include <QStyle>
#include <QSysInfo>
#if USE(QT_MOBILITY_SYSTEMINFO)
#include <qsysteminfo.h>
#endif
#include <QSystemTrayIcon>
#include <QTextCharFormat>
#include <QToolTip>
#include <QTouchEvent>
#include <QUndoStack>
#include <QUrl>
#include <QWindow>
#if defined(Q_WS_X11)
#include <QX11Info>
#endif


using namespace WebCore;

// Lookup table mapping QWebPage::WebActions to the associated Editor commands
static const char* editorCommandWebActions[] =
{
    0, // CopyImageToClipboard,

    /*0, // Back,
    0, // Forward,
    0, // Stop,
    0, // Reload,*/

    "Cut", // Cut,
    "Copy", // Copy,
    "Paste", // Paste,

    "Undo", // Undo,
    "Redo", // Redo,
    "MoveForward", // MoveToNextChar,
    "MoveBackward", // MoveToPreviousChar,
    "MoveWordForward", // MoveToNextWord,
    "MoveWordBackward", // MoveToPreviousWord,
    "MoveDown", // MoveToNextLine,
    "MoveUp", // MoveToPreviousLine,
    "MoveToBeginningOfLine", // MoveToStartOfLine,
    "MoveToEndOfLine", // MoveToEndOfLine,
    "MoveToBeginningOfParagraph", // MoveToStartOfBlock,
    "MoveToEndOfParagraph", // MoveToEndOfBlock,
    "MoveToBeginningOfDocument", // MoveToStartOfDocument,
    "MoveToEndOfDocument", // MoveToEndOfDocument,
    "MoveForwardAndModifySelection", // SelectNextChar,
    "MoveBackwardAndModifySelection", // SelectPreviousChar,
    "MoveWordForwardAndModifySelection", // SelectNextWord,
    "MoveWordBackwardAndModifySelection", // SelectPreviousWord,
    "MoveDownAndModifySelection", // SelectNextLine,
    "MoveUpAndModifySelection", // SelectPreviousLine,
    "MoveToBeginningOfLineAndModifySelection", // SelectStartOfLine,
    "MoveToEndOfLineAndModifySelection", // SelectEndOfLine,
    "MoveToBeginningOfParagraphAndModifySelection", // SelectStartOfBlock,
    "MoveToEndOfParagraphAndModifySelection", // SelectEndOfBlock,
    "MoveToBeginningOfDocumentAndModifySelection", // SelectStartOfDocument,
    "MoveToEndOfDocumentAndModifySelection", // SelectEndOfDocument,
    "DeleteWordBackward", // DeleteStartOfWord,
    "DeleteWordForward", // DeleteEndOfWord,

    0, // SetTextDirectionDefault,
    0, // SetTextDirectionLeftToRight,
    0, // SetTextDirectionRightToLeft,

    "ToggleBold", // ToggleBold,
    "ToggleItalic", // ToggleItalic,
    "ToggleUnderline", // ToggleUnderline,

    "InsertNewline", // InsertParagraphSeparator
    "InsertLineBreak", // InsertLineSeparator

    "SelectAll", // SelectAll
    /*0, // ReloadAndBypassCache,*/

    "PasteAndMatchStyle", // PasteAndMatchStyle
    "RemoveFormat", // RemoveFormat
    "Strikethrough", // ToggleStrikethrough,
    "Subscript", // ToggleSubscript
    "Superscript", // ToggleSuperscript
    "InsertUnorderedList", // InsertUnorderedList
    "InsertOrderedList", // InsertOrderedList
    "Indent", // Indent
    "Outdent", // Outdent,

    "AlignCenter", // AlignCenter,
    "AlignJustified", // AlignJustified,
    "AlignLeft", // AlignLeft,
    "AlignRight", // AlignRight,

    0, // ToggleMediaControls,
    0, // ToggleMediaLoop,
    0, // ToggleMediaPlayPause,
    0, // ToggleMediaMute,
    0, // ToggleVideoFullscreen,

    "Unselect", // Unselect,

    0 // WebActionCount
};

// Lookup the appropriate editor command to use for WebAction \a action
const char* QWebPagePrivate::editorCommandForWebActions(QWebPage::WebAction action)
{
    if ((action > QWebPage::NoWebAction) && (action < int(sizeof(editorCommandWebActions) / sizeof(const char*))))
        return editorCommandWebActions[action];
    return 0;
}

QWebPagePrivate::QWebPagePrivate(QWebPage *qq)
    : q(qq)
#ifndef QT_NO_UNDOSTACK
#endif
    , m_viewportSize(QSize(0, 0))
    , useFixedLayout(false)
    , window(0)
    , m_lastDropAction(Qt::IgnoreAction)
    , contextMenuCallback(nullptr)
    , contextMenuUserPtr(nullptr)
{
    WebKit::initializeWebKitWidgets();
    initializeWebCorePage();
    memset(actions, 0, sizeof(actions));
}

void QWebPagePrivate::show()
{
    if (!view)
        return;
    view->window()->show();
}

void QWebPagePrivate::setFocus()
{
    if (!view)
        return;
    view->setFocus();
}

void QWebPagePrivate::unfocus()
{
    if (!view)
        return;
    view->clearFocus();
}

void QWebPagePrivate::setWindowRect(const QRect &rect)
{
    emit q->geometryChangeRequested(rect);
}

QSize QWebPagePrivate::viewportSize() const
{
    return q->viewportSize();
}

QWebPageAdapter *QWebPagePrivate::createWindow(bool dialog)
{
    QWebPage *newPage = q->createWindow(dialog ? QWebPage::WebModalDialog : QWebPage::WebBrowserWindow);
    if (!newPage)
        return 0;
    // Make sure the main frame exists, as WebCore expects it when returning from this ChromeClient::createWindow()
    newPage->d->createMainFrame();
    return newPage->d;
}

void QWebPagePrivate::consoleMessageReceived(MessageSource source, MessageLevel level, const QString& message, int lineNumber, const QString& sourceID)
{
    emit q->consoleMessageReceived(QWebPage::MessageSource(source), QWebPage::MessageLevel(level), message, lineNumber, sourceID);
}

void QWebPagePrivate::setToolTip(const QString &tip)
{
#ifndef QT_NO_TOOLTIP
    if (!view)
        return;

    if (tip.isEmpty()) {
        view->setToolTip(QString());
        QToolTip::hideText();
    } else {
        QString dtip = QLatin1String("<p>") + QString(tip).toHtmlEscaped() + QLatin1String("</p>");
        view->setToolTip(dtip);
    }
#else
    Q_UNUSED(tip);
#endif
}


QWebFrameAdapter& QWebPagePrivate::mainFrameAdapter()
{
    return *q->mainFrame()->d;
}

void QWebPagePrivate::emitRestoreFrameStateRequested(QWebFrameAdapter *frame)
{
    emit q->restoreFrameStateRequested(QWebFramePrivate::kit(frame));
}

void QWebPagePrivate::emitFrameCreated(QWebFrameAdapter *frame)
{
    emit q->frameCreated(QWebFramePrivate::kit(frame));
}

QtPluginWidgetAdapter *QWebPagePrivate::createPlugin(const QString &classid, const QUrl &url, const QStringList &paramNames, const QStringList &paramValues)
{
    QObject *widget = q->createPlugin(classid, url, paramNames, paramValues);
    return adapterForWidget(widget);
}

QtPluginWidgetAdapter *QWebPagePrivate::adapterForWidget(QObject *object) const
{
    if (QWidget *widget = qobject_cast<QWidget*>(object))
        return new QWidgetPluginImpl(widget);
#ifndef QT_NO_GRAPHICSVIEW
    if (QGraphicsWidget *widget = qobject_cast<QGraphicsWidget*>(object))
        return new QGraphicsWidgetPluginImpl(widget);
#endif
    return 0;
}

void QWebPagePrivate::createMainFrame()
{
    if (!mainFrame) {
        mainFrame = new QWebFrame(q);
        emit q->frameCreated(mainFrame.data());
    }
}

#define MAP_WEB_ACTION_FROM_ADAPTER_EQUIVALENT(Name, Value) \
    case QWebPageAdapter::Name: return QWebPage::Name

static QWebPage::WebAction webActionForAdapterMenuAction(QWebPageAdapter::MenuAction action)
{
    switch (action) {
        FOR_EACH_MAPPED_MENU_ACTION(MAP_WEB_ACTION_FROM_ADAPTER_EQUIVALENT, SEMICOLON_SEPARATOR);
    default:
        Q_UNREACHABLE();
        break;
    }
    return QWebPage::NoWebAction;
}

#define MAP_ADAPTER_ACTION_FROM_WEBACTION_EQUIVALENT(Name, Value) \
    case QWebPage::Name: return QWebPageAdapter::Name

static QWebPageAdapter::MenuAction adapterMenuActionForWebAction(QWebPage::WebAction action)
{
    switch (action) {
        FOR_EACH_MAPPED_MENU_ACTION(MAP_ADAPTER_ACTION_FROM_WEBACTION_EQUIVALENT, SEMICOLON_SEPARATOR);
    default:
        Q_UNREACHABLE();
        break;
    }
    return QWebPageAdapter::NoAction;
}

#ifndef QT_NO_ACTION
void QWebPagePrivate::_q_webActionTriggered(bool checked)
{
    QAction *a = qobject_cast<QAction *>(q->sender());
    if (!a)
        return;
    QWebPage::WebAction action = static_cast<QWebPage::WebAction>(a->data().toInt());
    q->triggerAction(action, checked);
}

void QWebPagePrivate::_q_customActionTriggered(bool checked)
{
    Q_UNUSED(checked);
    QAction* a = qobject_cast<QAction*>(q->sender());
    if (!a)
        return;
    int action = a->data().toInt();
    triggerCustomAction(action, a->text());
}
#endif // QT_NO_ACTION

void QWebPagePrivate::updateAction(QWebPage::WebAction action)
{
#ifdef QT_NO_ACTION
    Q_UNUSED(action)
#else
    QAction *a = actions[action];
    if (!a || !mainFrame)
        return;

    bool enabled = a->isEnabled();
    bool checked = a->isChecked();

    QWebPageAdapter::MenuAction mappedAction = QWebPageAdapter::NoAction;
    const char* commandName = 0;

    switch (action) {
    case QWebPage::SetTextDirectionDefault:
    case QWebPage::SetTextDirectionLeftToRight:
    case QWebPage::SetTextDirectionRightToLeft:
        mappedAction = adapterMenuActionForWebAction(action);
        break;
    case QWebPage::SelectAll: // editor command is always enabled
        break;
    default: {
        // see if it's an editor command
        commandName = editorCommandForWebActions(action);
        break;
    }
    }
    if (mappedAction != QWebPageAdapter::NoAction || commandName)
        updateActionInternal(mappedAction, commandName, &enabled, &checked);

    a->setEnabled(enabled);

    if (a->isCheckable())
        a->setChecked(checked);
#endif // QT_NO_ACTION
}


void QWebPagePrivate::clearCustomActions()
{
    qDeleteAll(customActions);
    customActions.clear();
}

void QWebPagePrivate::updateUndoActions()
{
    updateAction(QWebPage::Undo); //these reflect the focused frame's undo history
    updateAction(QWebPage::Redo);
}

void QWebPagePrivate::updateEditorActions()
{
    updateUndoActions();
    updateAction(QWebPage::Cut);
    updateAction(QWebPage::Copy);
    updateAction(QWebPage::Paste);
    updateAction(QWebPage::MoveToNextChar);
    updateAction(QWebPage::MoveToPreviousChar);
    updateAction(QWebPage::MoveToNextWord);
    updateAction(QWebPage::MoveToPreviousWord);
    updateAction(QWebPage::MoveToNextLine);
    updateAction(QWebPage::MoveToPreviousLine);
    updateAction(QWebPage::MoveToStartOfLine);
    updateAction(QWebPage::MoveToEndOfLine);
    updateAction(QWebPage::MoveToStartOfBlock);
    updateAction(QWebPage::MoveToEndOfBlock);
    updateAction(QWebPage::MoveToStartOfDocument);
    updateAction(QWebPage::MoveToEndOfDocument);
    updateAction(QWebPage::SelectNextChar);
    updateAction(QWebPage::SelectPreviousChar);
    updateAction(QWebPage::SelectNextWord);
    updateAction(QWebPage::SelectPreviousWord);
    updateAction(QWebPage::SelectNextLine);
    updateAction(QWebPage::SelectPreviousLine);
    updateAction(QWebPage::SelectStartOfLine);
    updateAction(QWebPage::SelectEndOfLine);
    updateAction(QWebPage::SelectStartOfBlock);
    updateAction(QWebPage::SelectEndOfBlock);
    updateAction(QWebPage::SelectStartOfDocument);
    updateAction(QWebPage::SelectEndOfDocument);
    updateAction(QWebPage::DeleteStartOfWord);
    updateAction(QWebPage::DeleteEndOfWord);
    updateAction(QWebPage::SetTextDirectionDefault);
    updateAction(QWebPage::SetTextDirectionLeftToRight);
    updateAction(QWebPage::SetTextDirectionRightToLeft);
    updateAction(QWebPage::ToggleBold);
    updateAction(QWebPage::ToggleItalic);
    updateAction(QWebPage::ToggleUnderline);
    updateAction(QWebPage::InsertParagraphSeparator);
    updateAction(QWebPage::InsertLineSeparator);
    updateAction(QWebPage::PasteAndMatchStyle);
    updateAction(QWebPage::RemoveFormat);
    updateAction(QWebPage::ToggleStrikethrough);
    updateAction(QWebPage::ToggleSubscript);
    updateAction(QWebPage::ToggleSuperscript);
    updateAction(QWebPage::InsertUnorderedList);
    updateAction(QWebPage::InsertOrderedList);
    updateAction(QWebPage::Indent);
    updateAction(QWebPage::Outdent);
    updateAction(QWebPage::AlignCenter);
    updateAction(QWebPage::AlignJustified);
    updateAction(QWebPage::AlignLeft);
    updateAction(QWebPage::AlignRight);
    updateAction(QWebPage::Unselect);
}

void QWebPagePrivate::timerEvent(QTimerEvent *ev)
{
    int timerId = ev->timerId();
    if (timerId == tripleClickTimer.timerId())
        tripleClickTimer.stop();
    else
        q->timerEvent(ev);
}

bool QWebPagePrivate::requestSoftwareInputPanel() const
{
    return QStyle::RequestSoftwareInputPanel(client->style()->styleHint(QStyle::SH_RequestSoftwareInputPanel)) == QStyle::RSIP_OnMouseClick;
}

#ifndef QT_NO_CONTEXTMENU

void QWebPage::setContextMenuCallback(ContextMenuCallback callback, void* userPtr)
{
    d->contextMenuCallback = callback;
    d->contextMenuUserPtr = userPtr;
}

void QWebPage::clearContextMenuCallback()
{
    d->contextMenuCallback = nullptr;
    d->contextMenuUserPtr = nullptr;
}

void QWebPagePrivate::contextMenuEvent(const QPoint& globalPos)
{
    
    //Refer to ConextMenuController class for how to know which commands should be enabled.
    //These functions are no longer used but it's useful to know what was dismantled.
    /*ContextMenuController::handleContextMenuEvent(Event*)
      ContextMenuController::maybeCreateContextMenu(Event*)  // Creates menu structure
      ContextMenuController::populate()  // Populates menu items based on context
      ContextMenuController::checkOrEnableIfNeeded(ContextMenuItem&)  // For each item
    */

    //For hipecore, a custom callback is invoked.
    // Check if hipe has set a custom callback
    if (contextMenuCallback) {
        // Get the element at the clicked position
        QWebElement element;
        QWebFrame* frame = q->frameAt(globalPos); // You might need to convert coordinates
        if (frame) {
            QWebHitTestResult hitTest = frame->hitTestContent(globalPos); // Convert to local coordinates
            element = hitTest.element();
        }
        
        // Call the user's callback
        contextMenuCallback(contextMenuUserPtr, globalPos, element);
        return; // Don't show default context menu
    }


}
#endif // QT_NO_CONTEXTMENU


#ifndef QT_NO_SHORTCUT
QWebPage::WebAction QWebPagePrivate::editorActionForKeyEvent(QKeyEvent* event)
{
    static struct {
        QKeySequence::StandardKey standardKey;
        QWebPage::WebAction action;
    } editorActions[] = {
        { QKeySequence::Cut, QWebPage::Cut },
        { QKeySequence::Copy, QWebPage::Copy },
        { QKeySequence::Paste, QWebPage::Paste },
        { QKeySequence::Undo, QWebPage::Undo },
        { QKeySequence::Redo, QWebPage::Redo },
        { QKeySequence::MoveToNextChar, QWebPage::MoveToNextChar },
        { QKeySequence::MoveToPreviousChar, QWebPage::MoveToPreviousChar },
        { QKeySequence::MoveToNextWord, QWebPage::MoveToNextWord },
        { QKeySequence::MoveToPreviousWord, QWebPage::MoveToPreviousWord },
        { QKeySequence::MoveToNextLine, QWebPage::MoveToNextLine },
        { QKeySequence::MoveToPreviousLine, QWebPage::MoveToPreviousLine },
        { QKeySequence::MoveToStartOfLine, QWebPage::MoveToStartOfLine },
        { QKeySequence::MoveToEndOfLine, QWebPage::MoveToEndOfLine },
        { QKeySequence::MoveToStartOfBlock, QWebPage::MoveToStartOfBlock },
        { QKeySequence::MoveToEndOfBlock, QWebPage::MoveToEndOfBlock },
        { QKeySequence::MoveToStartOfDocument, QWebPage::MoveToStartOfDocument },
        { QKeySequence::MoveToEndOfDocument, QWebPage::MoveToEndOfDocument },
        { QKeySequence::SelectNextChar, QWebPage::SelectNextChar },
        { QKeySequence::SelectPreviousChar, QWebPage::SelectPreviousChar },
        { QKeySequence::SelectNextWord, QWebPage::SelectNextWord },
        { QKeySequence::SelectPreviousWord, QWebPage::SelectPreviousWord },
        { QKeySequence::SelectNextLine, QWebPage::SelectNextLine },
        { QKeySequence::SelectPreviousLine, QWebPage::SelectPreviousLine },
        { QKeySequence::SelectStartOfLine, QWebPage::SelectStartOfLine },
        { QKeySequence::SelectEndOfLine, QWebPage::SelectEndOfLine },
        { QKeySequence::SelectStartOfBlock, QWebPage::SelectStartOfBlock },
        { QKeySequence::SelectEndOfBlock,  QWebPage::SelectEndOfBlock },
        { QKeySequence::SelectStartOfDocument, QWebPage::SelectStartOfDocument },
        { QKeySequence::SelectEndOfDocument, QWebPage::SelectEndOfDocument },
        { QKeySequence::DeleteStartOfWord, QWebPage::DeleteStartOfWord },
        { QKeySequence::DeleteEndOfWord, QWebPage::DeleteEndOfWord },
        { QKeySequence::InsertParagraphSeparator, QWebPage::InsertParagraphSeparator },
        { QKeySequence::InsertLineSeparator, QWebPage::InsertLineSeparator },
        { QKeySequence::SelectAll, QWebPage::SelectAll },
        { QKeySequence::Deselect, QWebPage::Unselect },
        { QKeySequence::UnknownKey, QWebPage::NoWebAction }
    };

    for (int i = 0; editorActions[i].standardKey != QKeySequence::UnknownKey; ++i)
        if (event == editorActions[i].standardKey)
            return editorActions[i].action;

    return QWebPage::NoWebAction;
}
#endif // QT_NO_SHORTCUT

void QWebPagePrivate::keyPressEvent(QKeyEvent *ev)
{
    // we forward the key event to WebCore first to handle potential DOM
    // defined event handlers and later on end up in EditorClientQt::handleKeyboardEvent
    // to trigger editor commands via triggerAction().
    bool handled = handleKeyEvent(ev);

    if (!handled)
        handled = handleScrolling(ev);

    if (!handled) {
        handled = true;
        switch (ev->key()) {
        /*case Qt::Key_Back:
            q->triggerAction(QWebPage::Back);
            break;
        case Qt::Key_Forward:
            q->triggerAction(QWebPage::Forward);
            break;
        case Qt::Key_Stop:
            q->triggerAction(QWebPage::Stop);
            break;
        case Qt::Key_Refresh:
            q->triggerAction(QWebPage::Reload);
            break;*/
        default:
            handled = false;
            break;
        }
    }

    ev->setAccepted(handled);
}

void QWebPagePrivate::keyReleaseEvent(QKeyEvent *ev)
{
    if (ev->isAutoRepeat()) {
        ev->setAccepted(true);
        return;
    }

    bool handled = handleKeyEvent(ev);
    ev->setAccepted(handled);
}

template<class T>
void QWebPagePrivate::dragEnterEvent(T* ev)
{
#if ENABLE(DRAG_SUPPORT)
    Qt::DropAction action = dragEntered(ev->mimeData(), QPointF(ev->pos()).toPoint(), ev->possibleActions());
    ev->setDropAction(action);
    ev->acceptProposedAction();
#endif
}

template<class T>
void QWebPagePrivate::dragMoveEvent(T *ev)
{
#if ENABLE(DRAG_SUPPORT)
    m_lastDropAction = dragUpdated(ev->mimeData(), QPointF(ev->pos()).toPoint(), ev->possibleActions());
    ev->setDropAction(m_lastDropAction);
    if (m_lastDropAction != Qt::IgnoreAction)
        ev->accept();
#endif
}

template<class T>
void QWebPagePrivate::dropEvent(T *ev)
{
#if ENABLE(DRAG_SUPPORT)
    if (performDrag(ev->mimeData(), QPointF(ev->pos()).toPoint(), ev->possibleActions())) {
        ev->setDropAction(m_lastDropAction);
        ev->accept();
    }
#endif
}

void QWebPagePrivate::leaveEvent(QEvent*)
{
    // If a mouse button is pressed we will continue to receive mouse events after leaving the window.
    if (mousePressed)
        return;

    // Fake a mouse move event just outside of the widget, since all
    // the interesting mouse-out behavior like invalidating scrollbars
    // is handled by the WebKit event handler's mouseMoved function.
    QMouseEvent fakeEvent(QEvent::MouseMove, QCursor::pos(), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    mouseMoveEvent(&fakeEvent);
}

/*!
    \property QWebPage::palette
    \brief the page's palette

    The base brush of the palette is used to draw the background of the main frame.

    By default, this property contains the application's default palette.
*/
void QWebPage::setPalette(const QPalette &pal)
{
    d->palette = pal;
    if (!d->mainFrame || !d->mainFrame.data()->d->hasView())
        return;

    QBrush brush = pal.brush(QPalette::Base);
    QColor backgroundColor = brush.style() == Qt::SolidPattern ? brush.color() : QColor();
    d->mainFrame.data()->d->updateBackgroundRecursively(backgroundColor);
}

QPalette QWebPage::palette() const
{
    return d->palette;
}

void QWebPagePrivate::shortcutOverrideEvent(QKeyEvent* event)
{
    if (handleShortcutOverrideEvent(event)) {
        if (event->isAccepted())
            return;
#ifndef QT_NO_SHORTCUT
        else if (editorActionForKeyEvent(event) != QWebPage::NoWebAction)
            event->accept();
#endif
    }
}

bool QWebPagePrivate::gestureEvent(QGestureEvent* event)
{
    QWebFrameAdapter* frame = mainFrame.data()->d;
    if (!frame->hasView())
        return false;
    // QGestureEvents can contain updates for multiple gestures.
    bool handled = false;
#if ENABLE(QT_GESTURE_EVENTS)
    // QGestureEvent lives in Widgets, we'll need a dummy struct to mule the info it contains to the "other side"
    QGestureEventFacade gestureFacade;

    QGesture* gesture = event->gesture(Qt::TapGesture);
    // Beware that gestures send by DumpRenderTree will have state Qt::NoGesture,
    // due to not originating from a GestureRecognizer.
    if (gesture && (gesture->state() == Qt::GestureStarted || gesture->state() == Qt::NoGesture)) {
        gestureFacade.type = Qt::TapGesture;
        QPointF globalPos = static_cast<const QTapGesture*>(gesture)->position();
        gestureFacade.globalPos = globalPos.toPoint();
        gestureFacade.pos = event->widget()->mapFromGlobal(globalPos.toPoint());
        frame->handleGestureEvent(&gestureFacade);
        handled = true;
    }
    gesture = event->gesture(Qt::TapAndHoldGesture);
    if (gesture && (gesture->state() == Qt::GestureStarted || gesture->state() == Qt::NoGesture)) {
        gestureFacade.type = Qt::TapAndHoldGesture;
        QPointF globalPos = static_cast<const QTapAndHoldGesture*>(gesture)->position();
        gestureFacade.globalPos = globalPos.toPoint();
        gestureFacade.pos = event->widget()->mapFromGlobal(globalPos.toPoint());
        frame->handleGestureEvent(&gestureFacade);
        handled = true;
    }
#endif // ENABLE(QT_GESTURE_EVENTS)

    event->setAccepted(handled);
    return handled;
}

/*!
  This method is used by the input method to query a set of properties of the page
  to be able to support complex input method operations as support for surrounding
  text and reconversions.

  \a property specifies which property is queried.

  \sa QWidget::inputMethodEvent(), QInputMethodEvent
*/
QVariant QWebPage::inputMethodQuery(Qt::InputMethodQuery property) const
{
    return d->inputMethodQuery(property);
}

/*!
   \enum QWebPage::FindFlag

   This enum describes the options available to the findText() function. The options
   can be OR-ed together from the following list:

   \value FindBackward Searches backwards instead of forwards.
   \value FindCaseSensitively By default findText() works case insensitive. Specifying this option
   changes the behaviour to a case sensitive find operation.
   \value FindWrapsAroundDocument Makes findText() restart from the beginning of the document if the end
   was reached and the text was not found.
   \value HighlightAllOccurrences Highlights all existing occurrences of a specific string.
       (This value was introduced in 4.6.)
   \value FindAtWordBeginningsOnly Searches for the sub-string only at the beginnings of words.
       (This value was introduced in 5.2.)
   \value TreatMedialCapitalAsWordBeginning Treats a capital letter occurring anywhere in the middle of a word
   as the beginning of a new word.
       (This value was introduced in 5.2.)
   \value FindBeginsInSelection Begin searching inside the text selection first.
       (This value was introduced in 5.2.)
*/

/*!
    \enum QWebPage::VisibilityState

    This enum defines visibility states that a webpage can take.

    \value VisibilityStateVisible The webpage is at least partially visible at on at least one screen.
    \value VisibilityStateHidden The webpage is not visible at all on any screen.
    \value VisibilityStatePrerender The webpage is loaded off-screen and is not visible.
    \value VisibilityStateUnloaded The webpage is unloading its content.
    More information about this values can be found at \l{ http://www.w3.org/TR/page-visibility/#dom-document-visibilitystate}{W3C Recommendation: Page Visibility: visibilityState attribute}.

    \sa QWebPage::visibilityState
*/

/*!
    \enum QWebPage::WebAction

    This enum describes the types of action which can be performed on the web page.

    Actions only have an effect when they are applicable. The availability of
    actions can be be determined by checking \l{QAction::}{isEnabled()} on the
    action returned by action().

    One method of enabling the text editing, cursor movement, and text selection actions
    is by setting \l contentEditable to true.

    \value NoWebAction No action is triggered.
    \value CopyImageToClipboard Copy the highlighted image to the clipboard.  (Added in Qt 4.8)
    \value Back Navigate back in the history of navigated links.
    \value Forward Navigate forward in the history of navigated links.
    \value Stop Stop loading the current page.
    \value Cut Cut the content currently selected into the clipboard.
    \value Copy Copy the content currently selected into the clipboard.
    \value Paste Paste content from the clipboard.
    \value Undo Undo the last editing action.
    \value Redo Redo the last editing action.
    \value MoveToNextChar Move the cursor to the next character.
    \value MoveToPreviousChar Move the cursor to the previous character.
    \value MoveToNextWord Move the cursor to the next word.
    \value MoveToPreviousWord Move the cursor to the previous word.
    \value MoveToNextLine Move the cursor to the next line.
    \value MoveToPreviousLine Move the cursor to the previous line.
    \value MoveToStartOfLine Move the cursor to the start of the line.
    \value MoveToEndOfLine Move the cursor to the end of the line.
    \value MoveToStartOfBlock Move the cursor to the start of the block.
    \value MoveToEndOfBlock Move the cursor to the end of the block.
    \value MoveToStartOfDocument Move the cursor to the start of the document.
    \value MoveToEndOfDocument Move the cursor to the end of the document.
    \value SelectNextChar Select to the next character.
    \value SelectPreviousChar Select to the previous character.
    \value SelectNextWord Select to the next word.
    \value SelectPreviousWord Select to the previous word.
    \value SelectNextLine Select to the next line.
    \value SelectPreviousLine Select to the previous line.
    \value SelectStartOfLine Select to the start of the line.
    \value SelectEndOfLine Select to the end of the line.
    \value SelectStartOfBlock Select to the start of the block.
    \value SelectEndOfBlock Select to the end of the block.
    \value SelectStartOfDocument Select to the start of the document.
    \value SelectEndOfDocument Select to the end of the document.
    \value DeleteStartOfWord Delete to the start of the word.
    \value DeleteEndOfWord Delete to the end of the word.
    \value SetTextDirectionDefault Set the text direction to the default direction.
    \value SetTextDirectionLeftToRight Set the text direction to left-to-right.
    \value SetTextDirectionRightToLeft Set the text direction to right-to-left.
    \value ToggleBold Toggle the formatting between bold and normal weight.
    \value ToggleItalic Toggle the formatting between italic and normal style.
    \value ToggleUnderline Toggle underlining.
    \value InsertParagraphSeparator Insert a new paragraph.
    \value InsertLineSeparator Insert a new line.
    \value SelectAll Selects all content.
    \value PasteAndMatchStyle Paste content from the clipboard with current style. (Added in Qt 4.6)
    \value RemoveFormat Removes formatting and style. (Added in Qt 4.6)
    \value ToggleStrikethrough Toggle the formatting between strikethrough and normal style. (Added in Qt 4.6)
    \value ToggleSubscript Toggle the formatting between subscript and baseline. (Added in Qt 4.6)
    \value ToggleSuperscript Toggle the formatting between supercript and baseline. (Added in Qt 4.6)
    \value InsertUnorderedList Toggles the selection between an ordered list and a normal block. (Added in Qt 4.6)
    \value InsertOrderedList Toggles the selection between an ordered list and a normal block. (Added in Qt 4.6)
    \value Indent Increases the indentation of the currently selected format block by one increment. (Added in Qt 4.6)
    \value Outdent Decreases the indentation of the currently selected format block by one increment. (Added in Qt 4.6)
    \value AlignCenter Applies center alignment to content. (Added in Qt 4.6)
    \value AlignJustified Applies full justification to content. (Added in Qt 4.6)
    \value AlignLeft Applies left justification to content. (Added in Qt 4.6)
    \value AlignRight Applies right justification to content. (Added in Qt 4.6)
    \value ToggleMediaControls Toggles between showing and hiding the controls for the hovered audio or video element. (Added in Qt 5.2)
    \value ToggleMediaLoop Toggles whether the hovered audio or video should loop on completetion or not. (Added in Qt 5.2)
    \value ToggleMediaPlayPause Toggles the play/pause state of the hovered audio or video element. (Added in Qt 5.2)
    \value ToggleMediaMute Mutes or unmutes the hovered audio or video element. (Added in Qt 5.2)
    \value ToggleVideoFullscreen Switches the hovered video element into or out of fullscreen mode. (Added in Qt 5.2)
    \value Unselect Deselects existing selection. (Added in QtWebKit 5.9)

    \omitvalue WebActionCount

*/

/*!
    \enum QWebPage::WebWindowType

    This enum describes the types of window that can be created by the createWindow() function.

    \value WebBrowserWindow The window is a regular web browser window.
    \value WebModalDialog The window acts as modal dialog.
*/

/*!
    \class QWebPage
    \since 4.4
    \brief The QWebPage class provides an object to view and edit web documents.

    \inmodule QtWebKit

    QWebPage holds a main frame responsible for web content, settings, the history
    of navigated links and actions. This class can be used, together with QWebFrame,
    to provide functionality like QWebView in a widget-less environment.

    QWebPage's API is very similar to QWebView, as you are still provided with
    common functions like action() (known as
    \l{QWebView::pageAction()}{pageAction}() in QWebView), triggerAction(),
    findText() and settings(). More QWebView-like functions can be found in the
    main frame of QWebPage, obtained via the mainFrame() function. For example,
    the \l{QWebFrame::load()}{load}(), \l{QWebFrame::setUrl()}{setUrl}() and
    \l{QWebFrame::setHtml()}{setHtml}() functions for QWebPage can be accessed
    using QWebFrame.

    The loadStarted() signal is emitted when the page begins to load.The
    loadProgress() signal, on the other hand, is emitted whenever an element
    of the web page completes loading, such as an embedded image, a script,
    etc. Finally, the loadFinished() signal is emitted when the page contents
    are loaded completely, independent of script execution or page rendering.
    Its argument, either true or false, indicates whether or not the load
    operation succeeded.

    \section1 Using QWebPage in a Widget-less Environment

    Before you begin painting a QWebPage object, you need to set the size of
    the viewport by calling setViewportSize(). Then, you invoke the main
    frame's render function (QWebFrame::render()). An example of this
    is shown in the code snippet below.

    Suppose we have a \c Thumbnail class as follows:

    \snippet webkitsnippets/webpage/main.cpp 0

    The \c Thumbnail's constructor takes in a \a url. We connect our QWebPage
    object's \l{QWebPage::}{loadFinished()} signal to our private slot,
    \c render().

    \snippet webkitsnippets/webpage/main.cpp 1

    The \c render() function shows how we can paint a thumbnail using a
    QWebPage object.

    \snippet webkitsnippets/webpage/main.cpp 2

    We begin by setting the \l{QWebPage::viewportSize()}{viewportSize} and
    then we instantiate a QImage object, \c image, with the same size as our
    \l{QWebPage::viewportSize()}{viewportSize}. This image is then sent
    as a parameter to \c painter. Next, we render the contents of the main
    frame and its subframes into \c painter. Finally, we save the scaled image.

    \sa QWebFrame
*/

/*!
    Constructs an empty QWebPage with parent \a parent.
*/
QWebPage::QWebPage(QObject *parent)
    : QObject(parent)
    , d(new QWebPagePrivate(this))
{
    setView(qobject_cast<QWidget*>(parent));

    connect(this, SIGNAL(loadProgress(int)), this, SLOT(_q_onLoadProgressChanged(int)));
#ifndef NDEBUG
    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()), this, SLOT(_q_cleanupLeakMessages()));
#endif
}

/*!
    Destroys the web page.
*/
QWebPage::~QWebPage()
{
    delete d;
}

/*!
    Returns the main frame of the page.

    The main frame provides access to the hierarchy of sub-frames and is also needed if you
    want to explicitly render a web page into a given painter.

    \sa currentFrame()
*/
QWebFrame *QWebPage::mainFrame() const
{
    d->createMainFrame();
    return d->mainFrame.data();
}

/*!
    Returns the frame currently active.

    \sa mainFrame(), frameCreated()
*/
QWebFrame *QWebPage::currentFrame() const
{
    d->createMainFrame();
    return qobject_cast<QWebFrame*>(d->currentFrame());
}


/*!
    \since 4.6

    Returns the frame at the given point \a pos, or 0 if there is no frame at
    that position.

    \sa mainFrame(), currentFrame()
*/
QWebFrame* QWebPage::frameAt(const QPoint& pos) const
{
    QWebFrame* webFrame = mainFrame();
    if (!webFrame->geometry().contains(pos))
        return 0;
    QWebHitTestResult hitTestResult = webFrame->hitTestContent(pos);
    return hitTestResult.frame();
}

/*!
    Sets the \a view that is associated with the web page.

    \sa view()
*/
void QWebPage::setView(QWidget* view)
{
    if (this->view() == view)
        return;

    d->view = view;
    setViewportSize(view ? view->size() : QSize(0, 0));

    // If we have no client, we install a special client delegating
    // the responsibility to the QWidget. This is the code path
    // handling a.o. the "legacy" QWebView.
    //
    // If such a special delegate already exist, we substitute the view.

    if (d->client) {
        if (d->client->isQWidgetClient())
            static_cast<PageClientQWidget*>(d->client.data())->view = view;
        return;
    }

    if (view)
        d->client.reset(new PageClientQWidget(view, this));
}

/*!
    Returns the view widget that is associated with the web page.

    \sa setView()
*/
QWidget *QWebPage::view() const
{
    return d->view.data();
}

/*!
    This function is called whenever WebKit wants to create a new window of the given \a type, for
    example when a JavaScript program requests to open a document in a new window.

    If the new window can be created, the new window's QWebPage is returned; otherwise a null pointer is returned.

    If the view associated with the web page is a QWebView object, then the default implementation forwards
    the request to QWebView's createWindow() function; otherwise it returns a null pointer.

    If \a type is WebModalDialog, the application must call setWindowModality(Qt::ApplicationModal) on the new window.

    \note In the cases when the window creation is being triggered by JavaScript, apart from
    reimplementing this method application must also set the JavaScriptCanOpenWindows attribute
    of QWebSettings to true in order for it to get called.

    \sa QWebView::createWindow()
*/
QWebPage *QWebPage::createWindow(WebWindowType type)
{
    QWebView *webView = qobject_cast<QWebView*>(view());
    if (webView) {
        QWebView *newView = webView->createWindow(type);
        if (newView)
            return newView->page();
    }
    return 0;
}

/*!
    This function is called whenever WebKit encounters a HTML object element with type "application/x-qt-plugin". It is
    called regardless of the value of QWebSettings::PluginsEnabled. The \a classid, \a url, \a paramNames and \a paramValues
    correspond to the HTML object element attributes and child elements to configure the embeddable object.
*/
QObject *QWebPage::createPlugin(const QString &classid, const QUrl &url, const QStringList &paramNames, const QStringList &paramValues)
{
    Q_UNUSED(classid);
    Q_UNUSED(url);
    Q_UNUSED(paramNames);
    Q_UNUSED(paramValues);
    return 0;
}

/*!
 *  Returns the list of all content types supported by QWebPage.
 */
QStringList QWebPage::supportedContentTypes() const
{
    return d->supportedContentTypes();
}

/*!
 *  Returns true if QWebPage can handle the given \a mimeType; otherwise, returns false.
 */
bool QWebPage::supportsContentType(const QString& mimeType) const
{
    return d->supportsContentType(mimeType);
}

/*!
    Inserts \a text at the selection of the frame that has focus, as if it had been typed: it replaces
    the selected text, can be undone as one step (together with typing immediately before it), and does
    nothing unless the selection is in editable content. Each newline in \a text becomes a line break
    and never a new paragraph. If \a text is empty, the selected text is deleted.

    Unlike committing the text through an input method event, text with many lines is inserted as a
    single edit where the content keeps its white space (for example in a \c{<pre>}).
*/
void QWebPage::insertText(const QString& text)
{
    d->insertText(text);
}

/*!
    This function can be called to trigger the specified \a action.
    It is also called by Qt WebKit if the user triggers the action, for example
    through a context menu item.

    If \a action is a checkable action then \a checked specified whether the action
    is toggled or not.

    \sa action()
*/
void QWebPage::triggerAction(WebAction action, bool)
{
    const char *command = 0;
    QWebPageAdapter::MenuAction mappedAction = QWebPageAdapter::NoAction;
    QWebHitTestResultPrivate* hitTestResult = d->hitTestResult.d;

    switch (action) {
    /*case Back:
    case Forward:
    case Stop:
    case Reload:*/
    case SetTextDirectionDefault:
    case SetTextDirectionLeftToRight:
    case SetTextDirectionRightToLeft:
    case ToggleMediaControls:
    case ToggleMediaLoop:
    case ToggleMediaPlayPause:
    case ToggleMediaMute:
    case ToggleVideoFullscreen:
        mappedAction = adapterMenuActionForWebAction(action);
        break;
    /*case ReloadAndBypassCache: // Manual mapping
        mappedAction = QWebPageAdapter::Reload;
        break;*/
#ifndef QT_NO_CLIPBOARD
    case CopyImageToClipboard:
        QApplication::clipboard()->setPixmap(d->hitTestResult.pixmap());
        break;
#endif
    default:
        command = QWebPagePrivate::editorCommandForWebActions(action);
        break;
    }
    if (command || mappedAction != QWebPageAdapter::NoAction)
        d->triggerAction(mappedAction, hitTestResult, command, false);
}


QColor QWebPagePrivate::colorSelectionRequested(const QColor &selectedColor)
{
    QColor ret = selectedColor;
#ifndef QT_NO_COLORDIALOG
    ret = QColorDialog::getColor(selectedColor, q->view());
    if (!ret.isValid())
        ret = selectedColor;
#endif
    return ret;
}

std::unique_ptr<QWebSelectMethod> QWebPagePrivate::createSelectPopup()
{
    return std::make_unique<QtFallbackWebPopup>(this);
}

QRect QWebPagePrivate::viewRectRelativeToWindow()
{

    QWidget* ownerWidget= client.isNull() ? 0 : qobject_cast<QWidget*>(client->ownerWidget());
    if (!ownerWidget)
        return QRect();
    QWidget* topLevelWidget = ownerWidget->window();

    QPoint topLeftCorner = ownerWidget->mapFrom(topLevelWidget, QPoint(0, 0));
    return QRect(topLeftCorner, ownerWidget->size());
}

void QWebPagePrivate::respondToChangedContents()
{
    updateEditorActions();

    emit q->contentsChanged();
}

void QWebPagePrivate::respondToChangedSelection()
{
    updateEditorActions();
    emit q->selectionChanged();
}

void QWebPagePrivate::focusedFrameChanged()
{
    // Selection changes are reported before focus moves to a newly clicked frame, so the edit
    // actions (e.g. Undo, which follows the focused frame's own history) need refreshing here too.
    updateEditorActions();
}

void QWebPagePrivate::microFocusChanged()
{
    emit q->microFocusChanged();
}

void QWebPagePrivate::triggerCopyAction()
{
    q->triggerAction(QWebPage::Copy);
}

void QWebPagePrivate::triggerActionForKeyEvent(QKeyEvent* event)
{
    QWebPage::WebAction action = editorActionForKeyEvent(event);
    q->triggerAction(action);
}

#ifndef QT_NO_UNDOSTACK
QUndoStack* QWebPagePrivate::undoStackForGroup(const void* group) const
{
    QUndoStack*& stack = undoStacks[group];
    if (!stack)
        stack = new QUndoStack(q);
    return stack;
}
#endif

void QWebPagePrivate::removeUndoStacksExcept(const QSet<const void*>& liveGroups)
{
#ifndef QT_NO_UNDOSTACK
    for (auto it = undoStacks.begin(); it != undoStacks.end(); ) {
        if (liveGroups.contains(it.key()))
            ++it;
        else {
            delete it.value(); //releases the steps, and with them the document they refer to.
            it = undoStacks.erase(it);
        }
    }
#else
    Q_UNUSED(liveGroups)
#endif
}

bool QWebPagePrivate::canUndo(const void* group) const
{
#ifndef QT_NO_UNDOSTACK
    QUndoStack* stack = undoStacks.value(group);
    return stack && stack->canUndo();
#else
    Q_UNUSED(group)
    return false;
#endif
}

bool QWebPagePrivate::canRedo(const void* group) const
{
#ifndef QT_NO_UNDOSTACK
    QUndoStack* stack = undoStacks.value(group);
    return stack && stack->canRedo();
#else
    Q_UNUSED(group)
    return false;
#endif
}

void QWebPagePrivate::undo(const void* group)
{
#ifndef QT_NO_UNDOSTACK
    if (QUndoStack* stack = undoStacks.value(group))
        stack->undo();
    updateUndoActions();
#else
    Q_UNUSED(group)
#endif
}

void QWebPagePrivate::redo(const void* group)
{
#ifndef QT_NO_UNDOSTACK
    if (QUndoStack* stack = undoStacks.value(group))
        stack->redo();
    updateUndoActions();
#else
    Q_UNUSED(group)
#endif
}

void QWebPagePrivate::createUndoStep(QSharedPointer<UndoStepQt> step, const void* group)
{
#ifndef QT_NO_UNDOSTACK
    undoStackForGroup(group)->push(new QWebUndoCommand(step));
    updateUndoActions(); //WebCore reports the content change before it registers the step.
#else
    Q_UNUSED(step)
    Q_UNUSED(group)
#endif
}

const char *QWebPagePrivate::editorCommandForKeyEvent(QKeyEvent* event)
{
    QWebPage::WebAction action = editorActionForKeyEvent(event);
    return editorCommandForWebActions(action);
}

QSize QWebPage::viewportSize() const
{
    if (d->mainFrame && d->mainFrame.data()->d->hasView())
        return d->mainFrame.data()->d->frameRect().size();

    return d->m_viewportSize;
}

/*!
    \property QWebPage::viewportSize
    \brief the size of the viewport

    The size affects for example the visibility of scrollbars
    if the document is larger than the viewport.

    By default, for a newly-created Web page, this property contains a size with
    zero width and height.

    \sa QWebFrame::render(), preferredContentsSize
*/
void QWebPage::setViewportSize(const QSize &size) const
{
    d->m_viewportSize = size;

    d->updateWindow();

    QWebFrameAdapter& mainFrame = d->mainFrameAdapter();
    if (!mainFrame.hasView())
        return;

    mainFrame.setViewportSize(size);
}

void QWebPagePrivate::updateWindow()
{
    QWindow* _window = 0;
    if (view && view->window())
        _window = view->window()->windowHandle();

    if (window == _window)
        return;

    if (window)
        QObject::disconnect(window, SIGNAL(screenChanged(QScreen*)), q, SLOT(_q_updateScreen(QScreen*)));
    window = _window;
    if (window) {
        QObject::connect(window, SIGNAL(screenChanged(QScreen*)), q, SLOT(_q_updateScreen(QScreen*)));
        _q_updateScreen(window->screen());
    }
}

void QWebPagePrivate::_q_updateScreen(QScreen* screen)
{
    if (screen && !m_customDevicePixelRatioIsSet)
        setDevicePixelRatio(screen->devicePixelRatio());
}

void QWebPage::setDevicePixelRatio(qreal ratio)
{
    d->setDevicePixelRatio(ratio);
    d->m_customDevicePixelRatioIsSet = true;
}

qreal QWebPage::devicePixelRatio() const
{
    return d->devicePixelRatio();
}

void QWebPage::resetDevicePixelRatio()
{
    d->m_customDevicePixelRatioIsSet = false;
    d->updateWindow();
}

QSize QWebPage::preferredContentsSize() const
{
    QWebFrameAdapter* mainFrame = d->mainFrame ? d->mainFrame->d : 0;
    QSize customSize;
    if (mainFrame && mainFrame->hasView())
        customSize = mainFrame->customLayoutSize();

    return customSize.isNull() ? d->fixedLayoutSize : customSize;
}

/*!
    \property QWebPage::preferredContentsSize
    \since 4.6
    \brief a custom size used for laying out the page contents.

    By default all pages are laid out using the viewport of the page as the base.

    As pages mostly are designed for desktop usage, they often do not layout properly
    on small devices as the contents require a certain view width. For this reason
    it is common to use a different layout size and then scale the contents to fit
    within the actual view.

    If this property is set to a valid size, this size is used for all layout needs
    instead of the size of the viewport.

    Setting an invalid size, makes the page fall back to using the viewport size for layout.

    \sa viewportSize
*/
void QWebPage::setPreferredContentsSize(const QSize& size) const
{
    // FIXME: Rename this method to setCustomLayoutSize

    d->fixedLayoutSize = size;

    QWebFrameAdapter& mainFrame = d->mainFrameAdapter();
    if (!mainFrame.hasView())
        return;

    mainFrame.setCustomLayoutSize(size);
}

/*
    This function is to be called after any (animated) scroll/pan has ended, in the case the application handles the
    scrolling/panning of the web contents. This is commonly used in combination with tiling where is it common for
    the application to pan the actual view, which then resizes itself to the size of the contents.

    \note Calling this function makes WebKit stop trying to calculate the visibleContentRect. To turn that on
    again, call this method with an empty rect.

    \sa QGraphicsWebView::resizesToContents, QWebSettings::TiledBackingStoreEnabled
*/
void QWebPage::setActualVisibleContentRect(const QRect& rect) const
{
    QWebFrameAdapter& mainFrame = d->mainFrameAdapter();
    if (!mainFrame.hasView())
        return;

    mainFrame.setFixedVisibleContentRect(rect);
}

/*!
    \property QWebPage::hasSelection
    \brief whether this page contains selected content or not.

    \sa selectionChanged()
*/
bool QWebPage::hasSelection() const
{
    d->createMainFrame();
    return d->hasSelection();
}

/*!
    \property QWebPage::selectedText
    \brief the text currently selected

    By default, this property contains an empty string.

    \sa selectionChanged(), selectedHtml()
*/
QString QWebPage::selectedText() const
{
    d->createMainFrame();
    return d->selectedText();
}

/*!
    \since 4.8
    \property QWebPage::selectedHtml
    \brief the HTML currently selected

    By default, this property contains an empty string.

    \sa selectionChanged(), selectedText()
*/
QString QWebPage::selectedHtml() const
{
    d->createMainFrame();
    return d->selectedHtml();
}

#ifndef QT_NO_ACTION
/*!
   Returns a QAction for the specified WebAction \a action.

   The action is owned by the QWebPage but you can customize the look by
   changing its properties.

   QWebPage also takes care of implementing the action, so that upon
   triggering the corresponding action is performed on the page.

   \sa triggerAction()
*/
QAction *QWebPage::action(WebAction action) const
{
    if (action == QWebPage::NoWebAction)
        return 0;
    if (d->actions[action])
        return d->actions[action];

    QString text;
    QIcon icon;
    QStyle *style = d->client ? d->client->style() : qApp->style();
    bool checkable = false;
    QWebPageAdapter::MenuAction mappedAction = QWebPageAdapter::NoAction;

    switch (action) {
    // to be fetched from LocalizedStringsQt via the page adapter
    case CopyImageToClipboard:
    case Cut:
    case Copy:
    case Paste:
    case SelectAll:
    case SetTextDirectionDefault:
    case SetTextDirectionLeftToRight:
    case SetTextDirectionRightToLeft:
    case ToggleBold:
    case ToggleItalic:
    case ToggleUnderline:
    case ToggleMediaControls:
    case ToggleMediaLoop:
    case ToggleMediaPlayPause:
    case ToggleMediaMute:
    case ToggleVideoFullscreen:
        mappedAction = adapterMenuActionForWebAction(action);
        break;
    /*case InspectElement:
        mappedAction = QWebPageAdapter::InspectElement;
        break;

        // icon needed as well, map by hand.
    case Back:
        mappedAction = QWebPageAdapter::Back;
        icon = style->standardIcon(QStyle::SP_ArrowBack);
        break;
    case Forward:
        mappedAction = QWebPageAdapter::Forward;
        icon = style->standardIcon(QStyle::SP_ArrowForward);
        break;
    case Stop:
        mappedAction = QWebPageAdapter::Stop;
        icon = style->standardIcon(QStyle::SP_BrowserStop);
        break;
    case Reload:
        mappedAction = QWebPageAdapter::Reload;
        icon = style->standardIcon(QStyle::SP_BrowserReload);
        break;*/

#ifndef QT_NO_UNDOSTACK
    // Ordinary editor-command actions (not QUndoStack's own): they act on, and are enabled
    // according to, the focused frame's undo history.
    case Undo:
        text = tr("Undo");
        break;
    case Redo:
        text = tr("Redo");
        break;
#endif // QT_NO_UNDOSTACK
        // in place l10n
        case MoveToNextChar:
            text = tr("Move the cursor to the next character");
            break;
        case MoveToPreviousChar:
            text = tr("Move the cursor to the previous character");
            break;
        case MoveToNextWord:
            text = tr("Move the cursor to the next word");
            break;
        case MoveToPreviousWord:
            text = tr("Move the cursor to the previous word");
            break;
        case MoveToNextLine:
            text = tr("Move the cursor to the next line");
            break;
        case MoveToPreviousLine:
            text = tr("Move the cursor to the previous line");
            break;
        case MoveToStartOfLine:
            text = tr("Move the cursor to the start of the line");
            break;
        case MoveToEndOfLine:
            text = tr("Move the cursor to the end of the line");
            break;
        case MoveToStartOfBlock:
            text = tr("Move the cursor to the start of the block");
            break;
        case MoveToEndOfBlock:
            text = tr("Move the cursor to the end of the block");
            break;
        case MoveToStartOfDocument:
            text = tr("Move the cursor to the start of the document");
            break;
        case MoveToEndOfDocument:
            text = tr("Move the cursor to the end of the document");
            break;
        case SelectNextChar:
            text = tr("Select to the next character");
            break;
        case SelectPreviousChar:
            text = tr("Select to the previous character");
            break;
        case SelectNextWord:
            text = tr("Select to the next word");
            break;
        case SelectPreviousWord:
            text = tr("Select to the previous word");
            break;
        case SelectNextLine:
            text = tr("Select to the next line");
            break;
        case SelectPreviousLine:
            text = tr("Select to the previous line");
            break;
        case SelectStartOfLine:
            text = tr("Select to the start of the line");
            break;
        case SelectEndOfLine:
            text = tr("Select to the end of the line");
            break;
        case SelectStartOfBlock:
            text = tr("Select to the start of the block");
            break;
        case SelectEndOfBlock:
            text = tr("Select to the end of the block");
            break;
        case SelectStartOfDocument:
            text = tr("Select to the start of the document");
            break;
        case SelectEndOfDocument:
            text = tr("Select to the end of the document");
            break;
        case DeleteStartOfWord:
            text = tr("Delete to the start of the word");
            break;
        case DeleteEndOfWord:
            text = tr("Delete to the end of the word");
            break;

        case InsertParagraphSeparator:
            text = tr("Insert a new paragraph");
            break;
        case InsertLineSeparator:
            text = tr("Insert a new line");
            break;

        case PasteAndMatchStyle:
            text = tr("Paste and Match Style");
            break;
        case RemoveFormat:
            text = tr("Remove formatting");
            break;

        case ToggleStrikethrough:
            text = tr("Strikethrough");
            checkable = true;
            break;
        case ToggleSubscript:
            text = tr("Subscript");
            checkable = true;
            break;
        case ToggleSuperscript:
            text = tr("Superscript");
            checkable = true;
            break;
        case InsertUnorderedList:
            text = tr("Insert Bulleted List");
            checkable = true;
            break;
        case InsertOrderedList:
            text = tr("Insert Numbered List");
            checkable = true;
            break;
        case Indent:
            text = tr("Indent");
            break;
        case Outdent:
            text = tr("Outdent");
            break;
        case AlignCenter:
            text = tr("Center");
            break;
        case AlignJustified:
            text = tr("Justify");
            break;
        case AlignLeft:
            text = tr("Align Left");
            break;
        case AlignRight:
            text = tr("Align Right");
            break;
        case NoWebAction:
            return 0;
        default:
            break;
    }
    if (mappedAction != QWebPageAdapter::NoAction)
        text = d->contextMenuItemTagForAction(mappedAction, &checkable);

    if (text.isEmpty())
        return 0;

    QAction *a = new QAction(d->q);
    a->setText(text);
    a->setData(action);
    a->setCheckable(checkable);
    a->setIcon(icon);

    connect(a, SIGNAL(triggered(bool)),
        this, SLOT(_q_webActionTriggered(bool)));

    d->actions[action] = a;
    d->updateAction(action);
    return a;
}

QAction* QWebPage::customAction(int action) const
{
    auto actionIter = d->customActions.constFind(action);
    if (actionIter != d->customActions.constEnd())
        return *actionIter;

    QAction* a = new QAction(d->q);
    a->setData(action);
    connect(a, SIGNAL(triggered(bool)),
        this, SLOT(_q_customActionTriggered(bool)));

    d->customActions.insert(action, a);
    return a;
}
#endif // QT_NO_ACTION

/*!
    \property QWebPage::modified
    \brief whether the page contains unsubmitted form data, or the contents have been changed.

    By default, this property is false.

    \sa contentsChanged(), contentEditable, undoStack()
*/
bool QWebPage::isModified() const
{
#ifdef QT_NO_UNDOSTACK
    return false;
#else
    for (QUndoStack* stack : d->undoStacks) {
        if (stack->canUndo())
            return true;
    }
    return false;
#endif // QT_NO_UNDOSTACK
}

#ifndef QT_NO_UNDOSTACK
/*!
    Returns a pointer to the undo stack used for editable content.

    \sa modified
*/
QUndoStack *QWebPage::undoStack() const
{
    // Each document has its own undo history; this is the focused frame's.
    return d->undoStackForGroup(d->focusedUndoGroup());
}
#endif // QT_NO_UNDOSTACK

/*! \reimp
*/
bool QWebPage::event(QEvent *ev)
{
    switch (ev->type()) {
    case QEvent::Timer:
        d->timerEvent(static_cast<QTimerEvent*>(ev));
        break;
    case QEvent::MouseMove:
        d->mouseMoveEvent(static_cast<QMouseEvent*>(ev));
        break;
    case QEvent::MouseButtonPress:
        d->mousePressEvent(static_cast<QMouseEvent*>(ev));
        break;
    case QEvent::MouseButtonDblClick:
        d->mouseDoubleClickEvent(static_cast<QMouseEvent*>(ev));
        break;
    case QEvent::MouseButtonRelease:
        d->mouseReleaseEvent(static_cast<QMouseEvent*>(ev));
        break;
#if !defined(QT_NO_GRAPHICSVIEW)
    case QEvent::GraphicsSceneMouseMove: {
        QGraphicsSceneMouseEvent *gsEv = static_cast<QGraphicsSceneMouseEvent*>(ev);
        QMouseEvent dummyEvent(QEvent::MouseMove, gsEv->pos(), gsEv->screenPos(), gsEv->button(), gsEv->buttons(), gsEv->modifiers());
        d->mouseMoveEvent(&dummyEvent);
        ev->setAccepted(dummyEvent.isAccepted());
        break;
    }
    case QEvent::GraphicsSceneMouseRelease: {
        QGraphicsSceneMouseEvent *gsEv = static_cast<QGraphicsSceneMouseEvent*>(ev);
        QMouseEvent dummyEvent(QEvent::MouseButtonRelease, gsEv->pos(), gsEv->screenPos(), gsEv->button(), gsEv->buttons(), gsEv->modifiers());
        d->adjustPointForClicking(&dummyEvent);
        d->mouseReleaseEvent(&dummyEvent);
        ev->setAccepted(dummyEvent.isAccepted());
        break;
    }
    case QEvent::GraphicsSceneMousePress: {
        QGraphicsSceneMouseEvent *gsEv = static_cast<QGraphicsSceneMouseEvent*>(ev);
        QMouseEvent dummyEvent(QEvent::MouseButtonPress, gsEv->pos(), gsEv->screenPos(), gsEv->button(), gsEv->buttons(), gsEv->modifiers());
        d->adjustPointForClicking(&dummyEvent);
        d->mousePressEvent(&dummyEvent);
        ev->setAccepted(dummyEvent.isAccepted());
        break;
    }
    case QEvent::GraphicsSceneMouseDoubleClick: {
        QGraphicsSceneMouseEvent *gsEv = static_cast<QGraphicsSceneMouseEvent*>(ev);
        QMouseEvent dummyEvent(QEvent::MouseButtonDblClick, gsEv->pos(), gsEv->screenPos(), gsEv->button(), gsEv->buttons(), gsEv->modifiers());
        d->adjustPointForClicking(&dummyEvent);
        d->mouseDoubleClickEvent(&dummyEvent);
        ev->setAccepted(dummyEvent.isAccepted());
        break;
    }
#endif
#ifndef QT_NO_CONTEXTMENU
    case QEvent::ContextMenu:
        d->contextMenuEvent(static_cast<QContextMenuEvent*>(ev)->globalPos());
        break;
#if !defined(QT_NO_GRAPHICSVIEW)
    case QEvent::GraphicsSceneContextMenu:
        d->contextMenuEvent(static_cast<QGraphicsSceneContextMenuEvent*>(ev)->screenPos());
        break;
#endif
#endif
#ifndef QT_NO_WHEELEVENT
    case QEvent::Wheel: {
        QWheelEvent* wheelEv = static_cast<QWheelEvent*>(ev);
        d->wheelEvent(ev, wheelEv->position().toPoint(), wheelEv->globalPosition().toPoint(), wheelEv->angleDelta(), wheelEv->modifiers(), QApplication::wheelScrollLines());
        break;
    }
#if !defined(QT_NO_GRAPHICSVIEW)
    case QEvent::GraphicsSceneWheel: {
        QGraphicsSceneWheelEvent *gsEv = static_cast<QGraphicsSceneWheelEvent*>(ev);
        const QPoint angleDelta = gsEv->orientation() == Qt::Horizontal ? QPoint(gsEv->delta(), 0) : QPoint(0, gsEv->delta());
        d->wheelEvent(ev, gsEv->pos().toPoint(), gsEv->screenPos(), angleDelta, gsEv->modifiers(), QApplication::wheelScrollLines());
        break;
    }
#endif
#endif
    case QEvent::KeyPress:
        d->keyPressEvent(static_cast<QKeyEvent*>(ev));
        break;
    case QEvent::KeyRelease:
        d->keyReleaseEvent(static_cast<QKeyEvent*>(ev));
        break;
    case QEvent::FocusIn:
        d->focusInEvent(static_cast<QFocusEvent*>(ev));
        break;
    case QEvent::FocusOut:
        d->focusOutEvent(static_cast<QFocusEvent*>(ev));
        break;
#if ENABLE(DRAG_SUPPORT)
    case QEvent::DragEnter:
        d->dragEnterEvent(static_cast<QDragEnterEvent*>(ev));
        break;
    case QEvent::DragLeave:
        d->dragLeaveEvent();
        ev->accept();
        break;
    case QEvent::DragMove:
        d->dragMoveEvent(static_cast<QDragMoveEvent*>(ev));
        break;
    case QEvent::Drop:
        d->dropEvent(static_cast<QDropEvent*>(ev));
        break;
#if !defined(QT_NO_GRAPHICSVIEW)
    case QEvent::GraphicsSceneDragEnter:
        d->dragEnterEvent(static_cast<QGraphicsSceneDragDropEvent*>(ev));
        break;
    case QEvent::GraphicsSceneDragMove:
        d->dragMoveEvent(static_cast<QGraphicsSceneDragDropEvent*>(ev));
        break;
    case QEvent::GraphicsSceneDragLeave:
        d->dragLeaveEvent();
        ev->accept();
        break;
    case QEvent::GraphicsSceneDrop:
        d->dropEvent(static_cast<QGraphicsSceneDragDropEvent*>(ev));
        break;
#endif

#endif
    case QEvent::InputMethod:
        d->inputMethodEvent(static_cast<QInputMethodEvent*>(ev));
        break;
    case QEvent::ShortcutOverride:
        d->shortcutOverrideEvent(static_cast<QKeyEvent*>(ev));
        break;
    case QEvent::Leave:
        d->leaveEvent(ev);
        break;
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        // Return whether the default action was cancelled in the JS event handler
        return d->touchEvent(static_cast<QTouchEvent*>(ev));
#ifndef QT_NO_GESTURES
    case QEvent::Gesture:
        d->gestureEvent(static_cast<QGestureEvent*>(ev));
        break;
#endif
#ifndef QT_NO_PROPERTIES
    case QEvent::DynamicPropertyChange:
        d->dynamicPropertyChangeEvent(this, static_cast<QDynamicPropertyChangeEvent*>(ev));
        break;
#endif
    case QEvent::Show:
        d->setPluginsVisible(true);
        break;
    case QEvent::Hide:
        d->setPluginsVisible(false);
        break;
    default:
        return QObject::event(ev);
    }

    return true;
}

/*!
    Similar to QWidget::focusNextPrevChild() it focuses the next focusable web element
    if \a next is true; otherwise the previous element is focused.

    Returns true if it can find a new focusable element, or false if it can't.
*/
bool QWebPage::focusNextPrevChild(bool next)
{
    QKeyEvent ev(QEvent::KeyPress, Qt::Key_Tab, Qt::KeyboardModifiers(next ? Qt::NoModifier : Qt::ShiftModifier));
    d->keyPressEvent(&ev);
    return d->hasFocusedNode();
}

/*!
    \property QWebPage::contentEditable
    \brief whether the content in this QWebPage is editable or not
    \since 4.5

    If this property is enabled the contents of the page can be edited by the user through a visible
    cursor. If disabled (the default) only HTML elements in the web page with their
    \c{contenteditable} attribute set are editable.

    \sa modified, contentsChanged(), WebAction
*/
void QWebPage::setContentEditable(bool editable)
{
    if (isContentEditable() != editable) {
        d->setContentEditable(editable);
        d->updateEditorActions();
    }
}

bool QWebPage::isContentEditable() const
{
    return d->isContentEditable();
}

#ifndef QT_NO_CONTEXTMENU

bool QWebPagePrivate::handleScrollbarContextMenuEvent(QContextMenuEvent* event, bool horizontal, QWebPageAdapter::ScrollDirection* direction, QWebPageAdapter::ScrollGranularity* granularity)
{
    if (!QApplication::style()->styleHint(QStyle::SH_ScrollBar_ContextMenu))
        return false;

    QMenu menu;
    QAction* actScrollHere = menu.addAction(QCoreApplication::translate("QWebPage", "Scroll here"));
    menu.addSeparator();

    QAction* actScrollTop = menu.addAction(horizontal ? QCoreApplication::translate("QWebPage", "Left edge") : QCoreApplication::translate("QWebPage", "Top"));
    QAction* actScrollBottom = menu.addAction(horizontal ? QCoreApplication::translate("QWebPage", "Right edge") : QCoreApplication::translate("QWebPage", "Bottom"));
    menu.addSeparator();

    QAction* actPageUp = menu.addAction(horizontal ? QCoreApplication::translate("QWebPage", "Page left") : QCoreApplication::translate("QWebPage", "Page up"));
    QAction* actPageDown = menu.addAction(horizontal ? QCoreApplication::translate("QWebPage", "Page right") : QCoreApplication::translate("QWebPage", "Page down"));
    menu.addSeparator();

    QAction* actScrollUp = menu.addAction(horizontal ? QCoreApplication::translate("QWebPage", "Scroll left") : QCoreApplication::translate("QWebPage", "Scroll up"));
    QAction* actScrollDown = menu.addAction(horizontal ? QCoreApplication::translate("QWebPage", "Scroll right") : QCoreApplication::translate("QWebPage", "Scroll down"));

    QAction* actionSelected = menu.exec(event->globalPos());

    if (actionSelected == actScrollHere)
        return true;
    if (actionSelected == actScrollTop) {
        *direction = horizontal ? ScrollLeft : ScrollUp;
        *granularity = ScrollByDocument;
    } else if (actionSelected == actScrollBottom) {
        *direction =horizontal ? ScrollRight : ScrollDown;
        *granularity = ScrollByDocument;
    } else if (actionSelected == actPageUp) {
        *direction = horizontal ? ScrollLeft : ScrollUp;
        *granularity = ScrollByPage;
    } else if (actionSelected == actPageDown) {
        *direction =horizontal ? ScrollRight : ScrollDown;
        *granularity = ScrollByPage;
    } else if (actionSelected == actScrollUp) {
        *direction = horizontal ? ScrollLeft : ScrollUp;
        *granularity = ScrollByLine;
    } else if (actionSelected == actScrollDown) {
        *direction =horizontal ? ScrollRight : ScrollDown;
        *granularity = ScrollByLine;
    }
    return true;
}

/*!
    Filters the context menu event, \a event, through handlers for scrollbars and
    custom event handlers in the web page. Returns true if the event was handled;
    otherwise false.

    A web page may swallow a context menu event through a custom event handler, allowing for context
    menus to be implemented in HTML/JavaScript. This is used by \l{http://maps.google.com/}{Google
    Maps}, for example.
*/
bool QWebPage::swallowContextMenuEvent(QContextMenuEvent *event)
{
    QWebFrame* webFrame = frameAt(event->pos());
    return d->swallowContextMenuEvent(event, webFrame ? webFrame->d : 0);
}
#endif // QT_NO_CONTEXTMENU

/*!
    Updates the page's actions depending on the position \a pos. For example if \a pos is over an image
    element the CopyImageToClipboard action is enabled.
*/
void QWebPage::updatePositionDependentActions(const QPoint &pos)
{
#ifndef QT_NO_ACTION
    // First we disable all actions, but keep track of which ones were originally enabled.
    QBitArray originallyEnabledWebActions(QWebPage::WebActionCount);
    for (int i = QWebPageAdapter::NoAction + 1; i < QWebPageAdapter::ActionCount; ++i) {
        QWebPage::WebAction action = webActionForAdapterMenuAction(QWebPageAdapter::MenuAction(i));
        if (QAction *a = this->action(action)) {
            originallyEnabledWebActions.setBit(action, a->isEnabled());
            a->setEnabled(false);
        }
    }
#endif // QT_NO_ACTION

    QBitArray visitedWebActions(QWebPage::WebActionCount);
    d->createMainFrame();
    // Then we let updatePositionDependantMenuActions() enable the actions that are put into the menu
    QWebHitTestResultPrivate* result = d->updatePositionDependentMenuActions(pos, &visitedWebActions);
    if (!result)
        d->hitTestResult = QWebHitTestResult();
    else
        d->hitTestResult = QWebHitTestResult(result);

#ifndef QT_NO_ACTION
    // Finally, we restore the original enablement for the actions that were not put into the menu.
    originallyEnabledWebActions &= ~visitedWebActions; // Mask out visited actions (they're part of the menu)
    for (int i = 0; i < QWebPage::WebActionCount; ++i) {
        if (originallyEnabledWebActions.at(i)) {
            if (QAction *a = this->action(QWebPage::WebAction(i)))
                a->setEnabled(true);
        }
    }
#endif // QT_NO_ACTION

    // This whole process ensures that any actions put into to the context menu has the right
    // enablement, while also keeping the correct enablement for actions that were left out of
    // the menu.

}


/*!
 * \internal
 */
QWebPageAdapter *QWebPage::handle() const
{
    return d;
}

/*!
    Finds the specified string, \a subString, in the page, using the given \a options.

    If the HighlightAllOccurrences flag is passed, the function will highlight all occurrences
    that exist in the page. All subsequent calls will extend the highlight, rather than
    replace it, with occurrences of the new string.

    If the HighlightAllOccurrences flag is not passed, the function will select an occurrence
    and all subsequent calls will replace the current occurrence with the next one.

    To clear the selection, just pass an empty string.

    Returns true if \a subString was found; otherwise returns false.
*/
bool QWebPage::findText(const QString &subString, FindFlags options)
{
    return d->findText(subString, static_cast<QWebPageAdapter::FindFlag>(
        static_cast<FindFlags::Int>(options)));
}

/*!
    Returns a pointer to the page's settings object.

    \sa QWebSettings::globalSettings()
*/
QWebSettings *QWebPage::settings() const
{
    return d->settings;
}

/*!
    Sets the QWebPluginFactory \a factory responsible for creating plugins embedded into this
    QWebPage.

    Note: The plugin factory is only used if the QWebSettings::PluginsEnabled attribute is enabled.

    \sa pluginFactory()
*/
void QWebPage::setPluginFactory(QWebPluginFactory *factory)
{
    d->pluginFactory = factory;
}

/*!
    Returns the QWebPluginFactory that is responsible for creating plugins embedded into
    this QWebPage. If no plugin factory is installed a null pointer is returned.

    \sa setPluginFactory()
*/
QWebPluginFactory *QWebPage::pluginFactory() const
{
    return d->pluginFactory;
}


/*!
    Returns the total number of bytes that were received from the network to render the current page,
    including extra content such as embedded images.

    \sa bytesReceived()
*/
quint64 QWebPage::totalBytes() const
{
    return d->m_totalBytes;
}


/*!
    Returns the number of bytes that were received from the network to render the current page.

    \sa totalBytes(), loadProgress()
*/
quint64 QWebPage::bytesReceived() const
{
    return d->m_bytesReceived;
}


/*!
    \property QWebPage::visibilityState
    \brief the page's visibility state

    This property should be changed by Qt applications who want to notify the JavaScript application
    that the visibility state has changed (e.g. by reimplementing QWidget::setVisible).
    The visibility state will be updated with the \a state parameter value only if it's different from the previous set.
    Then, HTML DOM Document Object attributes 'hidden' and 'visibilityState'
    will be updated to the correct value and a 'visiblitychange' event will be fired.
    More information about this HTML5 API can be found at \l{http://www.w3.org/TR/page-visibility/}{W3C Recommendation: Page Visibility}.

    By default, this property is set to VisibilityStateVisible.
*/
void QWebPage::setVisibilityState(VisibilityState state)
{
    d->setVisibilityState(static_cast<QWebPageAdapter::VisibilityState>(state));
}

QWebPage::VisibilityState QWebPage::visibilityState() const
{
    return static_cast<VisibilityState>(d->visibilityState());
}


bool QWebPage::recentlyAudible() const
{
    return d->isPlayingAudio();
}


/*!
    \fn void QWebPage::loadStarted()

    This signal is emitted when a page starts loading content.

    \sa loadFinished()
*/

/*!
    \fn void QWebPage::loadProgress(int progress)

    This signal is emitted when the global progress status changes.
    The current value is provided by \a progress and scales from 0 to 100,
    which is the default range of QProgressBar.
    It accumulates changes from all the child frames.

    \sa bytesReceived()
*/

/*!
    \fn void QWebPage::loadFinished(bool ok)

    This signal is emitted when the page finishes loading content. This signal
    is independant of script execution or page rendering.
    \a ok will indicate whether the load was successful or any error occurred.

    \sa loadStarted()
*/

/*!
    \fn void QWebPage::linkHovered(const QString &link, const QString &title, const QString &textContent)

    This signal is emitted when the mouse hovers over a link.

    \a link contains the link url.
    \a title is the link element's title, if it is specified in the markup.
    \a textContent provides text within the link element, e.g., text inside an HTML anchor tag.

    When the mouse leaves the link element the signal is emitted with empty parameters.
*/

/*!
    \fn void QWebPage::statusBarMessage(const QString& text)

    This signal is emitted when the statusbar \a text is changed by the page.
*/

/*!
    \fn void QWebPage::frameCreated(QWebFrame *frame)

    This signal is emitted whenever the page creates a new \a frame.

    \sa currentFrame()
*/

/*!
    \fn void QWebPage::selectionChanged()

    This signal is emitted whenever the selection changes, either interactively
    or programmatically (e.g. by calling triggerAction() with a selection action).

    \sa selectedText()
*/

/*!
    \fn void QWebPage::contentsChanged()
    \since 4.5

    This signal is emitted whenever the text in form elements changes
    as well as other editable content.

    \sa contentEditable, modified, QWebFrame::toHtml(), QWebFrame::toPlainText()
*/

/*!
    \fn void QWebPage::geometryChangeRequested(const QRect& geom)

    This signal is emitted whenever the document wants to change the position and size of the
    page to \a geom. This can happen for example through JavaScript.
*/

/*!
    \fn void QWebPage::repaintRequested(const QRect& dirtyRect)

    This signal is emitted whenever this QWebPage should be updated. It's useful
    when rendering a QWebPage without a QWebView or QGraphicsWebView.
    \a dirtyRect contains the area that needs to be updated. To paint the QWebPage get
    the mainFrame() and call the render(QPainter*, const QRegion&) method with the
    \a dirtyRect as the second parameter.

    \sa mainFrame()
    \sa view()
*/

/*!
    \fn void QWebPage::scrollRequested(int dx, int dy, const QRect& rectToScroll)

    This signal is emitted whenever the content given by \a rectToScroll needs
    to be scrolled \a dx and \a dy downwards.
*/

/*!
    \fn void QWebPage::microFocusChanged()

    This signal is emitted when for example the position of the cursor in an editable form
    element changes. It is used to inform input methods about the new on-screen position where
    the user is able to enter text. This signal is usually connected to the
    QWidget::updateMicroFocus() slot.
*/

/*!
    \fn void QWebPage::toolBarVisibilityChangeRequested(bool visible)

    This signal is emitted whenever the visibility of the toolbar in a web browser
    window that hosts QWebPage should be changed to \a visible.
*/

/*!
    \fn void QWebPage::statusBarVisibilityChangeRequested(bool visible)

    This signal is emitted whenever the visibility of the statusbar in a web browser
    window that hosts QWebPage should be changed to \a visible.
*/

/*!
    \fn void QWebPage::menuBarVisibilityChangeRequested(bool visible)

    This signal is emitted whenever the visibility of the menubar in a web browser
    window that hosts QWebPage should be changed to \a visible.
*/

/*!
  \since 4.5
  \fn void QWebPage::restoreFrameStateRequested(QWebFrame* frame);

  This signal is emitted when the load of \a frame is finished and the application may now update its state accordingly.
*/

#include "moc_qwebpage.cpp"
