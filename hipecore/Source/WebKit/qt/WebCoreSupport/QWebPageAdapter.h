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
#ifndef QWebPageAdapter_h
#define QWebPageAdapter_h

#include "QWebPageClient.h"
#include "qwebelement.h"

#include <qbasictimer.h>
#include <qevent.h>
#include <qrect.h>
#include <qscopedpointer.h>
#include <qhash.h>
#include <qset.h>
#include <qsharedpointer.h>
#include <qstring.h>
#include <qurl.h>
#include <wtf/Forward.h>

QT_BEGIN_NAMESPACE
class QBitArray;
class QKeyEvent;
class QMimeData;
class QMouseEvent;
class QWheelEvent;
class QInputMethodEvent;
QT_END_NAMESPACE

namespace WebCore {
class ChromeClientQt;
class Frame;
class Page;
class UndoStep;
}

class QtPluginWidgetAdapter;
class QWebFrameAdapter;
class QWebHitTestResultPrivate;
class QWebPageClient;
class QWebPluginFactory;
class QWebSecurityOrigin;
class QWebSelectMethod;
class QWebSettings;
class UndoStepQt;

class QWEBKIT_EXPORT QWebPageAdapter {
public:

#define FOR_EACH_MAPPED_MENU_ACTION(F, SEPARATOR) \
    F(CopyImageToClipboard, WebCore::ContextMenuItemTagCopyImageToClipboard) SEPARATOR \
    F(Copy, WebCore::ContextMenuItemTagCopy) SEPARATOR \
    F(Cut, WebCore::ContextMenuItemTagCut) SEPARATOR \
    F(Paste, WebCore::ContextMenuItemTagPaste) SEPARATOR \
    F(SetTextDirectionDefault, WebCore::ContextMenuItemTagDefaultDirection) SEPARATOR \
    F(SetTextDirectionLeftToRight, WebCore::ContextMenuItemTagLeftToRight) SEPARATOR \
    F(SetTextDirectionRightToLeft, WebCore::ContextMenuItemTagRightToLeft) SEPARATOR \
    F(ToggleBold, WebCore::ContextMenuItemTagBold) SEPARATOR \
    F(ToggleItalic, WebCore::ContextMenuItemTagItalic) SEPARATOR \
    F(ToggleUnderline, WebCore::ContextMenuItemTagUnderline) SEPARATOR \
    F(SelectAll, WebCore::ContextMenuItemTagSelectAll) SEPARATOR \
    F(ToggleMediaControls, WebCore::ContextMenuItemTagToggleMediaControls) SEPARATOR \
    F(ToggleMediaLoop, WebCore::ContextMenuItemTagToggleMediaLoop) SEPARATOR \
    F(ToggleMediaPlayPause, WebCore::ContextMenuItemTagMediaPlayPause) SEPARATOR \
    F(ToggleMediaMute, WebCore::ContextMenuItemTagMediaMute) SEPARATOR \
    F(ToggleVideoFullscreen, WebCore::ContextMenuItemTagToggleVideoFullscreen)
#define COMMA_SEPARATOR ,
#define SEMICOLON_SEPARATOR ;
#define DEFINE_ACTION(Name, Value) \
    Name

    enum MenuAction {
        NoAction = - 1,
        FOR_EACH_MAPPED_MENU_ACTION(DEFINE_ACTION, COMMA_SEPARATOR)
        , ActionCount
    };

    // Duplicated from qwebpage.h
    enum FindFlag {
        FindBackward = 1,
        FindCaseSensitively = 2,
        FindWrapsAroundDocument = 4,
        HighlightAllOccurrences = 8,
        FindAtWordBeginningsOnly = 16,
        TreatMedialCapitalAsWordBeginning = 32,
        FindBeginsInSelection = 64,
        FindAtWordEndingsOnly = 128
    };

    // valid values matching those from ScrollTypes.h
    enum ScrollDirection {
        InvalidScrollDirection = -1,
        ScrollUp,
        ScrollDown,
        ScrollLeft,
        ScrollRight
    };
    // same here
    enum ScrollGranularity {
        InvalidScrollGranularity = -1,
        ScrollByLine,
        ScrollByPage,
        ScrollByDocument
    };

    // Must match with values of QWebPage::VisibilityState enum.
    enum VisibilityState {
        VisibilityStateVisible,
        VisibilityStateHidden,
        VisibilityStatePrerender,
        VisibilityStateUnloaded
    };

    // Must match with values of QWebPage::MessageSource enum.
    enum MessageSource {
        XmlMessageSource,
        JSMessageSource,
        NetworkMessageSource,
        ConsoleAPIMessageSource,
        StorageMessageSource,
        AppCacheMessageSource,
        RenderingMessageSource,
        CSSMessageSource,
        SecurityMessageSource,
        ContentBlockerMessageSource,
        OtherMessageSource,
    };

