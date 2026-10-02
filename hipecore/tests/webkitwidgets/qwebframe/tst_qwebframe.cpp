/*
    Copyright (C) 2008,2009 Nokia Corporation and/or its subsidiary(-ies)

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


#include <QtTest/QtTest>

#include <qwebpage.h>
#include <qwebelement.h>
#include <qwebview.h>
#include <qwebframe.h>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QPaintEngine>
#include <QPicture>
#include <QTextCodec>
#include "../util.h"

class tst_QWebFrame : public QObject
{
    Q_OBJECT

public:
    bool eventFilter(QObject* watched, QEvent* event);

public Q_SLOTS:
    void init();
    void cleanup();

private Q_SLOTS:
    // URL navigation, load-progress/started/finished signals, the JS window-object
    // bridge and resource loading are all gone from hipecore, so the setUrl*/load*/
    // javaScriptWindowObjectCleared*/setHtmlWith*Resource tests were removed.
    void setHtml();
    void metaData();
#if !defined(QT_NO_COMBOBOX)
    void popupFocus();
#endif
    void inputFieldFocus();
    void hitTestContent();
    void hasSetFocus();
    void renderGeometry();
    void renderHints();
    void scrollPosition();
    void scrollToAnchor();
    void scrollbarsOff();
    void setContent_data();
    void setContent();

private:
    QWebView* m_view { nullptr };
    QWebPage* m_page { nullptr };
    QWebView* m_inputFieldsTestView { nullptr };
    int m_inputFieldTestPaintCount { 0 };
};

bool tst_QWebFrame::eventFilter(QObject* watched, QEvent* event)
{
    // used on the inputFieldFocus test
    if (watched == m_inputFieldsTestView) {
        if (event->type() == QEvent::Paint)
            m_inputFieldTestPaintCount++;
    }
    return QObject::eventFilter(watched, event);
}

void tst_QWebFrame::init()
{
    m_view = new QWebView();
    m_page = m_view->page();
}

void tst_QWebFrame::cleanup()
{
    delete m_view;
}

void tst_QWebFrame::setHtml()
{
    QString html("<html><head></head><body><p>hello world</p></body></html>");
    QSignalSpy spy(m_view->page(), SIGNAL(loadFinished(bool)));
    m_view->page()->mainFrame()->setHtml(html);
    QCOMPARE(m_view->page()->mainFrame()->toHtml(), html);
    QCOMPARE(spy.count(), 1);
}

void tst_QWebFrame::metaData()
{
    m_view->setHtml("<html>"
                    "    <head>"
                    "        <meta name=\"description\" content=\"Test description\">"
                    "        <meta name=\"keywords\" content=\"HTML, JavaScript, Css\">"
                    "    </head>"
                    "</html>");

    QMultiMap<QString, QString> metaData = m_view->page()->mainFrame()->metaData();

    QCOMPARE(metaData.count(), 2);

    QCOMPARE(metaData.value("description"), QString("Test description"));
    QCOMPARE(metaData.value("keywords"), QString("HTML, JavaScript, Css"));
    QCOMPARE(metaData.value("nonexistant"), QString());

    m_view->setHtml("<html>"
                    "    <head>"
                    "        <meta name=\"samekey\" content=\"FirstValue\">"
                    "        <meta name=\"samekey\" content=\"SecondValue\">"
                    "    </head>"
                    "</html>");

    metaData = m_view->page()->mainFrame()->metaData();

    QCOMPARE(metaData.count(), 2);

    QStringList values = metaData.values("samekey");
    QCOMPARE(values.count(), 2);

    QVERIFY(values.contains("FirstValue"));
    QVERIFY(values.contains("SecondValue"));

    QCOMPARE(metaData.value("nonexistant"), QString());
}

