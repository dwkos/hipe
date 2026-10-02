/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>
    Copyright (C) 2010 Holger Hans Peter Freyther

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

#include "../util.h"
#include <QClipboard>
#include <QDir>
#include <QMainWindow>
#include <QMenu>
#include <QRegExp>
#include <QStyle>
#include <QtTest/QtTest>
#include <QTextCharFormat>
#include <private/qinputmethod_p.h>
#include <qgraphicsscene.h>
#include <qgraphicsview.h>
#include <qgraphicswebview.h>
#include <qpa/qplatforminputcontext.h>
#include <qwebelement.h>
#include <qwebframe.h>
#include <qwebpage.h>
#include <qwebview.h>


static void removeRecursive(const QString& dirname)
{
    QDir dir(dirname);
    QFileInfoList entries(dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot));
    for (int i = 0; i < entries.count(); ++i)
        if (entries[i].isDir())
            removeRecursive(entries[i].filePath());
        else
            dir.remove(entries[i].fileName());
    QDir().rmdir(dirname);
}

class TestInputContext : public QPlatformInputContext
{
public:
    TestInputContext()
    : m_visible(false)
    {
        QInputMethodPrivate* inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
        inputMethodPrivate->testContext = this;
    }

    ~TestInputContext()
    {
        QInputMethodPrivate* inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
        inputMethodPrivate->testContext = 0;
    }

    virtual void showInputPanel()
    {
        m_visible = true;
    }
    virtual void hideInputPanel()
    {
        m_visible = false;
    }
    virtual bool isInputPanelVisible() const
    {
        return m_visible;
    }

    bool m_visible;
};

class tst_QWebPage : public QObject
{
    Q_OBJECT

public:
    tst_QWebPage();
    virtual ~tst_QWebPage();

public Q_SLOTS:
    void init();
    void cleanup();
    void cleanupFiles();

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void updatePositionDependentActionsCrash();

    // Selection / caret behaviour, driven through the C++ API (QWebPage actions,
    // inputMethodQuery, QWebElement::setSelectionRange) rather than JS.
    void cursorMovements();
    void textSelection();
    void textEditing();
    void selectAllShortcut();

    void frameAt();

    void inputMethods_data();
    void inputMethods();
    void inputMethodsTextFormat_data();
    void inputMethodsTextFormat();

    void crashTests_LazyInitializationOfMainFrame();

    void screenshot_data();
    void screenshot();

    void findText();
    void deleteQWebViewTwice();
    void renderOnRepaintRequestedShouldNotRecurse();

private:
    QWebView* m_view { nullptr };
    QWebPage* m_page { nullptr };
    QString tmpDirPath() const
    {
        static QString tmpd = QDir::tempPath() + "/tst_qwebpage-"
            + QDateTime::currentDateTime().toString(QLatin1String("yyyyMMddhhmmss"));
        return tmpd;
    }
};

tst_QWebPage::tst_QWebPage()
{
}

tst_QWebPage::~tst_QWebPage()
{
}

void tst_QWebPage::init()
{
    m_view = new QWebView();
    m_page = m_view->page();
}

void tst_QWebPage::cleanup()
{
    delete m_view;
}

void tst_QWebPage::cleanupFiles()
{
    removeRecursive(tmpDirPath());
}

void tst_QWebPage::initTestCase()
{
    cleanupFiles(); // In case there are old files from previous runs
}

void tst_QWebPage::cleanupTestCase()
{
    cleanupFiles(); // Be nice
}

// https://bugs.webkit.org/show_bug.cgi?id=51331
void tst_QWebPage::updatePositionDependentActionsCrash()
{
    QWebView view;
    view.setHtml("<p>test");
    QPoint pos(0, 0);
    view.page()->updatePositionDependentActions(pos);
    QMenu* contextMenu = 0;
    foreach (QObject* child, view.children()) {
        contextMenu = qobject_cast<QMenu*>(child);
        if (contextMenu)
            break;
    }
    QVERIFY(!contextMenu);
}

// selectedText() / selectedHtml() are inherited from QWebPage; the offset and
// collapsed queries read the frame selection through the input-method API (the
// same path IMEs use), which is what replaced the old window.getSelection() JS.
class CursorTrackedPage : public QWebPage
{
public:

    CursorTrackedPage(QWidget *parent = 0): QWebPage(parent) {
        setViewportSize(QSize(1024, 768)); // big space
    }

    int selectionStartOffset() {
        return inputMethodQuery(Qt::ImAnchorPosition).toInt();
    }

    int selectionEndOffset() {
        return inputMethodQuery(Qt::ImCursorPosition).toInt();
    }

    // true if anchor == cursor, i.e. a caret rather than a range
    bool isSelectionCollapsed() {
        return inputMethodQuery(Qt::ImAnchorPosition).toInt() == inputMethodQuery(Qt::ImCursorPosition).toInt();
    }
};