    // Must match with values of QWebPage::MessageLevel enum.
    enum MessageLevel {
        LogMessageLevel = 1,
        WarningMessageLevel = 2,
        ErrorMessageLevel = 3,
        DebugMessageLevel = 4,
        InfoMessageLevel = 5,
    };

    QWebPageAdapter();
    virtual ~QWebPageAdapter();

    // Called manually from ~QWebPage destructor to ensure that
    // the QWebPageAdapter and the QWebPagePrivate are intact when
    // various destruction callbacks from WebCore::Page::~Page() hit us.
    void deletePage();
    // For similar reasons, we don't want to create the WebCore Page before
    // we properly initialized the style factory callbacks.
    void initializeWebCorePage();

    virtual void show() = 0;
    virtual void setFocus() = 0;
    virtual void unfocus() = 0;
    virtual void setWindowRect(const QRect&) = 0;
    virtual QSize viewportSize() const = 0;
    virtual QWebPageAdapter* createWindow(bool /*dialog*/) = 0;
    virtual QObject* handle() = 0;
    virtual void consoleMessageReceived(MessageSource, MessageLevel, const QString& message, int lineNumber, const QString& sourceID) = 0;
    virtual void javaScriptAlert(QWebFrameAdapter*, const QString& msg) = 0;
    virtual bool javaScriptConfirm(QWebFrameAdapter*, const QString& msg) = 0;
    virtual bool javaScriptPrompt(QWebFrameAdapter*, const QString& msg, const QString& defaultValue, QString* result) = 0;
    virtual bool shouldInterruptJavaScript() = 0;
    virtual void setToolTip(const QString&) = 0;
    virtual QColor colorSelectionRequested(const QColor& selectedColor) = 0;
    virtual std::unique_ptr<QWebSelectMethod> createSelectPopup() = 0;
    virtual QRect viewRectRelativeToWindow() = 0;


    virtual void respondToChangedContents() = 0;
    virtual void respondToChangedSelection() = 0;
    virtual void focusedFrameChanged() = 0; //the edit actions' state follows the focused frame
    virtual void microFocusChanged() = 0;
    virtual void triggerCopyAction() = 0;
    virtual void triggerActionForKeyEvent(QKeyEvent*) = 0;
    // Undo history is kept per undo group: the document the edits were made in (see
    // undoGroupForStep()). Every app in a Hipe window is a frame of the same page, so one
    // page-wide history would let an undo in one app revert the latest edit in another.
    virtual void removeUndoStacksExcept(const QSet<const void*>& liveGroups) = 0;
    virtual bool canUndo(const void* group) const = 0;
    virtual bool canRedo(const void* group) const = 0;
    virtual void undo(const void* group) = 0;
    virtual void redo(const void* group) = 0;
    virtual const char* editorCommandForKeyEvent(QKeyEvent*) = 0;
    virtual void createUndoStep(QSharedPointer<UndoStepQt>, const void* group) = 0;

    virtual void clearCustomActions() = 0;

    virtual QWebFrameAdapter& mainFrameAdapter() = 0;

    virtual void emitRestoreFrameStateRequested(QWebFrameAdapter *) = 0;
    virtual void emitFrameCreated(QWebFrameAdapter*) = 0;
    virtual QtPluginWidgetAdapter* createPlugin(const QString&, const QUrl&, const QStringList&, const QStringList&) = 0;
    virtual QtPluginWidgetAdapter* adapterForWidget(QObject*) const = 0;
    virtual bool requestSoftwareInputPanel() const = 0;
    struct MenuItemDescription {
        MenuItemDescription()
            : type(NoType)
            , action(NoAction)
            , traits(None)
        { }
        enum Type {
            NoType,
            Action,
            Separator,
            SubMenu
        } type;
        int action;
        enum Trait {
            None = 0,
            Enabled = 1,
            Checkable = 2,
            Checked = 4
        };
        Q_DECLARE_FLAGS(Traits, Trait);
        Traits traits;
        QList<MenuItemDescription> subMenu;
        QString title;
    };
    virtual bool handleScrollbarContextMenuEvent(QContextMenuEvent*, bool, ScrollDirection*, ScrollGranularity*) = 0;

    virtual void recentlyAudibleChanged(bool) = 0;
    virtual void focusedElementChanged(const QWebElement&) = 0;

    void setVisibilityState(VisibilityState);
    VisibilityState visibilityState() const;

    void setPluginsVisible(bool);