#if !defined(QT_NO_COMBOBOX)
void tst_QWebFrame::popupFocus()
{
    QWebView view;
    view.setHtml("<html>"
                 "    <body>"
                 "        <select name=\"select\">"
                 "            <option>1</option>"
                 "            <option>2</option>"
                 "        </select>"
                 "        <input type=\"text\"> </input>"
                 "        <textarea name=\"text_area\" rows=\"3\" cols=\"40\">"
                 "This test checks whether showing and hiding a popup"
                 "takes the focus away from the webpage."
                 "        </textarea>"
                 "    </body>"
                 "</html>");
    view.resize(400, 100);
    // Call setFocus before show to work around http://bugreports.qt.nokia.com/browse/QTBUG-14762
    view.setFocus();
    view.show();
    QTest::qWaitForWindowExposed(&view);
    view.activateWindow();
    QTRY_VERIFY(view.hasFocus());

    // open the popup by clicking. check if focus is on the popup
    const QWebElement webCombo = view.page()->mainFrame()->documentElement().findFirst(QLatin1String("select[name=select]"));
    QTest::mouseClick(&view, Qt::LeftButton, 0, webCombo.geometry().center());

    QComboBox* combo = view.findChild<QComboBox*>();
    QVERIFY(combo != 0);
    QTRY_VERIFY(!view.hasFocus() && combo->view()->hasFocus()); // Focus should be on the popup

    // hide the popup and check if focus is on the page
    combo->hidePopup();
    QTRY_VERIFY(view.hasFocus()); // Focus should be back on the WebView
}
#endif

void tst_QWebFrame::inputFieldFocus()
{
    QWebView view;
    view.setHtml("<html><body><input type=\"text\"></input></body></html>");
    view.resize(400, 100);
    view.show();
    QTest::qWaitForWindowExposed(&view);
    view.activateWindow();
    view.setFocus();
    QTRY_VERIFY(view.hasFocus());

    // double the flashing time, should at least blink once already
    int delay = qApp->cursorFlashTime() * 2;

    // focus the lineedit and check if it blinks
    bool autoSipEnabled = qApp->autoSipEnabled();
    qApp->setAutoSipEnabled(false);
    const QWebElement inputElement = view.page()->mainFrame()->documentElement().findFirst(QLatin1String("input[type=text]"));
    QTest::mouseClick(&view, Qt::LeftButton, 0, inputElement.geometry().center());
    m_inputFieldsTestView = &view;
    view.installEventFilter( this );
    QTest::qWait(delay);
    QVERIFY2(m_inputFieldTestPaintCount >= 3,
             "The input field should have a blinking caret");
    qApp->setAutoSipEnabled(autoSipEnabled);
}

void tst_QWebFrame::hitTestContent()
{
    QString html("<html><body><p>A paragraph</p><br/><br/><br/><a href=\"about:blank\" target=\"_foo\" id=\"link\">link text</a></body></html>");

    QWebPage page;
    QWebFrame* frame = page.mainFrame();
    frame->setHtml(html);
    page.setViewportSize(QSize(200, 0)); //no height so link is not visible
    const QWebElement linkElement = frame->documentElement().findFirst(QLatin1String("a#link"));
    QWebHitTestResult result = frame->hitTestContent(linkElement.geometry().center());
    QCOMPARE(result.linkText(), QString("link text"));
    QWebElement link = result.linkElement();
    QCOMPARE(link.attribute("target"), QString("_foo"));
    QCOMPARE(result.element().tagName(), QString("A"));
}

