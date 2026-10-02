/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2009 Torch Mobile Inc.
    Copyright (C) 2009 Girish Ramakrishnan <girish@forwardbias.in>

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

#include <qtest.h>
#include "../util.h"

#include <qpainter.h>
#include <qwebview.h>
#include <qwebpage.h>
#include <qwebelement.h>
#include <qwebframe.h>

#define VERIFY_INPUTMETHOD_HINTS(actual, expect) \
    QVERIFY(actual == expect);

class tst_QWebView : public QObject
{
    Q_OBJECT

public Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

private Q_SLOTS:
    void renderHints();
    void getWebKitVersion();

    void reusePage();
    void microFocusCoordinates();
    void focusInputTypes();
    void horizontalScrollbarTest();

#if !(defined(USE_QT_MOBILE_THEME) && USE_QT_MOBILE_THEME)
    void setPalette_data();
    void setPalette();
#endif
};

// This will be called before the first test function is executed.
// It is only called once.
void tst_QWebView::initTestCase()
{
}

// This will be called after the last test function is executed.
// It is only called once.
void tst_QWebView::cleanupTestCase()
{
}

// This will be called before each test function is executed.
void tst_QWebView::init()
{
}

// This will be called after every test function.
void tst_QWebView::cleanup()
{
}

void tst_QWebView::renderHints()
{
    QWebView webView;

    // default is only text antialiasing + smooth pixmap transform
    QVERIFY(!(webView.renderHints() & QPainter::Antialiasing));
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(webView.renderHints() & QPainter::SmoothPixmapTransform);

    webView.setRenderHint(QPainter::Antialiasing, true);
    QVERIFY(webView.renderHints() & QPainter::Antialiasing);
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(webView.renderHints() & QPainter::SmoothPixmapTransform);

    webView.setRenderHint(QPainter::Antialiasing, false);
    QVERIFY(!(webView.renderHints() & QPainter::Antialiasing));
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(webView.renderHints() & QPainter::SmoothPixmapTransform);

    webView.setRenderHint(QPainter::SmoothPixmapTransform, true);
    QVERIFY(!(webView.renderHints() & QPainter::Antialiasing));
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(webView.renderHints() & QPainter::SmoothPixmapTransform);

    webView.setRenderHint(QPainter::SmoothPixmapTransform, false);
    QVERIFY(webView.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(!(webView.renderHints() & QPainter::SmoothPixmapTransform));
}

void tst_QWebView::getWebKitVersion()
{
    QVERIFY(qWebKitVersion().toDouble() > 0);
}

// A QWebPage outlives the QWebView it was set on, and can be handed to a second
// view; tearing both down must not crash or double-free the page.
void tst_QWebView::reusePage()
{
    QPointer<QWebPage> page = new QWebPage;

    QWebView* view1 = new QWebView;
    view1->setPage(page.data());
    page.data()->mainFrame()->setHtml("<html><body id='b'>text</body></html>");
    view1->show();
    QTest::qWaitForWindowExposed(view1);
    delete view1;
    QVERIFY(page != 0); // deleting the view must not have deleted the page

    QWebView* view2 = new QWebView;
    view2->setPage(page.data());
    view2->show();
    QTest::qWaitForWindowExposed(view2);
    delete view2;

    delete page.data(); // must not crash
}

void tst_QWebView::microFocusCoordinates()
{
    QWebPage* page = new QWebPage;
    QWebView* webView = new QWebView;
    webView->setPage( page );

    page->mainFrame()->setHtml("<html><body>" \
        "<input type='text' id='input1' style='font--family: serif' value='' maxlength='20'/><br>" \
        "<canvas id='canvas1' width='500' height='500'></canvas>" \
        "<input type='password'/><br>" \
        "<canvas id='canvas2' width='500' height='500'></canvas>" \
        "</body></html>");

    page->mainFrame()->setFocus();

    QVariant initialMicroFocus = page->inputMethodQuery(Qt::ImCursorRectangle);
    QVERIFY(initialMicroFocus.isValid());

    page->mainFrame()->scroll(0,50);

    QVariant currentMicroFocus = page->inputMethodQuery(Qt::ImCursorRectangle);
    QVERIFY(currentMicroFocus.isValid());

    QCOMPARE(initialMicroFocus.toRect().translated(QPoint(0,-50)), currentMicroFocus.toRect());
}

void tst_QWebView::focusInputTypes()
{
    QWebView webView;
    webView.show();
    QTest::qWaitForWindowExposed(&webView);

    QWebFrame* const mainFrame = webView.page()->mainFrame();
    mainFrame->setHtml(QLatin1String(
        "<html><body>"
        "<input type='text' maxlength='20' style='position:absolute;left:10px;top:0px;height:50px;width:100px;'/><br>"
        "<input type='password' style='position:absolute;left:10px;top:50px;height:50px;width:100px;'/><br>"
        "<input type='tel' style='position:absolute;left:10px;top:100px;height:50px;width:100px;'/><br>"
        "<input type='number' style='position:absolute;left:10px;top:150px;height:50px;width:100px;'/><br>"
        "<input type='email' style='position:absolute;left:10px;top:200px;height:50px;width:100px;'/><br>"
        "<input type='url' style='position:absolute;left:10px;top:250px;height:50px;width:100px;'/><br>"
        "<textarea style='position:absolute;left:10px;top:310px;height:50px;width:100px;' rows='2' cols='20'>blah blah</textarea>"
        "</body></html>"));
    mainFrame->setFocus();

    // 'text' type
    QWebElement inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=text]"));
    QTest::mouseClick(&webView, Qt::LeftButton, Qt::NoModifier, inputElement.geometry().center());
    QVERIFY(webView.inputMethodHints() == Qt::ImhNone);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'password' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=password]"));
    QTest::mouseClick(&webView, Qt::LeftButton, Qt::NoModifier, inputElement.geometry().center());
    VERIFY_INPUTMETHOD_HINTS(webView.inputMethodHints(), Qt::ImhHiddenText);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'tel' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=tel]"));
    QTest::mouseClick(&webView, Qt::LeftButton, Qt::NoModifier, inputElement.geometry().center());
    VERIFY_INPUTMETHOD_HINTS(webView.inputMethodHints(), Qt::ImhDialableCharactersOnly);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'number' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=number]"));
    QTest::mouseClick(&webView, Qt::LeftButton, Qt::NoModifier, inputElement.geometry().center());
    VERIFY_INPUTMETHOD_HINTS(webView.inputMethodHints(), Qt::ImhDigitsOnly);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'email' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=email]"));
    QTest::mouseClick(&webView, Qt::LeftButton, Qt::NoModifier, inputElement.geometry().center());
    VERIFY_INPUTMETHOD_HINTS(webView.inputMethodHints(), Qt::ImhEmailCharactersOnly);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'url' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=url]"));
    QTest::mouseClick(&webView, Qt::LeftButton, Qt::NoModifier, inputElement.geometry().center());
    VERIFY_INPUTMETHOD_HINTS(webView.inputMethodHints(), Qt::ImhUrlCharactersOnly);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'password' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=password]"));
    QTest::mouseClick(&webView, Qt::LeftButton, Qt::NoModifier, inputElement.geometry().center());
    VERIFY_INPUTMETHOD_HINTS(webView.inputMethodHints(), Qt::ImhHiddenText);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'text' type
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=text]"));
    QTest::mouseClick(&webView, Qt::LeftButton, Qt::NoModifier, inputElement.geometry().center());
    QVERIFY(webView.inputMethodHints() == Qt::ImhNone);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'password' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("input[type=password]"));
    QTest::mouseClick(&webView, Qt::LeftButton, Qt::NoModifier, inputElement.geometry().center());
    VERIFY_INPUTMETHOD_HINTS(webView.inputMethodHints(), Qt::ImhHiddenText);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));

    // 'text area' field
    inputElement = mainFrame->documentElement().findFirst(QLatin1String("textarea"));
    QTest::mouseClick(&webView, Qt::LeftButton, Qt::NoModifier, inputElement.geometry().center());
    QVERIFY(webView.inputMethodHints() == Qt::ImhNone);
    QVERIFY(webView.testAttribute(Qt::WA_InputMethodEnabled));
}