    static QWebPageAdapter* kit(WebCore::Page*);
    void registerUndoStep(WTF::PassRefPtr<WebCore::UndoStep>, const void* group);
    // The frame whose document a step edited (its undo group), or null if unknown.
    WebCore::Frame* frameForUndoStep(WebCore::UndoStep*) const;
    // The undo group used by undo/redo: the focused frame's document.
    const void* focusedUndoGroup() const;
    // Drops the undo history of documents no longer shown in any frame of the page.
    void pruneUndoStacks();

    // Records that frame now has focus: for it and each of its ancestors, frame is where focus last was
    // within that frame's subtree. See lastFocusedFrameWithin().
    void noteFocusedFrame(WebCore::Frame*);
    // The frame inside frame's subtree (possibly frame itself) that last had focus, or null if unknown.
    // Focusing an <iframe> uses this to put focus back where it was inside it, e.g. in a nested frame.
    WebCore::Frame* lastFocusedFrameWithin(WebCore::Frame*) const;

    bool hasSelection() const;
    QString selectedText() const;
    QString selectedHtml() const;

    bool isContentEditable() const;
    void setContentEditable(bool);

    bool findText(const QString& subString, FindFlag options);

    void adjustPointForClicking(QMouseEvent*);

    void mouseMoveEvent(QMouseEvent*);
    void mousePressEvent(QMouseEvent*);
    void mouseDoubleClickEvent(QMouseEvent*);
    void mouseTripleClickEvent(QMouseEvent*);
    void mouseReleaseEvent(QMouseEvent*);
    void handleSoftwareInputPanel(Qt::MouseButton, const QPoint&);
#ifndef QT_NO_WHEELEVENT
    // Marks the event accepted or ignored according to whether the page handled it.
    void wheelEvent(QEvent*, const QPoint& position, const QPoint& globalPosition, const QPoint& angleDelta, Qt::KeyboardModifiers, int wheelScrollLines);
#endif
#if ENABLE(DRAG_SUPPORT)
    Qt::DropAction dragEntered(const QMimeData*, const QPoint&, Qt::DropActions);
    void dragLeaveEvent();
    Qt::DropAction dragUpdated(const QMimeData*, const QPoint&, Qt::DropActions);
    bool performDrag(const QMimeData*, const QPoint&, Qt::DropActions);
#endif
    void inputMethodEvent(QInputMethodEvent*);
    void insertText(const QString&);
    QVariant inputMethodQuery(Qt::InputMethodQuery property) const;
    void dynamicPropertyChangeEvent(QObject*, QDynamicPropertyChangeEvent*);
    bool handleKeyEvent(QKeyEvent*);
    bool handleScrolling(QKeyEvent*);
    void focusInEvent(QFocusEvent*);
    void focusOutEvent(QFocusEvent*);
    bool handleShortcutOverrideEvent(QKeyEvent*);
    // Returns whether the default action was cancelled in the JS event handler
    bool touchEvent(QTouchEvent*);
    bool swallowContextMenuEvent(QContextMenuEvent *, QWebFrameAdapter*);

    QWebHitTestResultPrivate* updatePositionDependentMenuActions(const QPoint&, QBitArray*);
    void updateActionInternal(MenuAction, const char* commandName, bool* enabled, bool* checked);
    void triggerAction(MenuAction, QWebHitTestResultPrivate*, const char* commandName, bool endToEndReload);
    void triggerCustomAction(int action, const QString &title);
    QString contextMenuItemTagForAction(MenuAction, bool* checkable) const;

    QStringList supportedContentTypes() const;

    // Called from QWebPage as private slots.
    void _q_cleanupLeakMessages();
    void _q_onLoadProgressChanged(int);

    bool supportsContentType(const QString& mimeType) const;

    QObject* currentFrame() const;
    bool hasFocusedNode() const;
    void setDevicePixelRatio(float devicePixelRatio);
    float devicePixelRatio();

    bool isPlayingAudio() const;

    static void openNewWindow(const QUrl&, WebCore::Frame*);

    QWebSettings *settings;

    WebCore::Page *page;
    QHash<const void*, const void*> lastFocusedFrames; //frame -> frame in its subtree that last had focus (addresses only)
    QScopedPointer<QWebPageClient> client;

    QWebPluginFactory *pluginFactory;

    QPoint tripleClick;
    QBasicTimer tripleClickTimer;

    bool clickCausedFocus;
    bool mousePressed;
    bool m_useNativeVirtualKeyAsDOMKey;
    quint64 m_totalBytes;
    quint64 m_bytesReceived;

public:
    static bool drtRun;

    friend class WebCore::ChromeClientQt;
};

#endif // QWebPageAdapter_h