void tst_QWebPage::cursorMovements()
{
    CursorTrackedPage* page = new CursorTrackedPage;
    QString content("<html><body><p id=one>The quick brown fox</p><p id=two>jumps over the lazy dog</p><p>May the source<br/>be with you!</p></body></html>");
    page->mainFrame()->setHtml(content);
    page->mainFrame()->setFocus();

    // select the text of the first paragraph
    QWebElement one = page->mainFrame()->findFirstElement("#one");
    one.select();
    QCOMPARE(page->selectedText().trimmed(), QString::fromLatin1("The quick brown fox"));
    QVERIFY(page->selectedHtml().contains(QLatin1String("The quick brown fox")));

    // these actions must exist
    QVERIFY(page->action(QWebPage::MoveToNextChar) != 0);
    QVERIFY(page->action(QWebPage::MoveToPreviousChar) != 0);
    QVERIFY(page->action(QWebPage::MoveToNextWord) != 0);
    QVERIFY(page->action(QWebPage::MoveToPreviousWord) != 0);
    QVERIFY(page->action(QWebPage::MoveToNextLine) != 0);
    QVERIFY(page->action(QWebPage::MoveToPreviousLine) != 0);
    QVERIFY(page->action(QWebPage::MoveToStartOfLine) != 0);
    QVERIFY(page->action(QWebPage::MoveToEndOfLine) != 0);
    QVERIFY(page->action(QWebPage::MoveToStartOfBlock) != 0);
    QVERIFY(page->action(QWebPage::MoveToEndOfBlock) != 0);
    QVERIFY(page->action(QWebPage::MoveToStartOfDocument) != 0);
    QVERIFY(page->action(QWebPage::MoveToEndOfDocument) != 0);

    // right now they are disabled because contentEditable is false
    QCOMPARE(page->action(QWebPage::MoveToNextChar)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::MoveToPreviousChar)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::MoveToNextWord)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::MoveToPreviousWord)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::MoveToNextLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::MoveToPreviousLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::MoveToStartOfLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::MoveToEndOfLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::MoveToStartOfBlock)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::MoveToEndOfBlock)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::MoveToStartOfDocument)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::MoveToEndOfDocument)->isEnabled(), false);

    // make it editable before navigating the cursor
    page->setContentEditable(true);

    // here the actions are enabled after contentEditable is true
    QCOMPARE(page->action(QWebPage::MoveToNextChar)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::MoveToPreviousChar)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::MoveToNextWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::MoveToPreviousWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::MoveToNextLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::MoveToPreviousLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::MoveToStartOfLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::MoveToEndOfLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::MoveToStartOfBlock)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::MoveToEndOfBlock)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::MoveToStartOfDocument)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::MoveToEndOfDocument)->isEnabled(), true);

    // cursor will be before the word "jump"
    page->triggerAction(QWebPage::MoveToNextChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);

    // cursor will be between 'j' and 'u' in the word "jump"
    page->triggerAction(QWebPage::MoveToNextChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 1);

    // cursor will be between 'u' and 'm' in the word "jump"
    page->triggerAction(QWebPage::MoveToNextChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 2);

    // cursor will be after the word "jump"
    page->triggerAction(QWebPage::MoveToNextWord);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 5);

    // cursor will be after the word "lazy"
    page->triggerAction(QWebPage::MoveToNextWord);
    page->triggerAction(QWebPage::MoveToNextWord);
    page->triggerAction(QWebPage::MoveToNextWord);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 19);

    // cursor will be between 'z' and 'y' in "lazy"
    page->triggerAction(QWebPage::MoveToPreviousChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 18);

    // cursor will be between 'a' and 'z' in "lazy"
    page->triggerAction(QWebPage::MoveToPreviousChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 17);

    // cursor will be before the word "lazy"
    page->triggerAction(QWebPage::MoveToPreviousWord);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 15);

    // cursor will be before the word "quick"
    page->triggerAction(QWebPage::MoveToPreviousWord);
    page->triggerAction(QWebPage::MoveToPreviousWord);
    page->triggerAction(QWebPage::MoveToPreviousWord);
    page->triggerAction(QWebPage::MoveToPreviousWord);
    page->triggerAction(QWebPage::MoveToPreviousWord);
    page->triggerAction(QWebPage::MoveToPreviousWord);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 4);

    // cursor will be between 'p' and 's' in the word "jumps"
    page->triggerAction(QWebPage::MoveToNextWord);
    page->triggerAction(QWebPage::MoveToNextWord);
    page->triggerAction(QWebPage::MoveToNextWord);
    page->triggerAction(QWebPage::MoveToNextChar);
    page->triggerAction(QWebPage::MoveToNextChar);
    page->triggerAction(QWebPage::MoveToNextChar);
    page->triggerAction(QWebPage::MoveToNextChar);
    page->triggerAction(QWebPage::MoveToNextChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 4);

    // cursor will be before the word "jumps"
    page->triggerAction(QWebPage::MoveToStartOfLine);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);

    // cursor will be after the word "dog"
    page->triggerAction(QWebPage::MoveToEndOfLine);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 23);

    // cursor will be between 'w' and 'n' in "brown"
    page->triggerAction(QWebPage::MoveToStartOfLine);
    page->triggerAction(QWebPage::MoveToPreviousWord);
    page->triggerAction(QWebPage::MoveToPreviousWord);
    page->triggerAction(QWebPage::MoveToNextChar);
    page->triggerAction(QWebPage::MoveToNextChar);
    page->triggerAction(QWebPage::MoveToNextChar);
    page->triggerAction(QWebPage::MoveToNextChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 14);

    // cursor will be after the word "fox"
    page->triggerAction(QWebPage::MoveToEndOfLine);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 19);

    // cursor will be before the word "The"
    page->triggerAction(QWebPage::MoveToStartOfDocument);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);

    // cursor will be after the word "you!"
    page->triggerAction(QWebPage::MoveToEndOfDocument);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 12);

    // cursor will be before the word "be"
    page->triggerAction(QWebPage::MoveToStartOfBlock);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);

    // cursor will be after the word "you!"
    page->triggerAction(QWebPage::MoveToEndOfBlock);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 12);

    // try to move before the document start
    page->triggerAction(QWebPage::MoveToStartOfDocument);
    page->triggerAction(QWebPage::MoveToPreviousChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);
    page->triggerAction(QWebPage::MoveToStartOfDocument);
    page->triggerAction(QWebPage::MoveToPreviousWord);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);

    // try to move past the document end
    page->triggerAction(QWebPage::MoveToEndOfDocument);
    page->triggerAction(QWebPage::MoveToNextChar);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 12);
    page->triggerAction(QWebPage::MoveToEndOfDocument);
    page->triggerAction(QWebPage::MoveToNextWord);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 12);

    delete page;
}

