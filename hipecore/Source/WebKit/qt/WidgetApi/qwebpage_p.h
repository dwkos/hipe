/*
    Copyright (C) 2008, 2009 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2008 Holger Hans Peter Freyther
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

#ifndef qwebpage_p_h
#define qwebpage_p_h

#include "QWebPageAdapter.h"

#include "qwebframe.h"
#include "qwebpage.h"

#include <QHash>
#include <QPointer>
#include <qevent.h>
#include <qgesture.h>
#include <qgraphicssceneevent.h>
#include <qgraphicswidget.h>
#include <qmetaobject.h>


namespace WebCore {
class ContextMenuClientQt;
class ContextMenuItem;
class ContextMenu;
class Document;
class EditorClientQt;
class Element;
class IntRect;
class Node;
class NodeList;
class Frame;
}

QT_BEGIN_NAMESPACE
class QBitArray;
class QMenu;
class QScreen;
class QUndoStack;
class QWindow;
QT_END_NAMESPACE

class QtPluginWidgetAdapter;
class QWebFrameAdapter;
class UndoStepQt;

class QWebPagePrivate : public QWebPageAdapter {
public:
    QWebPagePrivate(QWebPage*);
    ~QWebPagePrivate();

    static WebCore::Page* core(const QWebPage*);

    // Adapter implementation
    void show() override;
    void setFocus() override;
    void unfocus() override;
    void setWindowRect(const QRect &) override;
    QSize viewportSize() const override;
    QWebPageAdapter* createWindow(bool /*dialog*/) override;
    QObject* handle() override { return q; }
    void consoleMessageReceived(MessageSource source, MessageLevel level, const QString& message, int lineNumber, const QString& sourceID) override;
    void setToolTip(const QString&) override;
    QWebFrameAdapter& mainFrameAdapter() override;
    QColor colorSelectionRequested(const QColor& selectedColor) override;
    std::unique_ptr<QWebSelectMethod> createSelectPopup() override;
    QRect viewRectRelativeToWindow() override;

    void respondToChangedContents() override;
    void respondToChangedSelection() override;
    void focusedFrameChanged() override;
    void microFocusChanged() override;
    void triggerCopyAction() override;
    void triggerActionForKeyEvent(QKeyEvent*) override;
    void removeUndoStacksExcept(const QSet<const void*>& liveGroups) override;
    bool canUndo(const void* group) const override;
    bool canRedo(const void* group) const override;
    void undo(const void* group) override;
    void redo(const void* group) override;
    void createUndoStep(QSharedPointer<UndoStepQt>, const void* group) override;
    const char* editorCommandForKeyEvent(QKeyEvent*) override;

    void clearCustomActions() override;

    void emitRestoreFrameStateRequested(QWebFrameAdapter*) override;
    void emitFrameCreated(QWebFrameAdapter*) override;
    QtPluginWidgetAdapter* createPlugin(const QString &, const QUrl &, const QStringList &, const QStringList &) override;
    QtPluginWidgetAdapter* adapterForWidget(QObject *) const override;
    bool requestSoftwareInputPanel() const override;
    bool handleScrollbarContextMenuEvent(QContextMenuEvent*, bool, ScrollDirection*, ScrollGranularity*) override;
    void recentlyAudibleChanged(bool) override;
    void focusedElementChanged(const QWebElement&) override;


    void createMainFrame();

    void _q_webActionTriggered(bool checked);
    void _q_customActionTriggered(bool checked);
    void updateAction(QWebPage::WebAction);
    void updateEditorActions();
    void updateUndoActions();

    void timerEvent(QTimerEvent*);

#ifndef QT_NO_CONTEXTMENU
    void contextMenuEvent(const QPoint& globalPos);
    
    //hipecore context menu handling
    QWebPage::ContextMenuCallback contextMenuCallback;
    void* contextMenuUserPtr;

#endif
    void keyPressEvent(QKeyEvent*);
    void keyReleaseEvent(QKeyEvent*);

    template<class T> void dragEnterEvent(T*);
    template<class T> void dragMoveEvent(T*);
    template<class T> void dropEvent(T*);

    void shortcutOverrideEvent(QKeyEvent*);
    void leaveEvent(QEvent*);

    bool gestureEvent(QGestureEvent*);

    void updateWindow();
    void _q_updateScreen(QScreen*);

#ifndef QT_NO_SHORTCUT
    static QWebPage::WebAction editorActionForKeyEvent(QKeyEvent*);
#endif
    static const char* editorCommandForWebActions(QWebPage::WebAction);

    QWebPage *q;
    QPointer<QWebFrame> mainFrame;

#ifndef QT_NO_UNDOSTACK
    QUndoStack* undoStackForGroup(const void* group) const; //created on demand.
    mutable QHash<const void*, QUndoStack*> undoStacks; //one undo history per document (undo group).
#endif

    QPointer<QWidget> view;

    QSize m_viewportSize;
    QSize fixedLayoutSize;

    QWebHitTestResult hitTestResult;
    QPalette palette;
    bool useFixedLayout;

    QAction *actions[QWebPage::WebActionCount];
    QHash<int, QAction*> customActions;

    QPointer <QWindow> window;
    Qt::DropAction m_lastDropAction;

    bool m_customDevicePixelRatioIsSet { false };
};

#endif
