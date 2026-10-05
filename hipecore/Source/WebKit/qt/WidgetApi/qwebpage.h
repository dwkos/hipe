/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2007 Staikos Computing Services Inc.
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

#ifndef QWEBPAGE_H
#define QWEBPAGE_H

#include <HipeCore/qwebkitglobal.h>
#include <HipeCore/qwebsettings.h>

#include <QtCore/qobject.h>
#include <QtCore/qurl.h>
#include <QtWidgets/qwidget.h>

QT_BEGIN_NAMESPACE
class QUndoStack;
class QMenu;
class QScreen;
QT_END_NAMESPACE

class QWebElement;
class QWebFrame;
class QWebNetworkRequest;

class QWebFrameData;
class QWebHitTestResult;
class QWebNetworkInterface;
class QWebPageAdapter;
class QWebPagePrivate;
class QWebSecurityOrigin;

namespace WebCore {
    class ChromeClientQt;
    class EditorClientQt;
    class FrameLoaderClientQt;
    class ResourceHandle;

    struct FrameLoadRequest;
}

class QWEBKITWIDGETS_EXPORT QWebPage : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool modified READ isModified)
    Q_PROPERTY(QString selectedText READ selectedText)
    Q_PROPERTY(QString selectedHtml READ selectedHtml)
    Q_PROPERTY(bool hasSelection READ hasSelection)
    Q_PROPERTY(QSize viewportSize READ viewportSize WRITE setViewportSize)
    Q_PROPERTY(QSize preferredContentsSize READ preferredContentsSize WRITE setPreferredContentsSize)
    Q_PROPERTY(QPalette palette READ palette WRITE setPalette)
    Q_PROPERTY(bool contentEditable READ isContentEditable WRITE setContentEditable)
    Q_PROPERTY(VisibilityState visibilityState READ visibilityState WRITE setVisibilityState)
    Q_PROPERTY(bool recentlyAudible READ recentlyAudible NOTIFY recentlyAudibleChanged)
    Q_ENUMS(MessageLevel MessageSource VisibilityState WebAction)
public:
    enum WebAction {
        NoWebAction = - 1,

        CopyImageToClipboard,

        /*Back,
        Forward,
        Stop,
        Reload,*/

        Cut,
        Copy,
        Paste,

        Undo,
        Redo,
        MoveToNextChar,
        MoveToPreviousChar,
        MoveToNextWord,
        MoveToPreviousWord,
        MoveToNextLine,
        MoveToPreviousLine,
        MoveToStartOfLine,
        MoveToEndOfLine,
        MoveToStartOfBlock,
        MoveToEndOfBlock,
        MoveToStartOfDocument,
        MoveToEndOfDocument,
        SelectNextChar,
        SelectPreviousChar,
        SelectNextWord,
        SelectPreviousWord,
        SelectNextLine,
        SelectPreviousLine,
        SelectStartOfLine,
        SelectEndOfLine,
        SelectStartOfBlock,
        SelectEndOfBlock,
        SelectStartOfDocument,
        SelectEndOfDocument,
        DeleteStartOfWord,
        DeleteEndOfWord,

        SetTextDirectionDefault,
        SetTextDirectionLeftToRight,
        SetTextDirectionRightToLeft,

        ToggleBold,
        ToggleItalic,
        ToggleUnderline,

        InsertParagraphSeparator,
        InsertLineSeparator,

        SelectAll,

        PasteAndMatchStyle,
        RemoveFormat,

        ToggleStrikethrough,
        ToggleSubscript,
        ToggleSuperscript,
        InsertUnorderedList,
        InsertOrderedList,
        Indent,
        Outdent,

        AlignCenter,
        AlignJustified,
        AlignLeft,
        AlignRight,

        ToggleMediaControls,
        ToggleMediaLoop,
        ToggleMediaPlayPause,
        ToggleMediaMute,
        ToggleVideoFullscreen,

        Unselect,

        WebActionCount
    };

    enum FindFlag {
        FindBackward = 1,
        FindCaseSensitively = 2,
        FindWrapsAroundDocument = 4,
        HighlightAllOccurrences = 8,
        FindAtWordBeginningsOnly = 16,
        TreatMedialCapitalAsWordBeginning = 32,
        FindBeginsInSelection = 64,
        FindAtWordEndingsOnly = 128,
        FindExactMatchOnly = (FindAtWordBeginningsOnly | FindAtWordEndingsOnly)
    };
    Q_DECLARE_FLAGS(FindFlags, FindFlag)

    enum WebWindowType {
        WebBrowserWindow,
        WebModalDialog
    };

    enum VisibilityState {
        VisibilityStateVisible,
        VisibilityStateHidden,
        VisibilityStatePrerender,
        VisibilityStateUnloaded
    };

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

    enum MessageLevel {
        LogMessageLevel = 1,
        WarningMessageLevel = 2,
        ErrorMessageLevel = 3,
        DebugMessageLevel = 4,
        InfoMessageLevel = 5,
    };

    typedef void (*ContextMenuCallback)(void* userPtr, const QPoint& globalPos, const QWebElement& element);

    explicit QWebPage(QObject *parent = Q_NULLPTR);
    ~QWebPage();

    QWebFrame *mainFrame() const;
    QWebFrame *currentFrame() const;
    QWebFrame* frameAt(const QPoint& pos) const;

    QWebSettings *settings() const;

    void setView(QWidget *view);
    QWidget *view() const;

    bool isModified() const;