void tst_QWebPage::textSelection()
{
    CursorTrackedPage* page = new CursorTrackedPage;
    QString content("<html><body><p id=one>The quick brown fox</p>" \
        "<p id=two>jumps over the lazy dog</p>" \
        "<p>May the source<br/>be with you!</p></body></html>");
    page->mainFrame()->setHtml(content);
    page->mainFrame()->setFocus();

    // these actions must exist
    QVERIFY(page->action(QWebPage::SelectAll) != 0);
    QVERIFY(page->action(QWebPage::SelectNextChar) != 0);
    QVERIFY(page->action(QWebPage::SelectPreviousChar) != 0);
    QVERIFY(page->action(QWebPage::SelectNextWord) != 0);
    QVERIFY(page->action(QWebPage::SelectPreviousWord) != 0);
    QVERIFY(page->action(QWebPage::SelectNextLine) != 0);
    QVERIFY(page->action(QWebPage::SelectPreviousLine) != 0);
    QVERIFY(page->action(QWebPage::SelectStartOfLine) != 0);
    QVERIFY(page->action(QWebPage::SelectEndOfLine) != 0);
    QVERIFY(page->action(QWebPage::SelectStartOfBlock) != 0);
    QVERIFY(page->action(QWebPage::SelectEndOfBlock) != 0);
    QVERIFY(page->action(QWebPage::SelectStartOfDocument) != 0);
    QVERIFY(page->action(QWebPage::SelectEndOfDocument) != 0);

    // right now they are disabled because contentEditable is false and 
    // there isn't an existing selection to modify
    QCOMPARE(page->action(QWebPage::SelectNextChar)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::SelectPreviousChar)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::SelectNextWord)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::SelectPreviousWord)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::SelectNextLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::SelectPreviousLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::SelectStartOfLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::SelectEndOfLine)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::SelectStartOfBlock)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::SelectEndOfBlock)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::SelectStartOfDocument)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::SelectEndOfDocument)->isEnabled(), false);

    // ..but SelectAll is awalys enabled
    QCOMPARE(page->action(QWebPage::SelectAll)->isEnabled(), true);

    // Verify hasSelection returns false since there is no selection yet...
    QCOMPARE(page->hasSelection(), false);

    // select the text of the first paragraph
    QWebElement one = page->mainFrame()->findFirstElement("#one");
    one.select();
    QCOMPARE(page->selectedText().trimmed(), QString::fromLatin1("The quick brown fox"));
    QVERIFY(page->selectedHtml().contains(QLatin1String("The quick brown fox")));

    // Make sure hasSelection returns true, since there is selected text now...
    QCOMPARE(page->hasSelection(), true);

    // here the actions are enabled after a selection has been created
    QCOMPARE(page->action(QWebPage::SelectNextChar)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectPreviousChar)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectNextWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectPreviousWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectNextLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectPreviousLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectStartOfLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectEndOfLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectStartOfBlock)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectEndOfBlock)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectStartOfDocument)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectEndOfDocument)->isEnabled(), true);

    // make it editable before navigating the cursor
    page->setContentEditable(true);

    // cursor will be before the word "The", this makes sure there is a charet
    page->triggerAction(QWebPage::MoveToStartOfDocument);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);

    // here the actions are enabled after contentEditable is true
    QCOMPARE(page->action(QWebPage::SelectNextChar)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectPreviousChar)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectNextWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectPreviousWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectNextLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectPreviousLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectStartOfLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectEndOfLine)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectStartOfBlock)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectEndOfBlock)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectStartOfDocument)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SelectEndOfDocument)->isEnabled(), true);

    delete page;
}

