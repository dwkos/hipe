/*
    Copyright (C) 2009 Jakub Wieczorek <faw217@gmail.com>

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
#include <QtTest/QtTest>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QStyleOptionGraphicsItem>
#include <qgraphicswebview.h>
#include <qwebpage.h>
#include <qwebframe.h>


class tst_QGraphicsWebView : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void qgraphicswebview();
    void crashOnViewlessWebPages();
    void microFocusCoordinates();
    void focusInputTypes();
    void crashOnSetScaleBeforeSetUrl();
    void horizontalScrollbarTest();

#if !(defined(USE_QT_MOBILE_THEME) && USE_QT_MOBILE_THEME)
    void setPalette_data();
    void setPalette();
#endif
    void renderHints();
};

void tst_QGraphicsWebView::qgraphicswebview()
{
    QGraphicsWebView item;
    item.url();
    item.title();
    item.zoomFactor();
    item.settings();
    item.page();
    item.setPage(0);
    item.page();
    item.setUrl(QUrl());
    item.setZoomFactor(0);
    item.load(QUrl());
    item.setHtml(QString());
    item.setContent(QByteArray());
    item.isModified();
}

class WebPage : public QWebPage
{
    Q_OBJECT

public:
    WebPage(QObject* parent = 0): QWebPage(parent)
    {
    }

    QGraphicsWebView* webView;

private Q_SLOTS:
    // Force a webview deletion during the load.
    // It should not cause WebPage to crash due to
    // it accessing invalid pageClient pointer.
    void aborting()
    {
        delete webView;
    }
};

class GraphicsWebView : public QGraphicsWebView
{
    Q_OBJECT

public:
    GraphicsWebView(QGraphicsItem* parent = 0): QGraphicsWebView(parent)
    {
    }

    void fireMouseClick(QPointF point) {
        QGraphicsSceneMouseEvent presEv(QEvent::GraphicsSceneMousePress);
        presEv.setPos(point);
        presEv.setButton(Qt::LeftButton);
        presEv.setButtons(Qt::LeftButton);
        QGraphicsSceneMouseEvent relEv(QEvent::GraphicsSceneMouseRelease);
        relEv.setPos(point);
        relEv.setButton(Qt::LeftButton);
        relEv.setButtons(Qt::LeftButton);
        QGraphicsWebView::sceneEvent(&presEv);
        QGraphicsWebView::sceneEvent(&relEv);
    }
};

void tst_QGraphicsWebView::crashOnViewlessWebPages()
{
    QGraphicsScene scene;
    QGraphicsView view(&scene);

    QGraphicsWebView* webView = new QGraphicsWebView;
    WebPage* page = new WebPage;
    webView->setPage(page);
    page->webView = webView;
    scene.addItem(webView);

    view.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    view.resize(600, 480);
    webView->resize(view.geometry().size());

    QCoreApplication::processEvents();
    view.show();

    // Resizing the page will resize and layout the empty "about:blank"
    // page, so we first connect the signal afterward.
    connect(page->mainFrame(), SIGNAL(initialLayoutCompleted()), page, SLOT(aborting()));

    page->mainFrame()->load(QUrl("data:text/html,"
                                 "<frameset cols=\"25%,75%\">"
                                     "<frame src=\"data:text/html,foo \">"
                                     "<frame src=\"data:text/html,bar\">"
                                 "</frameset>"));

    waitForSignal(page, SIGNAL(loadFinished(bool)), 1000);
    delete page; // must not crash
}

void tst_QGraphicsWebView::crashOnSetScaleBeforeSetUrl()
{
    QGraphicsWebView* webView = new QGraphicsWebView;
    webView->setScale(2.0);
    delete webView;
}

void tst_QGraphicsWebView::microFocusCoordinates()
{
    QWebPage* page = new QWebPage;
    QGraphicsWebView* webView = new QGraphicsWebView;
    webView->setPage( page );
    QGraphicsView* view = new QGraphicsView;
    QGraphicsScene* scene = new QGraphicsScene(view);
    view->setScene(scene);
    scene->addItem(webView);
    view->setGeometry(QRect(0,0,500,500));

    page->mainFrame()->setHtml("<html><body>" \
        "<input type='text' id='input1' style='font--family: serif' value='' maxlength='20'/><br>" \
        "<canvas id='canvas1' width='500' height='500'></canvas>" \
        "<input type='password'/><br>" \
        "<canvas id='canvas2' width='500' height='500'></canvas>" \
        "</body></html>");

    page->mainFrame()->setFocus();

    QVariant initialMicroFocus = page->inputMethodQuery(Qt::ImCursorRectangle);
    QVERIFY(initialMicroFocus.isValid());

    page->mainFrame()->scroll(0,300);

    QVariant currentMicroFocus = page->inputMethodQuery(Qt::ImCursorRectangle);
    QVERIFY(currentMicroFocus.isValid());

    QCOMPARE(initialMicroFocus.toRect().translated(QPoint(0,-300)), currentMicroFocus.toRect());

    delete view;
}

void tst_QGraphicsWebView::focusInputTypes()
{
    QWebPage* page = new QWebPage;
    GraphicsWebView* webView = new GraphicsWebView;
    webView->setPage( page );
    QGraphicsView* view = new QGraphicsView;
    QGraphicsScene* scene = new QGraphicsScene(view);
    view->setScene(scene);
    scene->addItem(webView);
    view->setGeometry(QRect(0,0,500,500));
    QCoreApplication::processEvents();
    page->mainFrame()->setHtml(QLatin1String(
        "<html><body>"
        "<input type='text' maxlength='20' style='position:absolute;left:10px;top:0px;height:50px;width:100px;'/><br>"
        "<input type='password' style='position:absolute;left:10px;top:50px;height:50px;width:100px;'/><br>"
        "<input type='tel' style='position:absolute;left:10px;top:100px;height:50px;width:100px;'/><br>"
        "<input type='number' style='position:absolute;left:10px;top:150px;height:50px;width:100px;'/><br>"
        "<input type='email' style='position:absolute;left:10px;top:200px;height:50px;width:100px;'/><br>"
        "<input type='url' style='position:absolute;left:10px;top:250px;height:50px;width:100px;'/>"
        "</body></html>"));
    page->mainFrame()->setFocus();

    // 'text' type
    webView->fireMouseClick(QPointF(20.0, 10.0));
    QVERIFY(webView->inputMethodHints() == Qt::ImhNone);

    // 'password' field
    webView->fireMouseClick(QPointF(20.0, 60.0));
    QVERIFY(webView->inputMethodHints() & Qt::ImhHiddenText);

    // 'tel' field
    webView->fireMouseClick(QPointF(20.0, 110.0));
    QVERIFY(webView->inputMethodHints() & Qt::ImhDialableCharactersOnly);

    // 'number' field
    webView->fireMouseClick(QPointF(20.0, 160.0));
    QVERIFY(webView->inputMethodHints() & Qt::ImhDigitsOnly);

    // 'email' field
    webView->fireMouseClick(QPointF(20.0, 210.0));
    QVERIFY(webView->inputMethodHints() & Qt::ImhEmailCharactersOnly);

    // 'url' field
    webView->fireMouseClick(QPointF(20.0, 260.0));
    QVERIFY(webView->inputMethodHints() & Qt::ImhUrlCharactersOnly);

    delete webView;
    delete view;
}

#if !(defined(USE_QT_MOBILE_THEME) && USE_QT_MOBILE_THEME)
void tst_QGraphicsWebView::setPalette_data()
{
    QTest::addColumn<bool>("active");
    QTest::addColumn<bool>("background");
    QTest::newRow("activeBG") << true << true;
    QTest::newRow("activeFG") << true << false;
    QTest::newRow("inactiveBG") << false << true;
    QTest::newRow("inactiveFG") << false << false;
}

// Render a QGraphicsWebView to a QImage twice, each time with a different palette set,
// verify that images rendered are not the same, confirming WebCore usage of
// custom palette on selections.
void tst_QGraphicsWebView::setPalette()
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
    QGraphicsScene controlScene;
    QGraphicsView controlView(&controlScene);
    QGraphicsWebView controlWebView;
    controlScene.addItem(&controlWebView);
    controlWebView.setHtml(html);
    controlWebView.setGeometry(QRectF(0, 0, 200, 200));

    QGraphicsScene scene1;
    QGraphicsView view1(&scene1);
    view1.setSceneRect(0, 0, 300, 300);
    QGraphicsWebView webView1;
    webView1.setResizesToContents(true);
    scene1.addItem(&webView1);
    webView1.setFocus();

    QPalette palette1;
    QBrush brush1(Qt::red);
    brush1.setStyle(Qt::SolidPattern);
    if (active && background) {
        // Rendered image must have red background on an active QGraphicsWebView.
        palette1.setBrush(QPalette::Active, QPalette::Highlight, brush1);
    } else if (active && !background) {
        // Rendered image must have red foreground on an active QGraphicsWebView.
        palette1.setBrush(QPalette::Active, QPalette::HighlightedText, brush1);
    } else if (!active && background) {
        // Rendered image must have red background on an inactive QGraphicsWebView.
        palette1.setBrush(QPalette::Inactive, QPalette::Highlight, brush1);
    } else if (!active && !background) {
        // Rendered image must have red foreground on an inactive QGraphicsWebView.
        palette1.setBrush(QPalette::Inactive, QPalette::HighlightedText, brush1);
    }

    webView1.setHtml(html);
    view1.resize(webView1.page()->viewportSize());
    webView1.setPalette(palette1);
    view1.show();

    QVERIFY(webView1.palette() == palette1);
    QVERIFY(webView1.page()->palette() == palette1);

    QTest::qWaitForWindowExposed(&view1);

    if (!active) {
        controlView.show();
        QTest::qWaitForWindowExposed(&controlView);
        QApplication::setActiveWindow(&controlView);
        activeView = &controlView;
        controlView.activateWindow();
    } else {
        QApplication::setActiveWindow(&view1);
        view1.activateWindow();
        activeView = &view1;
    }

    QTRY_COMPARE(QApplication::activeWindow(), activeView);

    webView1.page()->triggerAction(QWebPage::SelectAll);

    QImage img1(webView1.page()->viewportSize(), QImage::Format_ARGB32);
    QPainter painter1(&img1);
    webView1.page()->currentFrame()->render(&painter1);
    painter1.end();
    view1.close();
    controlView.close();

    QGraphicsScene scene2;
    QGraphicsView view2(&scene2);
    view2.setSceneRect(0, 0, 300, 300);
    QGraphicsWebView webView2;
    webView2.setResizesToContents(true);
    scene2.addItem(&webView2);
    webView2.setFocus();

    QPalette palette2;
    QBrush brush2(Qt::blue);
    brush2.setStyle(Qt::SolidPattern);
    if (active && background) {
        // Rendered image must have blue background on an active QGraphicsWebView.
        palette2.setBrush(QPalette::Active, QPalette::Highlight, brush2);
    } else if (active && !background) {
        // Rendered image must have blue foreground on an active QGraphicsWebView.
        palette2.setBrush(QPalette::Active, QPalette::HighlightedText, brush2);
    } else if (!active && background) {
        // Rendered image must have blue background on an inactive QGraphicsWebView.
        palette2.setBrush(QPalette::Inactive, QPalette::Highlight, brush2);
    } else if (!active && !background) {
        // Rendered image must have blue foreground on an inactive QGraphicsWebView.
        palette2.setBrush(QPalette::Inactive, QPalette::HighlightedText, brush2);
    }

    webView2.setHtml(html);
    view2.resize(webView2.page()->viewportSize());
    webView2.setPalette(palette2);
    view2.show();

    QTest::qWaitForWindowExposed(&view2);

    if (!active) {
        controlView.show();
        QTest::qWaitForWindowExposed(&controlView);
        QApplication::setActiveWindow(&controlView);
        activeView = &controlView;
        controlView.activateWindow();
    } else {
        QApplication::setActiveWindow(&view2);
        view2.activateWindow();
        activeView = &view2;
    }

    QTRY_COMPARE(QApplication::activeWindow(), activeView);

    webView2.page()->triggerAction(QWebPage::SelectAll);

    QImage img2(webView2.page()->viewportSize(), QImage::Format_ARGB32);
    QPainter painter2(&img2);
    webView2.page()->currentFrame()->render(&painter2);
    painter2.end();

    view2.close();
    controlView.close();

    QVERIFY(img1 != img2);
}
#endif

void tst_QGraphicsWebView::renderHints()
{
    QGraphicsWebView webView;

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

void tst_QGraphicsWebView::horizontalScrollbarTest()
{
    QWebPage* page = new QWebPage;
    GraphicsWebView* webView = new GraphicsWebView;
    webView->setPage(page);
    webView->setGeometry(QRect(0, 0, 600, 600));
    QGraphicsView* view = new QGraphicsView;
    QGraphicsScene* scene = new QGraphicsScene(view);
    view->setScene(scene);
    scene->addItem(webView);

    // Turn off scrolling on the containing QGraphicsView, let the
    // QGraphicsWebView handle the scrolling by itself.
    view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view->show();
    QCoreApplication::processEvents();

    page->mainFrame()->setHtml(QLatin1String(
        "<html><body><div style='width: 1000px; height: 1000px; background-color: green'></div></body></html>"));
    page->mainFrame()->setFocus();

    QVERIFY(webView->page()->mainFrame()->scrollPosition() == QPoint(0, 0));

    // Note: The test below assumes that the layout direction is Qt::LeftToRight.
    webView->fireMouseClick(QPointF(550.0, 590.0));
    QVERIFY(page->mainFrame()->scrollPosition().x() > 0);

    // Note: The test below assumes that the layout direction is Qt::LeftToRight.
    webView->fireMouseClick(QPointF(20.0, 590.0));
    QVERIFY(page->mainFrame()->scrollPosition() == QPoint(0, 0));

    delete webView;
    delete view;
}

QTEST_MAIN(tst_QGraphicsWebView)

#include "tst_qgraphicswebview.moc"