void tst_QWebView::horizontalScrollbarTest()
{
    QWebView webView;
    webView.resize(600, 600);
    webView.show();
    QTest::qWaitForWindowExposed(&webView);

    QWebFrame* const mainFrame = webView.page()->mainFrame();
    mainFrame->setHtml(QLatin1String(
        "<html><body><div style='width: 1000px; height: 1000px; background-color: green'></div></body></html>"));
    mainFrame->setFocus();

    QVERIFY(webView.page()->mainFrame()->scrollPosition() == QPoint(0, 0));

    // Note: The test below assumes that the layout direction is Qt::LeftToRight.
    QTest::mouseClick(&webView, Qt::LeftButton, Qt::NoModifier, QPoint(550, 595));
    QVERIFY(webView.page()->mainFrame()->scrollPosition().x() > 0);

    // Note: The test below assumes that the layout direction is Qt::LeftToRight.
    QTest::mouseClick(&webView, Qt::LeftButton, Qt::NoModifier, QPoint(20, 595));
    QVERIFY(webView.page()->mainFrame()->scrollPosition() == QPoint(0, 0));
}


#if !(defined(USE_QT_MOBILE_THEME) && USE_QT_MOBILE_THEME)
void tst_QWebView::setPalette_data()
{
    QTest::addColumn<bool>("active");
    QTest::addColumn<bool>("background");
    QTest::newRow("activeBG") << true << true;
    QTest::newRow("activeFG") << true << false;
    QTest::newRow("inactiveBG") << false << true;
    QTest::newRow("inactiveFG") << false << false;
}