void tst_QWebPage::textEditing()
{
    CursorTrackedPage* page = new CursorTrackedPage;
    QString content("<html><body><p id=one>The quick brown fox</p>" \
        "<p id=two>jumps over the lazy dog</p>" \
        "<p>May the source<br/>be with you!</p></body></html>");
    page->mainFrame()->setHtml(content);
    page->mainFrame()->setFocus();

    // these actions must exist
    QVERIFY(page->action(QWebPage::Cut) != 0);
    QVERIFY(page->action(QWebPage::Copy) != 0);
    QVERIFY(page->action(QWebPage::Paste) != 0);
    QVERIFY(page->action(QWebPage::DeleteStartOfWord) != 0);
    QVERIFY(page->action(QWebPage::DeleteEndOfWord) != 0);
    QVERIFY(page->action(QWebPage::SetTextDirectionDefault) != 0);
    QVERIFY(page->action(QWebPage::SetTextDirectionLeftToRight) != 0);
    QVERIFY(page->action(QWebPage::SetTextDirectionRightToLeft) != 0);
    QVERIFY(page->action(QWebPage::ToggleBold) != 0);
    QVERIFY(page->action(QWebPage::ToggleItalic) != 0);
    QVERIFY(page->action(QWebPage::ToggleUnderline) != 0);
    QVERIFY(page->action(QWebPage::InsertParagraphSeparator) != 0);
    QVERIFY(page->action(QWebPage::InsertLineSeparator) != 0);
    QVERIFY(page->action(QWebPage::PasteAndMatchStyle) != 0);
    QVERIFY(page->action(QWebPage::RemoveFormat) != 0);
    QVERIFY(page->action(QWebPage::ToggleStrikethrough) != 0);
    QVERIFY(page->action(QWebPage::ToggleSubscript) != 0);
    QVERIFY(page->action(QWebPage::ToggleSuperscript) != 0);
    QVERIFY(page->action(QWebPage::InsertUnorderedList) != 0);
    QVERIFY(page->action(QWebPage::InsertOrderedList) != 0);
    QVERIFY(page->action(QWebPage::Indent) != 0);
    QVERIFY(page->action(QWebPage::Outdent) != 0);
    QVERIFY(page->action(QWebPage::AlignCenter) != 0);
    QVERIFY(page->action(QWebPage::AlignJustified) != 0);
    QVERIFY(page->action(QWebPage::AlignLeft) != 0);
    QVERIFY(page->action(QWebPage::AlignRight) != 0);

    // right now they are disabled because contentEditable is false
    QCOMPARE(page->action(QWebPage::Cut)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::Paste)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::DeleteStartOfWord)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::DeleteEndOfWord)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::SetTextDirectionDefault)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::SetTextDirectionLeftToRight)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::SetTextDirectionRightToLeft)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::ToggleBold)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::ToggleItalic)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::ToggleUnderline)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::InsertParagraphSeparator)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::InsertLineSeparator)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::PasteAndMatchStyle)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::RemoveFormat)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::ToggleStrikethrough)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::ToggleSubscript)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::ToggleSuperscript)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::InsertUnorderedList)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::InsertOrderedList)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::Indent)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::Outdent)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::AlignCenter)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::AlignJustified)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::AlignLeft)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::AlignRight)->isEnabled(), false);

    // Select everything
    page->triggerAction(QWebPage::SelectAll);

    // make sure it is enabled since there is a selection
    QCOMPARE(page->action(QWebPage::Copy)->isEnabled(), true);

    // make it editable before navigating the cursor
    page->setContentEditable(true);

    // clear the selection
    page->triggerAction(QWebPage::MoveToStartOfDocument);
    QVERIFY(page->isSelectionCollapsed());
    QCOMPARE(page->selectionStartOffset(), 0);

    // make sure it is disabled since there isn't a selection
    QCOMPARE(page->action(QWebPage::Copy)->isEnabled(), false);

    // here the actions are enabled after contentEditable is true
    QCOMPARE(page->action(QWebPage::Paste)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::DeleteStartOfWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::DeleteEndOfWord)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SetTextDirectionDefault)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SetTextDirectionLeftToRight)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::SetTextDirectionRightToLeft)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::ToggleBold)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::ToggleItalic)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::ToggleUnderline)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::InsertParagraphSeparator)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::InsertLineSeparator)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::PasteAndMatchStyle)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::ToggleStrikethrough)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::ToggleSubscript)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::ToggleSuperscript)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::InsertUnorderedList)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::InsertOrderedList)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::Indent)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::Outdent)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::AlignCenter)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::AlignJustified)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::AlignLeft)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::AlignRight)->isEnabled(), true);
    
    // make sure these are disabled since there isn't a selection
    QCOMPARE(page->action(QWebPage::Cut)->isEnabled(), false);
    QCOMPARE(page->action(QWebPage::RemoveFormat)->isEnabled(), false);
    
    // make sure everything is selected
    page->triggerAction(QWebPage::SelectAll);
    
    // this is only true if there is an editable selection
    QCOMPARE(page->action(QWebPage::Cut)->isEnabled(), true);
    QCOMPARE(page->action(QWebPage::RemoveFormat)->isEnabled(), true);

    delete page;
}

// Regression guard for the WebAction enum <-> editorCommandWebActions[] table
// staying in sync: triggerAction(SelectAll) must run the "SelectAll" editor
// command (select the whole document), never a paste. A one-slot skew in that
// positional table silently turns SelectAll into PasteAndMatchStyle.
void tst_QWebPage::selectAllShortcut()
{
    CursorTrackedPage* page = new CursorTrackedPage;
    const QString body = QLatin1String("The quick brown fox");
    page->mainFrame()->setHtml(QString::fromLatin1("<html><body><p id=one>%1</p></body></html>").arg(body));
    page->mainFrame()->setFocus();
    page->setContentEditable(true);

#ifndef QT_NO_CLIPBOARD
    QApplication::clipboard()->setText(QLatin1String("CLIPBOARD-SENTINEL"));
#endif

    // Via the QAction (this is the path periscope's Edit menu uses).
    page->action(QWebPage::SelectAll)->trigger();
    QCOMPARE(page->selectedText(), body);
    QCOMPARE(page->mainFrame()->toPlainText().trimmed(), body);

    // And via triggerAction() directly, from a collapsed caret.
    page->triggerAction(QWebPage::MoveToStartOfDocument);
    QVERIFY(page->isSelectionCollapsed());
    page->triggerAction(QWebPage::SelectAll);
    QCOMPARE(page->selectedText(), body);
    QCOMPARE(page->mainFrame()->toPlainText().trimmed(), body);

#ifndef QT_NO_CLIPBOARD
    // The clipboard sentinel must never have landed in the document.
    QVERIFY(!page->mainFrame()->toPlainText().contains(QLatin1String("CLIPBOARD-SENTINEL")));
#endif

    delete page;
}

void frameAtHelper(QWebPage* webPage, QWebFrame* webFrame, QPoint framePosition)
{
    if (!webFrame)
        return;

    framePosition += QPoint(webFrame->pos());
    QList<QWebFrame*> children = webFrame->childFrames();
    for (int i = 0; i < children.size(); ++i) {
        if (children.at(i)->childFrames().size() > 0)
            frameAtHelper(webPage, children.at(i), framePosition);

        QRect frameRect(children.at(i)->pos() + framePosition, children.at(i)->geometry().size());
        QVERIFY(children.at(i) == webPage->frameAt(frameRect.topLeft()));
    }
}