void tst_QWebFrame::hasSetFocus()
{
    QString html("<html><body><p>top</p>" \
                    "<iframe width='80%' height='30%'/>" \
                 "</body></html>");

    QSignalSpy loadSpy(m_page, SIGNAL(loadFinished(bool)));
    m_page->mainFrame()->setHtml(html);

    waitForSignal(m_page->mainFrame(), SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(loadSpy.size(), 1);

    QList<QWebFrame*> children = m_page->mainFrame()->childFrames();
    QWebFrame* frame = children.at(0);
    QString innerHtml("<html><body><p>another iframe</p>" \
                        "<iframe width='80%' height='30%'/>" \
                      "</body></html>");
    frame->setHtml(innerHtml);

    waitForSignal(frame, SIGNAL(loadFinished(bool)), 200);
    QCOMPARE(loadSpy.size(), 2);

    m_page->mainFrame()->setFocus();
    QTRY_VERIFY(m_page->mainFrame()->hasFocus());

    for (int i = 0; i < children.size(); ++i) {
        children.at(i)->setFocus();
        QTRY_VERIFY(children.at(i)->hasFocus());
        QVERIFY(!m_page->mainFrame()->hasFocus());
    }

    m_page->mainFrame()->setFocus();
    QTRY_VERIFY(m_page->mainFrame()->hasFocus());
}

void tst_QWebFrame::renderGeometry()
{
    QString html("<html>" \
                    "<head><style>" \
                       "body, iframe { margin: 0px; border: none; }" \
                    "</style></head>" \
                    "<body><iframe width='100px' height='100px'/></body>" \
                 "</html>");

    QWebPage page;
    page.mainFrame()->setHtml(html);

    QList<QWebFrame*> frames = page.mainFrame()->childFrames();
    QWebFrame *frame = frames.at(0);
    // A block larger than the 100x100 iframe, so the contents layer overflows
    // and the iframe gets scrollbars. (Was an <img>; resource loads are gone.)
    QString innerHtml("<body style='margin: 0px;'>"
                      "<div style='width: 128px; height: 128px; background: #000;'></div></body>");
    frame->setHtml(innerHtml);

    QPicture picture;

    QSize size = page.mainFrame()->contentsSize();
    page.setViewportSize(size);

    // render contents layer only (the iframe is smaller than the image, so it will have scrollbars)
    QPainter painter1(&picture);
    frame->render(&painter1, QWebFrame::ContentsLayer);
    painter1.end();

    QCOMPARE(size.width(), picture.boundingRect().width() + frame->scrollBarGeometry(Qt::Vertical).width());
    QCOMPARE(size.height(), picture.boundingRect().height() + frame->scrollBarGeometry(Qt::Horizontal).height());

    // render everything, should be the size of the iframe
    QPainter painter2(&picture);
    frame->render(&painter2, QWebFrame::AllLayers);
    painter2.end();

    QCOMPARE(size.width(), picture.boundingRect().width());   // width: 100px
    QCOMPARE(size.height(), picture.boundingRect().height()); // height: 100px
}


class DummyPaintEngine: public QPaintEngine {
public:

    DummyPaintEngine()
        : QPaintEngine(QPaintEngine::AllFeatures)
        , renderHints(0)
    {
    }

    bool begin(QPaintDevice*)
    {
        setActive(true);
        return true;
    }

    bool end()
    {
        setActive(false);
        return false;
    }

    void updateState(const QPaintEngineState& state)
    {
        renderHints = state.renderHints();
    }

    void drawPath(const QPainterPath&) { }
    void drawPixmap(const QRectF&, const QPixmap&, const QRectF&) { }

    QPaintEngine::Type type() const
    {
        return static_cast<QPaintEngine::Type>(QPaintEngine::User + 2);
    }

    QPainter::RenderHints renderHints;
};

class DummyPaintDevice: public QPaintDevice {
public:
    DummyPaintDevice()
        : QPaintDevice()
        , m_engine(new DummyPaintEngine)
    {
    }

    ~DummyPaintDevice()
    {
        delete m_engine;
    }

    QPaintEngine* paintEngine() const
    {
        return m_engine;
    }

    QPainter::RenderHints renderHints() const
    {
        return m_engine->renderHints;
    }

protected:
    int metric(PaintDeviceMetric metric) const;

private:
    DummyPaintEngine* m_engine;
    friend class DummyPaintEngine;
};


int DummyPaintDevice::metric(PaintDeviceMetric metric) const
{
    switch (metric) {
    case PdmWidth:
        return 400;
        break;

    case PdmHeight:
        return 200;
        break;

    case PdmNumColors:
        return INT_MAX;
        break;

    case PdmDepth:
        return 32;
        break;

    default:
        break;
    }
    return 0;
}

void tst_QWebFrame::renderHints()
{
    QString html("<html><body><p>Hello, world!</p></body></html>");

    QWebPage page;
    page.mainFrame()->setHtml(html);
    page.setViewportSize(page.mainFrame()->contentsSize());

    // We will call frame->render and trap the paint engine state changes
    // to ensure that GraphicsContext does not clobber the render hints.
    DummyPaintDevice buffer;
    QPainter painter(&buffer);

    painter.setRenderHint(QPainter::TextAntialiasing, false);
    page.mainFrame()->render(&painter);
    QVERIFY(!(buffer.renderHints() & QPainter::TextAntialiasing));
    QVERIFY(!(buffer.renderHints() & QPainter::SmoothPixmapTransform));
    QVERIFY(!(buffer.renderHints() & QPainter::HighQualityAntialiasing));

    painter.setRenderHint(QPainter::TextAntialiasing, true);
    page.mainFrame()->render(&painter);
    QVERIFY(buffer.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(!(buffer.renderHints() & QPainter::SmoothPixmapTransform));
    QVERIFY(!(buffer.renderHints() & QPainter::HighQualityAntialiasing));

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    page.mainFrame()->render(&painter);
    QVERIFY(buffer.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(buffer.renderHints() & QPainter::SmoothPixmapTransform);
    QVERIFY(!(buffer.renderHints() & QPainter::HighQualityAntialiasing));

    painter.setRenderHint(QPainter::HighQualityAntialiasing, true);
    page.mainFrame()->render(&painter);
    QVERIFY(buffer.renderHints() & QPainter::TextAntialiasing);
    QVERIFY(buffer.renderHints() & QPainter::SmoothPixmapTransform);
    QVERIFY(buffer.renderHints() & QPainter::HighQualityAntialiasing);
}

void tst_QWebFrame::scrollPosition()
{
    // oversized content in a small viewport, to provoke the scrollbars to appear
    QString html("<html><body><div style='width: 500px; height: 500px;'></div></body></html>");

    QWebPage page;
    page.setViewportSize(QSize(200, 200));

    QWebFrame* frame = page.mainFrame();
    frame->setHtml(html);
    frame->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
    frame->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);

    // try to set the scroll offset programmatically
    frame->setScrollPosition(QPoint(23, 29));
    QCOMPARE(frame->scrollPosition().x(), 23);
    QCOMPARE(frame->scrollPosition().y(), 29);
}

void tst_QWebFrame::scrollToAnchor()
{
    QWebPage page;
    page.setViewportSize(QSize(480, 800));
    QWebFrame* frame = page.mainFrame();

    QString html("<html><body><p style=\"margin-bottom: 1500px;\">Hello.</p>"
                 "<p><a id=\"foo\">This</a> is an anchor</p>"
                 "<p style=\"margin-bottom: 1500px;\"><a id=\"bar\">This</a> is another anchor</p>"
                 "</body></html>");
    frame->setHtml(html);
    frame->setScrollPosition(QPoint(0, 0));
    QCOMPARE(frame->scrollPosition().x(), 0);
    QCOMPARE(frame->scrollPosition().y(), 0);

    QWebElement fooAnchor = frame->findFirstElement("a[id=foo]");

    frame->scrollToAnchor("foo");
    QCOMPARE(frame->scrollPosition().y(), fooAnchor.geometry().top());

    frame->scrollToAnchor("bar");
    frame->scrollToAnchor("foo");
    QCOMPARE(frame->scrollPosition().y(), fooAnchor.geometry().top());

    frame->scrollToAnchor("top");
    QCOMPARE(frame->scrollPosition().y(), 0);

    frame->scrollToAnchor("bar");
    frame->scrollToAnchor("notexist");
    QVERIFY(frame->scrollPosition().y() != 0);
}


void tst_QWebFrame::scrollbarsOff()
{
    QWebView view;
    QWebFrame* mainFrame = view.page()->mainFrame();

    mainFrame->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
    mainFrame->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);

    view.resize(200, 200);

    // Content much larger than the viewport: with ScrollBarAlwaysOff no
    // scrollbar should be laid out in either direction.
    view.setHtml("<body><div style='width: 1000px; height: 1000px;'></div></body>");

    QVERIFY(mainFrame->scrollBarGeometry(Qt::Horizontal).isEmpty());
    QVERIFY(mainFrame->scrollBarGeometry(Qt::Vertical).isEmpty());
}

void tst_QWebFrame::setContent_data()
{
    QTest::addColumn<QString>("mimeType");
    QTest::addColumn<QByteArray>("testContents");
    QTest::addColumn<QString>("expected");

    QString str = QString::fromUtf8("ὕαλον ϕαγεῖν δύναμαι· τοῦτο οὔ με βλάπτει");
    QTest::newRow("UTF-8 plain text") << "text/plain; charset=utf-8" << str.toUtf8() << str;

    QTextCodec *utf16 = QTextCodec::codecForName("UTF-16");
    if (utf16)
        QTest::newRow("UTF-16 plain text") << "text/plain; charset=utf-16" << utf16->fromUnicode(str) << str;

    str = QString::fromUtf8("Une chaîne de caractères à sa façon.");
    QTest::newRow("latin-1 plain text") << "text/plain; charset=iso-8859-1" << str.toLatin1() << str;


}

void tst_QWebFrame::setContent()
{
    QFETCH(QString, mimeType);
    QFETCH(QByteArray, testContents);
    QFETCH(QString, expected);
    m_view->setContent(testContents, mimeType);
    QWebFrame* mainFrame = m_view->page()->mainFrame();
    QCOMPARE(expected , mainFrame->toPlainText());
}


QTEST_MAIN(tst_QWebFrame)
#include "tst_qwebframe.moc"