// Render a QWebView to a QImage twice, each time with a different palette set,
// verify that images rendered are not the same, confirming WebCore usage of
// custom palette on selections.
void tst_QWebView::setPalette()
{
    QString html = "<html><head></head>"
                   "<body>"
                   "Some text here"
                   "</body>"
                   "</html>";

    QFETCH(bool, active);
    QFETCH(bool, background);

    QWidget* activeView = 0;

    // Use controlView to manage active/inactive state of test views by raising
    // or lowering their position in the window stack.
    QWebView controlView;
    controlView.setHtml(html);

    QWebView view1;

    QPalette palette1;
    QBrush brush1(Qt::red);
    brush1.setStyle(Qt::SolidPattern);
    if (active && background) {
        // Rendered image must have red background on an active QWebView.
        palette1.setBrush(QPalette::Active, QPalette::Highlight, brush1);
    } else if (active && !background) {
        // Rendered image must have red foreground on an active QWebView.
        palette1.setBrush(QPalette::Active, QPalette::HighlightedText, brush1);
    } else if (!active && background) {
        // Rendered image must have red background on an inactive QWebView.
        palette1.setBrush(QPalette::Inactive, QPalette::Highlight, brush1);
    } else if (!active && !background) {
        // Rendered image must have red foreground on an inactive QWebView.
        palette1.setBrush(QPalette::Inactive, QPalette::HighlightedText, brush1);
    }

    view1.setPalette(palette1);
    view1.setHtml(html);
    view1.page()->setViewportSize(view1.page()->currentFrame()->contentsSize());
    view1.show();

    QTest::qWaitForWindowExposed(&view1);

    if (!active) {
        controlView.show();
        QTest::qWaitForWindowExposed(&controlView);
        activeView = &controlView;
        controlView.activateWindow();
    } else {
        view1.activateWindow();
        activeView = &view1;
    }

    QTRY_COMPARE(QApplication::activeWindow(), activeView);

    view1.page()->triggerAction(QWebPage::SelectAll);

    QImage img1(view1.page()->viewportSize(), QImage::Format_ARGB32);
    QPainter painter1(&img1);
    view1.page()->currentFrame()->render(&painter1);
    painter1.end();
    view1.close();
    controlView.close();

    QWebView view2;

    QPalette palette2;
    QBrush brush2(Qt::blue);
    brush2.setStyle(Qt::SolidPattern);
    if (active && background) {
        // Rendered image must have blue background on an active QWebView.
        palette2.setBrush(QPalette::Active, QPalette::Highlight, brush2);
    } else if (active && !background) {
        // Rendered image must have blue foreground on an active QWebView.
        palette2.setBrush(QPalette::Active, QPalette::HighlightedText, brush2);
    } else if (!active && background) {
        // Rendered image must have blue background on an inactive QWebView.
        palette2.setBrush(QPalette::Inactive, QPalette::Highlight, brush2);
    } else if (!active && !background) {
        // Rendered image must have blue foreground on an inactive QWebView.
        palette2.setBrush(QPalette::Inactive, QPalette::HighlightedText, brush2);
    }

    view2.setPalette(palette2);
    view2.setHtml(html);
    view2.page()->setViewportSize(view2.page()->currentFrame()->contentsSize());
    view2.show();

    QTest::qWaitForWindowExposed(&view2);

    if (!active) {
        controlView.show();
        QTest::qWaitForWindowExposed(&controlView);
        activeView = &controlView;
        controlView.activateWindow();
    } else {
        view2.activateWindow();
        activeView = &view2;
    }

    QTRY_COMPARE(QApplication::activeWindow(), activeView);

    view2.page()->triggerAction(QWebPage::SelectAll);

    QImage img2(view2.page()->viewportSize(), QImage::Format_ARGB32);
    QPainter painter2(&img2);
    view2.page()->currentFrame()->render(&painter2);
    painter2.end();

    view2.close();
    controlView.close();

    QVERIFY(img1 != img2);
}
#endif

QTEST_MAIN(tst_QWebView)
#include "tst_qwebview.moc"