void tst_QWebPage::frameAt()
{
    QWebView webView;
    QWebPage* webPage = webView.page();
    QSignalSpy loadSpy(webPage, SIGNAL(loadFinished(bool)));
    QUrl url = QUrl("qrc:///resources/iframe.html");
    webPage->mainFrame()->load(url);
    QTRY_COMPARE(loadSpy.count(), 1);
    frameAtHelper(webPage, webPage->mainFrame(), webPage->mainFrame()->pos());
}

void tst_QWebPage::inputMethods_data()
{
    QTest::addColumn<QString>("viewType");
    QTest::newRow("QWebView") << "QWebView";
    QTest::newRow("QGraphicsWebView") << "QGraphicsWebView";
}

static Qt::InputMethodHints inputMethodHints(QObject* object)
{
    if (QGraphicsObject* o = qobject_cast<QGraphicsObject*>(object))
        return o->inputMethodHints();
    if (QWidget* w = qobject_cast<QWidget*>(object))
        return w->inputMethodHints();
    return Qt::InputMethodHints();
}

static bool inputMethodEnabled(QObject* object)
{
    if (QGraphicsObject* o = qobject_cast<QGraphicsObject*>(object))
        return o->flags() & QGraphicsItem::ItemAcceptsInputMethod;
    if (QWidget* w = qobject_cast<QWidget*>(object))
        return w->testAttribute(Qt::WA_InputMethodEnabled);
    return false;
}