#ifndef QT_NO_UNDOSTACK
    QUndoStack *undoStack() const;
#endif


    quint64 totalBytes() const;
    quint64 bytesReceived() const;

    VisibilityState visibilityState() const;
    void setVisibilityState(VisibilityState);

    bool recentlyAudible() const;

    bool hasSelection() const;
    QString selectedText() const;
    QString selectedHtml() const;

#ifndef QT_NO_ACTION
    QAction *action(WebAction action) const;
    QAction *customAction(int action) const;
#endif
    virtual void triggerAction(WebAction action, bool checked = false);
    void insertText(const QString& text);

    void setDevicePixelRatio(qreal ratio);
    qreal devicePixelRatio() const;
    void resetDevicePixelRatio();

    QSize viewportSize() const;
    void setViewportSize(const QSize &size) const;

    QSize preferredContentsSize() const;
    void setPreferredContentsSize(const QSize &size) const;
    void setActualVisibleContentRect(const QRect& rect) const;

    bool event(QEvent*) Q_DECL_OVERRIDE;
    bool focusNextPrevChild(bool next);

    QVariant inputMethodQuery(Qt::InputMethodQuery property) const;

    bool findText(const QString &subString, FindFlags options = FindFlags());

    void setPalette(const QPalette &palette);
    QPalette palette() const;

    void setContentEditable(bool editable);
    bool isContentEditable() const;

#ifndef QT_NO_CONTEXTMENU
    bool swallowContextMenuEvent(QContextMenuEvent *event);

    void setContextMenuCallback(ContextMenuCallback callback, void* userPtr = nullptr);
    void clearContextMenuCallback();
#endif
    void updatePositionDependentActions(const QPoint &pos);

    QStringList supportedContentTypes() const;
    bool supportsContentType(const QString& mimeType) const;

    QWebPageAdapter* handle() const;


Q_SIGNALS:
    void loadStarted();
    void loadProgress(int progress);
    void loadFinished(bool ok);

    void linkHovered(const QString &link, const QString &title, const QString &textContent);
    void statusBarMessage(const QString& text);
    void selectionChanged();
    void frameCreated(QWebFrame *frame);
    void geometryChangeRequested(const QRect& geom);
    void repaintRequested(const QRect& dirtyRect);
    void scrollRequested(int dx, int dy, const QRect& scrollViewRect);

    void toolBarVisibilityChangeRequested(bool visible);
    void statusBarVisibilityChangeRequested(bool visible);
    void menuBarVisibilityChangeRequested(bool visible);

    void focusedElementChanged(const QWebElement &element);
    void microFocusChanged();
    void contentsChanged();

    void restoreFrameStateRequested(QWebFrame* frame);

    void consoleMessageReceived(MessageSource source, MessageLevel level, const QString& message, int lineNumber, const QString& sourceID);

    void recentlyAudibleChanged(bool recentlyAudible);

protected:
    virtual QWebPage *createWindow(WebWindowType type);


private:
    Q_PRIVATE_SLOT(d, void _q_onLoadProgressChanged(int))
#ifndef QT_NO_ACTION
    Q_PRIVATE_SLOT(d, void _q_webActionTriggered(bool checked))
    Q_PRIVATE_SLOT(d, void _q_customActionTriggered(bool checked))
#endif
    Q_PRIVATE_SLOT(d, void _q_cleanupLeakMessages())
    Q_PRIVATE_SLOT(d, void _q_updateScreen(QScreen*))

    QWebPagePrivate *d;

    friend class QWebFrame;
    friend class QWebPagePrivate;
    friend class QWebView;
    friend class QWebViewPrivate;
    friend class QGraphicsWebView;
    friend class QGraphicsWebViewPrivate;
    friend class WebCore::ChromeClientQt;
    friend class WebCore::EditorClientQt;
    friend class WebCore::FrameLoaderClientQt;
    friend class WebCore::ResourceHandle;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QWebPage::FindFlags)

#endif