static void clickOnPage(QWebPage* page, const QPoint& position)
{
    QMouseEvent evpres(QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    page->event(&evpres);
    QMouseEvent evrel(QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    page->event(&evrel);
}

// C++ equivalent of `el.focus(); el.select();` - focus a form field and select
// all of its contents. `selector` is a CSS selector.
static void focusAndSelectAll(QWebPage* page, const QString& selector)
{
    QWebElement element = page->mainFrame()->documentElement().findFirst(selector);
    element.setFocus();
    page->triggerAction(QWebPage::SelectAll);
}

// C++ equivalent of reading `el.value` on the focused form field.
static QString focusedFieldValue(QWebPage* page)
{
    return page->inputMethodQuery(Qt::ImSurroundingText).toString();
}

void tst_QWebPage::inputMethods()
{
    QFETCH(QString, viewType);
    QWebPage* page = new QWebPage;
    QObject* view = 0;
    QObject* container = 0;
    if (viewType == "QWebView") {
        QWebView* wv = new QWebView;
        wv->setPage(page);
        view = wv;
        container = view;
    } else if (viewType == "QGraphicsWebView") {
        QGraphicsWebView* wv = new QGraphicsWebView;
        wv->setPage(page);
        view = wv;

        QGraphicsView* gv = new QGraphicsView;
        QGraphicsScene* scene = new QGraphicsScene(gv);
        gv->setScene(scene);
        scene->addItem(wv);
        wv->setGeometry(QRect(0, 0, 500, 500));

        container = gv;
    } else
        QVERIFY2(false, "Unknown view type");

    page->settings()->setFontFamily(QWebSettings::SerifFont, page->settings()->fontFamily(QWebSettings::FixedFont));
    page->mainFrame()->setHtml("<html><body>" \
                                            "<input type='text' id='input1' style='font-family: serif' value='' maxlength='20'/><br>" \
                                            "<input type='password'/>" \
                                            "</body></html>");
    page->mainFrame()->setFocus();

    TestInputContext testContext;

    QWebElementCollection inputs = page->mainFrame()->documentElement().findAll("input");
    QPoint textInputCenter = inputs.at(0).geometry().center();

    clickOnPage(page, textInputCenter);

    // This part of the test checks if the SIP (Software Input Panel) is triggered,
    // which normally happens on mobile platforms, when a user input form receives
    // a mouse click.
    int  inputPanel = 0;
    if (viewType == "QWebView") {
        if (QWebView* wv = qobject_cast<QWebView*>(view))
            inputPanel = wv->style()->styleHint(QStyle::SH_RequestSoftwareInputPanel);
    } else if (viewType == "QGraphicsWebView") {
        if (QGraphicsWebView* wv = qobject_cast<QGraphicsWebView*>(view))
            inputPanel = wv->style()->styleHint(QStyle::SH_RequestSoftwareInputPanel);
    }

    // For non-mobile platforms RequestSoftwareInputPanel event is not called
    // because there is no SIP (Software Input Panel) triggered. In the case of a
    // mobile platform, an input panel, e.g. virtual keyboard, is usually invoked
    // and the RequestSoftwareInputPanel event is called. For these two situations
    // this part of the test can verified as the checks below.
    if (inputPanel)
        QVERIFY(testContext.isInputPanelVisible());
    else
        QVERIFY(!testContext.isInputPanelVisible());
    testContext.hideInputPanel();

    clickOnPage(page, textInputCenter);
    QVERIFY(testContext.isInputPanelVisible());

    //ImMicroFocus
    QVariant variant = page->inputMethodQuery(Qt::ImMicroFocus);
    QVERIFY(inputs.at(0).geometry().contains(variant.toRect().topLeft()));

    // We assigned the serif font famility to be the same as the fixef font family.
    // Then test ImFont on a serif styled element, we should get our fixef font family.
    variant = page->inputMethodQuery(Qt::ImFont);
    QFont font = variant.value<QFont>();
    QCOMPARE(page->settings()->fontFamily(QWebSettings::FixedFont), font.family());

    QList<QInputMethodEvent::Attribute> inputAttributes;

    //Insert text.
    {
        QInputMethodEvent eventText("QtWebKit", inputAttributes);
        QSignalSpy signalSpy(page, SIGNAL(microFocusChanged()));
        page->event(&eventText);
        QCOMPARE(signalSpy.count(), 0);
    }

    {
        QInputMethodEvent eventText("", inputAttributes);
        eventText.setCommitString(QString("QtWebKit"), 0, 0);
        page->event(&eventText);
    }

    //ImMaximumTextLength
    variant = page->inputMethodQuery(Qt::ImMaximumTextLength);
    QCOMPARE(20, variant.toInt());

    //Set selection
    inputAttributes << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, 3, 2, QVariant());
    QInputMethodEvent eventSelection("",inputAttributes);
    page->event(&eventSelection);

    //ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    int anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 3);

    //ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    int cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 5);

    //ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    QString selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString("eb"));

    //Set selection with negative length
    inputAttributes << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, 6, -5, QVariant());
    QInputMethodEvent eventSelection3("",inputAttributes);
    page->event(&eventSelection3);

    //ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 1);

    //ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 6);

    //ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString("tWebK"));

    //ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    QString value = variant.value<QString>();
    QCOMPARE(value, QString("QtWebKit"));

    {
        QList<QInputMethodEvent::Attribute> attributes;
        // Clear the selection, so the next test does not clear any contents.
        QInputMethodEvent::Attribute newSelection(QInputMethodEvent::Selection, 0, 0, QVariant());
        attributes.append(newSelection);
        QInputMethodEvent event("composition", attributes);
        page->event(&event);
    }

    // A ongoing composition should not change the surrounding text before it is committed.
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    value = variant.value<QString>();
    QCOMPARE(value, QString("QtWebKit"));

    // Cancel current composition first
    inputAttributes << QInputMethodEvent::Attribute(QInputMethodEvent::Selection, 0, 0, QVariant());
    QInputMethodEvent eventSelection4("", inputAttributes);
    page->event(&eventSelection4);

    // START - Tests for Selection when the Editor is NOT in Composition mode

    // LEFT to RIGHT selection
    // Deselect the selection by sending MouseButtonPress events
    // This moves the current cursor to the end of the text
    clickOnPage(page, textInputCenter);

    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event(QString(), attributes);
        event.setCommitString("XXX", 0, 0);
        page->event(&event);
        event.setCommitString(QString(), -2, 2); // Erase two characters.
        page->event(&event);
        event.setCommitString(QString(), -1, 1); // Erase one character.
        page->event(&event);
        variant = page->inputMethodQuery(Qt::ImSurroundingText);
        value = variant.value<QString>();
        QCOMPARE(value, QString("QtWebKit"));
    }

    //Move to the start of the line
    page->triggerAction(QWebPage::MoveToStartOfLine);

    QKeyEvent keyRightEventPress(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
    QKeyEvent keyRightEventRelease(QEvent::KeyRelease, Qt::Key_Right, Qt::NoModifier);

    //Move 2 characters RIGHT
    for (int j = 0; j < 2; ++j) {
        page->event(&keyRightEventPress);
        page->event(&keyRightEventRelease);
    }

    //Select to the end of the line
    page->triggerAction(QWebPage::SelectEndOfLine);

    //ImAnchorPosition QtWebKit
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 2);

    //ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 8);

    //ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString("WebKit"));

    //RIGHT to LEFT selection
    //Deselect the selection (this moves the current cursor to the end of the text)
    clickOnPage(page, textInputCenter);

    //ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 8);

    //ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 8);

    //ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    QKeyEvent keyLeftEventPress(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier);
    QKeyEvent keyLeftEventRelease(QEvent::KeyRelease, Qt::Key_Left, Qt::NoModifier);

    //Move 2 characters LEFT
    for (int i = 0; i < 2; ++i) {
        page->event(&keyLeftEventPress);
        page->event(&keyLeftEventRelease);
    }

    //Select to the start of the line
    page->triggerAction(QWebPage::SelectStartOfLine);

    //ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 6);

    //ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 0);

    //ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString("QtWebK"));

    //END - Tests for Selection when the Editor is not in Composition mode

    //ImhHiddenText
    QPoint passwordInputCenter = inputs.at(1).geometry().center();
    clickOnPage(page, passwordInputCenter);

    QVERIFY(inputMethodEnabled(view));
    QVERIFY(inputMethodHints(view) & Qt::ImhHiddenText);

    clickOnPage(page, textInputCenter);
    QVERIFY(!(inputMethodHints(view) & Qt::ImhHiddenText));

    page->mainFrame()->setHtml("<html><body><p>nothing to input here");
    testContext.hideInputPanel();

    QWebElement para = page->mainFrame()->findFirstElement("p");
    clickOnPage(page, para.geometry().center());

    QVERIFY(!testContext.isInputPanelVisible());

    //START - Test for sending empty QInputMethodEvent
    page->mainFrame()->setHtml("<html><body>" \
                                            "<input type='text' id='input3' value='QtWebKit2'/>" \
                                            "</body></html>");
    focusAndSelectAll(page, QLatin1String("#input3"));

    //Send empty QInputMethodEvent
    QInputMethodEvent emptyEvent;
    page->event(&emptyEvent);

    QCOMPARE(focusedFieldValue(page), QString("QtWebKit2"));
    //END - Test for sending empty QInputMethodEvent

    page->mainFrame()->setHtml("<html><body>" \
                                            "<input type='text' id='input4' value='QtWebKit inputMethod'/>" \
                                            "</body></html>");
    focusAndSelectAll(page, QLatin1String("#input4"));

    // Clear the selection, also cancel the ongoing composition if there is one.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent::Attribute newSelection(QInputMethodEvent::Selection, 0, 0, QVariant());
        attributes.append(newSelection);
        QInputMethodEvent event("", attributes);
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    QString surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("QtWebKit inputMethod"));

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 0);

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 0);

    // 1. Insert a character to the begining of the line.
    // Send temporary text, which makes the editor has composition 'm'.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("m", attributes);
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("QtWebKit inputMethod"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 0);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 0);

    // Send temporary text, which makes the editor has composition 'n'.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("n", attributes);
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("QtWebKit inputMethod"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 0);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 0);

    // Send commit text, which makes the editor conforms composition.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("", attributes);
        event.setCommitString("o");
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oQtWebKit inputMethod"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 1);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 1);

    // 2. insert a character to the middle of the line.
    // Send temporary text, which makes the editor has composition 'd'.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("d", attributes);
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oQtWebKit inputMethod"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 1);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 1);

    // Send commit text, which makes the editor conforms composition.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("", attributes);
        event.setCommitString("e");
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oeQtWebKit inputMethod"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 2);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 2);

    // 3. Insert a character to the end of the line.
    page->triggerAction(QWebPage::MoveToEndOfLine);
    
    // Send temporary text, which makes the editor has composition 't'.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("t", attributes);
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oeQtWebKit inputMethod"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 22);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 22);

    // Send commit text, which makes the editor conforms composition.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("", attributes);
        event.setCommitString("t");
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oeQtWebKit inputMethodt"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 23);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 23);

    // 4. Replace the selection.
    page->triggerAction(QWebPage::SelectPreviousWord);

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString("inputMethodt"));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oeQtWebKit inputMethodt"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 11);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 23);

    // Send temporary text, which makes the editor has composition 'w'.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("w", attributes);
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oeQtWebKit "));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 11);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 11);

    // Send commit text, which makes the editor conforms composition.
    {
        QList<QInputMethodEvent::Attribute> attributes;
        QInputMethodEvent event("", attributes);
        event.setCommitString("2");
        page->event(&event);
    }

    // ImCurrentSelection
    variant = page->inputMethodQuery(Qt::ImCurrentSelection);
    selectionValue = variant.value<QString>();
    QCOMPARE(selectionValue, QString(""));

    // ImSurroundingText
    variant = page->inputMethodQuery(Qt::ImSurroundingText);
    surroundingValue = variant.value<QString>();
    QCOMPARE(surroundingValue, QString("oeQtWebKit 2"));

    // ImCursorPosition
    variant = page->inputMethodQuery(Qt::ImCursorPosition);
    cursorPosition =  variant.toInt();
    QCOMPARE(cursorPosition, 12);

    // ImAnchorPosition
    variant = page->inputMethodQuery(Qt::ImAnchorPosition);
    anchorPosition =  variant.toInt();
    QCOMPARE(anchorPosition, 12);

    // Check sending RequestSoftwareInputPanel event
    page->mainFrame()->setHtml("<html><body>" \
                                            "<input type='text' id='input5' value='QtWebKit inputMethod'/>" \
                                            "<div id='btnDiv' onclick='i=document.getElementById(&quot;input5&quot;); i.focus();'>abc</div>"\
                                            "</body></html>");
    QWebElement inputElement = page->mainFrame()->findFirstElement("div");
    clickOnPage(page, inputElement.geometry().center());

    QVERIFY(!testContext.isInputPanelVisible());

    // START - Newline test for textarea
    // Each pass focuses the textarea and selects all of it, so the newline key +
    // the two committed strings replace whatever is there; the field ends up as
    // "\n\nthird line" (one \n from the Enter/Return key, one from the commit).
    qApp->processEvents();
    page->mainFrame()->setHtml("<html><body>" \
                                            "<textarea rows='5' cols='1' id='input5'></textarea>" \
                                            "</body></html>");

    QList<QInputMethodEvent::Attribute> attribs;
    QInputMethodEvent eventText(QString(), attribs);
    eventText.setCommitString("\n");
    QInputMethodEvent eventText2(QString(), attribs);
    eventText2.setCommitString("third line");

    auto typeNewlineThenText = [&](int key, const QString& keyText, const char* label) {
        focusAndSelectAll(page, QLatin1String("#input5"));
        QKeyEvent keyEvent(QEvent::KeyPress, key, Qt::NoModifier, keyText);
        page->event(&keyEvent);
        page->event(&eventText);
        page->event(&eventText2);
        qApp->processEvents();
        QVERIFY2(focusedFieldValue(page) == QString("\n\nthird line"), label);
    };

    typeNewlineThenText(Qt::Key_Enter, QString(), "Enter, no key text");
    typeNewlineThenText(Qt::Key_Enter, QString("\r"), "Enter, key text \\r");
    typeNewlineThenText(Qt::Key_Enter, QString("\n"), "Enter, key text \\n");
    typeNewlineThenText(Qt::Key_Enter, QString("\n\r"), "Enter, key text \\n\\r");
    typeNewlineThenText(Qt::Key_Return, QString(), "Return, no key text");

    // END - Newline test for textarea

    delete container;
}

void tst_QWebPage::inputMethodsTextFormat_data()
{
    QTest::addColumn<QString>("string");
    QTest::addColumn<int>("start");
    QTest::addColumn<int>("length");

    QTest::newRow("") << QString("") << 0 << 0;
    QTest::newRow("Q") << QString("Q") << 0 << 1;
    QTest::newRow("Qt") << QString("Qt") << 0 << 1;
    QTest::newRow("Qt") << QString("Qt") << 0 << 2;
    QTest::newRow("Qt") << QString("Qt") << 1 << 1;
    QTest::newRow("Qt ") << QString("Qt ") << 0 << 1;
    QTest::newRow("Qt ") << QString("Qt ") << 1 << 1;
    QTest::newRow("Qt ") << QString("Qt ") << 2 << 1;
    QTest::newRow("Qt ") << QString("Qt ") << 2 << -1;
    QTest::newRow("Qt ") << QString("Qt ") << -2 << 3;
    QTest::newRow("Qt ") << QString("Qt ") << 0 << 3;
    QTest::newRow("Qt by") << QString("Qt by") << 0 << 1;
    QTest::newRow("Qt by Nokia") << QString("Qt by Nokia") << 0 << 1;
}


void tst_QWebPage::inputMethodsTextFormat()
{
    QWebPage* page = new QWebPage;
    QWebView* view = new QWebView;
    view->setPage(page);
    page->settings()->setFontFamily(QWebSettings::SerifFont, "FooSerifFont");
    page->mainFrame()->setHtml("<html><body>" \
                                            "<input type='text' id='input1' style='font-family: serif' value='' maxlength='20'/>");
    page->mainFrame()->documentElement().findFirst("#input1").setFocus();
    page->mainFrame()->setFocus();
    view->show();

    QFETCH(QString, string);
    QFETCH(int, start);
    QFETCH(int, length);

    QList<QInputMethodEvent::Attribute> attrs;
    QTextCharFormat format;
    format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
    format.setUnderlineColor(Qt::red);
    attrs.append(QInputMethodEvent::Attribute(QInputMethodEvent::TextFormat, start, length, format));
    QInputMethodEvent im(string, attrs);
    page->event(&im);

    QTest::qWait(1000);

    delete view;
}

void tst_QWebPage::crashTests_LazyInitializationOfMainFrame()
{
    {
        QWebPage webPage;
    }
    {
        QWebPage webPage;
        webPage.selectedText();
    }
    {
        QWebPage webPage;
        webPage.selectedHtml();
    }
    {
        QWebPage webPage;
        QPoint pos(10,10);
        webPage.updatePositionDependentActions(pos);
    }
}

static void takeScreenshot(QWebPage* page)
{
    QWebFrame* mainFrame = page->mainFrame();
    page->setViewportSize(mainFrame->contentsSize());
    QImage image(page->viewportSize(), QImage::Format_ARGB32);
    QPainter painter(&image);
    mainFrame->render(&painter);
    painter.end();
}

void tst_QWebPage::screenshot_data()
{
    QTest::addColumn<QString>("html");
    QTest::newRow("PlainText") << "<html><body id='b'>text</body></html>";
}

void tst_QWebPage::screenshot()
{
    if (!QDir(TESTS_SOURCE_DIR).exists())
        QSKIP(QString("This test requires access to resources found in '%1'").arg(TESTS_SOURCE_DIR).toLatin1().constData(), SkipAll);

    QDir::setCurrent(TESTS_SOURCE_DIR);

    QFETCH(QString, html);
    QWebPage* page = new QWebPage;
    QWebFrame* mainFrame = page->mainFrame();
    mainFrame->setHtml(html, QUrl::fromLocalFile(TESTS_SOURCE_DIR));
    ::waitForSignal(mainFrame, SIGNAL(loadFinished(bool)), 2000);

    // take screenshot without a view
    takeScreenshot(page);

    QWebView* view = new QWebView;
    view->setPage(page);

    // take screenshot when attached to a view
    takeScreenshot(page);

    delete page;
    delete view;

    QDir::setCurrent(QApplication::applicationDirPath());
}

void tst_QWebPage::findText()
{
    m_view->setHtml(QString("<html><head></head><body><div>foo bar</div></body></html>"));
    m_page->triggerAction(QWebPage::SelectAll);
    QVERIFY(!m_page->selectedText().isEmpty());
    QVERIFY(!m_page->selectedHtml().isEmpty());
    m_page->findText("");
    QVERIFY(m_page->selectedText().isEmpty());
    QVERIFY(m_page->selectedHtml().isEmpty());
    QStringList words = (QStringList() << "foo" << "bar");
    foreach (QString subString, words) {
        m_page->findText(subString, QWebPage::FindWrapsAroundDocument);
        QCOMPARE(m_page->selectedText(), subString);
        QVERIFY(m_page->selectedHtml().contains(subString));
        m_page->findText("");
        QVERIFY(m_page->selectedText().isEmpty());
        QVERIFY(m_page->selectedHtml().isEmpty());
    }
}

void tst_QWebPage::deleteQWebViewTwice()
{
    for (int i = 0; i < 2; ++i) {
        QMainWindow mainWindow;
        QWebView* webView = new QWebView(&mainWindow);
        mainWindow.setCentralWidget(webView);
        webView->setHtml(QString("<a href=\"#\">anchor</a>"));
        mainWindow.show();
        // The point of this test is that deleting the view (twice, via the loop)
        // does not crash; setHtml completes synchronously so there is no load
        // signal to wait for.
    }
}

class RepaintRequestedRenderer : public QObject {
    Q_OBJECT
public:
    RepaintRequestedRenderer(QWebPage* page, QPainter* painter)
        : m_page(page)
        , m_painter(painter)
        , m_recursionCount(0)
    {
        connect(m_page, SIGNAL(repaintRequested(QRect)), this, SLOT(onRepaintRequested(QRect)));
    }

Q_SIGNALS:
    void finished();

private Q_SLOTS:
    void onRepaintRequested(const QRect& rect)
    {
        QCOMPARE(m_recursionCount, 0);

        m_recursionCount++;
        m_page->mainFrame()->render(m_painter, rect);
        m_recursionCount--;

        QMetaObject::invokeMethod(this, "finished", Qt::QueuedConnection);
    }

private:
    QWebPage* m_page;
    QPainter* m_painter;
    int m_recursionCount;
};

void tst_QWebPage::renderOnRepaintRequestedShouldNotRecurse()
{
    QSize viewportSize(720, 576);
    QWebPage page;

    QImage image(viewportSize, QImage::Format_ARGB32);
    QPainter painter(&image);

    page.setPreferredContentsSize(viewportSize);
    page.setViewportSize(viewportSize);
    RepaintRequestedRenderer r(&page, &painter);

    page.mainFrame()->setHtml("zalan loves trunk", QUrl());

    QVERIFY(::waitForSignal(&r, SIGNAL(finished())));
}


QTEST_MAIN(tst_QWebPage)
#include "tst_qwebpage.moc"
