/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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


#include <QtTest/QtTest>
#include <qwebpage.h>
#include <qwidget.h>
#include <qwebview.h>
#include <qwebframe.h>
#include <qwebelement.h>
#include <util.h>
//TESTED_CLASS=
//TESTED_FILES=

class tst_QWebElement : public QObject
{
    Q_OBJECT

public:
    tst_QWebElement();
    virtual ~tst_QWebElement();

public Q_SLOTS:
    void init();
    void cleanup();

private Q_SLOTS:
    void textHtml();
    void simpleCollection();
    void attributes();
    void attributesNS();
    void listAttributes();
    void classes();
    void namespaceURI();
    void iteration();
    void nonConstIterator();
    void constIterator();
    void foreachManipulation();
    void emptyCollection();
    void appendCollection();
    void documentElement();
    void style();
    void computedStyle();
    void textWidthAndFontMetrics();
    void formControlValue();
    void eventDetailModifiers();
    void appendAndPrepend();
    void appendSvgInside();
    void svgRenderingFidelity();
    void insertBeforeAndAfter();
    void remove();
    void clear();
    void replaceWith();
    void encloseWith();
    void encloseContentsWith();
    void nullSelect();
    void firstChildNextSibling();
    void lastChildPreviousSibling();
    void hasSetFocus();
    void render();
    void addElementToHead();
    void scriptAndNoscriptParsing();
    void xmlParsing();
    void hipeLocation();
    void hipeLocationMarkup();
    void hipeLocationWeak();
    void hipeLocationBindMarkup();
    void appendNewElement();
    void eventReportsCurrentLocation();

private:
    QWebView* m_view { nullptr };
    QWebPage* m_page { nullptr };
    QWebFrame* m_mainFrame { nullptr };
};

tst_QWebElement::tst_QWebElement()
{
}

tst_QWebElement::~tst_QWebElement()
{
}

void tst_QWebElement::init()
{
    m_view = new QWebView();
    m_page = m_view->page();
    m_mainFrame = m_page->mainFrame();
}

void tst_QWebElement::cleanup()
{
    delete m_view;
}

void tst_QWebElement::textHtml()
{
    QString html = "<head></head><body><p>test</p></body>";
    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement();
    QVERIFY(!body.isNull());

    QCOMPARE(body.toPlainText(), QString("test"));
    QCOMPARE(body.toPlainText(), m_mainFrame->toPlainText());

    QCOMPARE(body.toInnerXml(), html);
}

void tst_QWebElement::simpleCollection()
{
    QString html = "<body><p>first para</p><p>second para</p></body>";
    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement();

    QWebElementCollection list = body.findAll("p");
    QCOMPARE(list.count(), 2);
    QCOMPARE(list.at(0).toPlainText(), QString("first para"));
    QCOMPARE(list.at(1).toPlainText(), QString("second para"));
}

void tst_QWebElement::attributes()
{
    m_mainFrame->setHtml("<body><p>Test");
    QWebElement body = m_mainFrame->documentElement();

    QVERIFY(!body.hasAttribute("title"));
    QVERIFY(!body.hasAttributes());

    body.setAttribute("title", "test title");

    QVERIFY(body.hasAttributes());
    QVERIFY(body.hasAttribute("title"));

    QCOMPARE(body.attribute("title"), QString("test title"));

    body.removeAttribute("title");

    QVERIFY(!body.hasAttribute("title"));
    QVERIFY(!body.hasAttributes());

    QCOMPARE(body.attribute("does-not-exist", "testvalue"), QString("testvalue"));
}

void tst_QWebElement::attributesNS()
{
    QString content = "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
                      "xmlns:svg=\"http://www.w3.org/2000/svg\">"
                      "<body><svg:svg id=\"foobar\" width=\"400px\" height=\"300px\">"
                      "</svg:svg></body></html>";

    m_mainFrame->setContent(content.toUtf8(), "application/xhtml+xml");

    QWebElement svg = m_mainFrame->findFirstElement("svg");
    QVERIFY(!svg.isNull());

    QVERIFY(!svg.hasAttributeNS("http://www.w3.org/2000/svg", "foobar"));
    QCOMPARE(svg.attributeNS("http://www.w3.org/2000/svg", "foobar", "defaultblah"), QString("defaultblah"));
    svg.setAttributeNS("http://www.w3.org/2000/svg", "svg:foobar", "true");
    QVERIFY(svg.hasAttributeNS("http://www.w3.org/2000/svg", "foobar"));
    QCOMPARE(svg.attributeNS("http://www.w3.org/2000/svg", "foobar", "defaultblah"), QString("true"));
}

void tst_QWebElement::listAttributes()
{
    QString content = "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
                      "xmlns:svg=\"http://www.w3.org/2000/svg\">"
                      "<body><svg:svg foo=\"\" svg:bar=\"\">"
                      "</svg:svg></body></html>";

    m_mainFrame->setContent(content.toUtf8(), "application/xhtml+xml");

    QWebElement svg = m_mainFrame->findFirstElement("svg");
    QVERIFY(!svg.isNull());

    QVERIFY(svg.attributeNames().contains("foo"));
    QVERIFY(svg.attributeNames("http://www.w3.org/2000/svg").contains("bar"));

    svg.setAttributeNS("http://www.w3.org/2000/svg", "svg:foobar", "true");
    QVERIFY(svg.attributeNames().contains("foo"));
    QStringList attributes = svg.attributeNames("http://www.w3.org/2000/svg");
    QCOMPARE(attributes.size(), 2);
    QVERIFY(attributes.contains("bar"));
    QVERIFY(attributes.contains("foobar"));
}

void tst_QWebElement::classes()
{
    m_mainFrame->setHtml("<body><p class=\"a b c d a c\">Test");

    QWebElement body = m_mainFrame->documentElement();
    QCOMPARE(body.classes().count(), 0);

    QWebElement p = m_mainFrame->documentElement().findAll("p").at(0);
    QStringList classes = p.classes();
    QCOMPARE(classes.count(), 4);
    QCOMPARE(classes[0], QLatin1String("a"));
    QCOMPARE(classes[1], QLatin1String("b"));
    QCOMPARE(classes[2], QLatin1String("c"));
    QCOMPARE(classes[3], QLatin1String("d"));
    QVERIFY(p.hasClass("a"));
    QVERIFY(p.hasClass("b"));
    QVERIFY(p.hasClass("c"));
    QVERIFY(p.hasClass("d"));
    QVERIFY(!p.hasClass("e"));

    p.addClass("f");
    QVERIFY(p.hasClass("f"));
    p.addClass("a");
    QCOMPARE(p.classes().count(), 5);
    QVERIFY(p.hasClass("a"));
    QVERIFY(p.hasClass("b"));
    QVERIFY(p.hasClass("c"));
    QVERIFY(p.hasClass("d"));

    p.toggleClass("a");
    QVERIFY(!p.hasClass("a"));
    QVERIFY(p.hasClass("b"));
    QVERIFY(p.hasClass("c"));
    QVERIFY(p.hasClass("d"));
    QVERIFY(p.hasClass("f"));
    QCOMPARE(p.classes().count(), 4);
    p.toggleClass("f");
    QVERIFY(!p.hasClass("f"));
    QCOMPARE(p.classes().count(), 3);
    p.toggleClass("a");
    p.toggleClass("f");
    QVERIFY(p.hasClass("a"));
    QVERIFY(p.hasClass("f"));
    QCOMPARE(p.classes().count(), 5);

    p.removeClass("f");
    QVERIFY(!p.hasClass("f"));
    QCOMPARE(p.classes().count(), 4);
    p.removeClass("d");
    QVERIFY(!p.hasClass("d"));
    QCOMPARE(p.classes().count(), 3);
    p.removeClass("not-exist");
    QCOMPARE(p.classes().count(), 3);
    p.removeClass("c");
    QVERIFY(!p.hasClass("c"));
    QCOMPARE(p.classes().count(), 2);
    p.removeClass("b");
    QVERIFY(!p.hasClass("b"));
    QCOMPARE(p.classes().count(), 1);
    p.removeClass("a");
    QVERIFY(!p.hasClass("a"));
    QCOMPARE(p.classes().count(), 0);
    p.removeClass("foobar");
    QCOMPARE(p.classes().count(), 0);
}

void tst_QWebElement::namespaceURI()
{
    QString content = "<html xmlns=\"http://www.w3.org/1999/xhtml\" "
                      "xmlns:svg=\"http://www.w3.org/2000/svg\">"
                      "<body><svg:svg id=\"foobar\" width=\"400px\" height=\"300px\">"
                      "</svg:svg></body></html>";

    m_mainFrame->setContent(content.toUtf8(), "application/xhtml+xml");
    QWebElement body = m_mainFrame->documentElement();
    QCOMPARE(body.namespaceUri(), QLatin1String("http://www.w3.org/1999/xhtml"));

    QWebElement svg = body.findAll("*#foobar").at(0);
    QCOMPARE(svg.prefix(), QLatin1String("svg"));
    QCOMPARE(svg.localName(), QLatin1String("svg"));
    QCOMPARE(svg.tagName(), QLatin1String("svg:svg"));
    QCOMPARE(svg.namespaceUri(), QLatin1String("http://www.w3.org/2000/svg"));

}

void tst_QWebElement::iteration()
{
    QString html = "<body><p>first para</p><p>second para</p></body>";
    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement();

   QWebElementCollection paras = body.findAll("p");
    QList<QWebElement> referenceList = paras.toList();

    QList<QWebElement> foreachList;
    foreach(QWebElement p, paras) {
       foreachList.append(p);
    }
    QVERIFY(foreachList.count() == 2);
    QCOMPARE(foreachList.count(), referenceList.count());
    QCOMPARE(foreachList.at(0), referenceList.at(0));
    QCOMPARE(foreachList.at(1), referenceList.at(1));

    QList<QWebElement> forLoopList;
    for (int i = 0; i < paras.count(); ++i) {
        forLoopList.append(paras.at(i));
    }
    QVERIFY(foreachList.count() == 2);
    QCOMPARE(foreachList.count(), referenceList.count());
    QCOMPARE(foreachList.at(0), referenceList.at(0));
    QCOMPARE(foreachList.at(1), referenceList.at(1));

    for (int i = 0; i < paras.count(); ++i) {
        QCOMPARE(paras.at(i), paras[i]);
    }

    QCOMPARE(paras.at(0), paras.first());
    QCOMPARE(paras.at(1), paras.last());
}

void tst_QWebElement::nonConstIterator()
{
    QString html = "<body><p>first para</p><p>second para</p></body>";
    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement();
    QWebElementCollection paras = body.findAll("p");

    QWebElementCollection::iterator it = paras.begin();
    QCOMPARE(*it, paras.at(0));
    ++it;
    (*it).encloseWith("<div>");
    QCOMPARE(*it, paras.at(1));
    ++it;
    QCOMPARE(it,  paras.end());
}

void tst_QWebElement::constIterator()
{
    QString html = "<body><p>first para</p><p>second para</p></body>";
    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement();
    const QWebElementCollection paras = body.findAll("p");

    QWebElementCollection::const_iterator it = paras.begin();
    QCOMPARE(*it, paras.at(0));
    ++it;
    QCOMPARE(*it, paras.at(1));
    ++it;
    QCOMPARE(it,  paras.end());
}

void tst_QWebElement::foreachManipulation()
{
    QString html = "<body><p>first para</p><p>second para</p></body>";
    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement();

    foreach(QWebElement p, body.findAll("p")) {
        p.setInnerXml("<div>foo</div><div>bar</div>");
    }

    QCOMPARE(body.findAll("div").count(), 4);
}

void tst_QWebElement::emptyCollection()
{
    QWebElementCollection emptyCollection;
    QCOMPARE(emptyCollection.count(), 0);
}

void tst_QWebElement::appendCollection()
{
    QString html = "<body><span class='a'>aaa</span><p>first para</p><div>foo</div>"
        "<span class='b'>bbb</span><p>second para</p><div>bar</div></body>";
    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement();

    QWebElementCollection collection = body.findAll("p");
    QCOMPARE(collection.count(), 2);

    collection.append(body.findAll("div"));
    QCOMPARE(collection.count(), 4);

    collection += body.findAll("span.a");
    QCOMPARE(collection.count(), 5);

    QWebElementCollection all = collection + body.findAll("span.b");
    QCOMPARE(all.count(), 6);
    QCOMPARE(collection.count(), 5);

     all += collection;
    QCOMPARE(all.count(), 11);

    QCOMPARE(collection.count(), 5);
    QWebElementCollection test;
    test.append(collection);
    QCOMPARE(test.count(), 5);
    test.append(QWebElementCollection());
    QCOMPARE(test.count(), 5);
}

void tst_QWebElement::documentElement()
{
    m_mainFrame->setHtml("<body><p>Test");

    QWebElement para = m_mainFrame->documentElement().findAll("p").at(0);
    QVERIFY(para.parent().parent() == m_mainFrame->documentElement());
    QVERIFY(para.document() == m_mainFrame->documentElement());
}

void tst_QWebElement::style()
{
    QString html = "<head>"
        "<style type='text/css'>"
            "p { color: green !important }"
            "#idP { color: red }"
            ".classP { color : yellow ! important }"
        "</style>"
    "</head>"
    "<body>"
        "<p id='idP' class='classP' style='color: blue;'>some text</p>"
    "</body>";

    m_mainFrame->setHtml(html);

    QWebElement p = m_mainFrame->documentElement().findAll("p").at(0);
    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("blue"));
    QVERIFY(p.styleProperty("cursor", QWebElement::InlineStyle).isEmpty());

    p.setStyleProperty("color", "red");
    p.setStyleProperty("cursor", "auto");

    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("red"));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("yellow"));
    QCOMPARE(p.styleProperty("cursor", QWebElement::InlineStyle), QLatin1String("auto"));

    p.setStyleProperty("color", "green !important");
    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("green"));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("green"));

    p.setStyleProperty("color", "blue");
    // A current important InlineStyle shouldn't be overwritten by a non-important one.
    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("green"));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("green"));

    p.setStyleProperty("color", "blue !important");
    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("blue"));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("blue"));

    QString html2 = "<head>"
        "<style type='text/css'>"
            "p { color: green }"
            "#idP { color: red }"
            ".classP { color: yellow }"
        "</style>"
    "</head>"
    "<body>"
        "<p id='idP' class='classP' style='color: blue;'>some text</p>"
    "</body>";

    m_mainFrame->setHtml(html2);
    p = m_mainFrame->documentElement().findAll("p").at(0);

    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("blue"));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("blue"));

    QString html3 = "<head>"
        "<style type='text/css'>"
            "p { color: green !important }"
            "#idP { color: red !important}"
            ".classP { color: yellow !important}"
        "</style>"
    "</head>"
    "<body>"
        "<p id='idP' class='classP' style='color: blue !important;'>some text</p>"
    "</body>";

    m_mainFrame->setHtml(html3);
    p = m_mainFrame->documentElement().findAll("p").at(0);

    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("blue"));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("blue"));

    QString html5 = "<head>"
        "<style type='text/css'>"
            "p { color: green }"
            "#idP { color: red }"
            ".classP { color: yellow }"
        "</style>"
    "</head>"
    "<body>"
        "<p id='idP' class='classP'>some text</p>"
    "</body>";

    m_mainFrame->setHtml(html5);
    p = m_mainFrame->documentElement().findAll("p").at(0);

    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String(""));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("red"));

    // An !important id rule beats an !important class rule regardless of source order.
    // (This used to pull #idP {color: black !important} from an external qrc: stylesheet;
    // resource loading is gone, so the rule is inline now.)
    QString html6 = "<head>"
        "<style type='text/css'>"
            "p { color: green }"
            "#idP { color: black ! important}"
            ".classP { color: yellow ! important}"
        "</style>"
    "</head>"
    "<body>"
        "<p id='idP' class='classP' style='color: blue;'>some text</p>"
    "</body>";

    m_mainFrame->setHtml(html6);

    p = m_mainFrame->documentElement().findAll("p").at(0);
    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("blue"));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String("black"));

    QString html8 = "<body><p>some text</p></body>";

    m_mainFrame->setHtml(html8);
    p = m_mainFrame->documentElement().findAll("p").at(0);

    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String(""));
    QCOMPARE(p.styleProperty("color", QWebElement::CascadedStyle), QLatin1String(""));
}

void tst_QWebElement::computedStyle()
{
    QString html = "<body><p>some text</p></body>";
    m_mainFrame->setHtml(html);

    QWebElement p = m_mainFrame->documentElement().findAll("p").at(0);
    QCOMPARE(p.styleProperty("cursor", QWebElement::ComputedStyle), QLatin1String("auto"));
    QVERIFY(!p.styleProperty("cursor", QWebElement::ComputedStyle).isEmpty());
    QVERIFY(p.styleProperty("cursor", QWebElement::InlineStyle).isEmpty());

    p.setStyleProperty("cursor", "text");
    p.setStyleProperty("color", "red");

    QCOMPARE(p.styleProperty("cursor", QWebElement::ComputedStyle), QLatin1String("text"));
    QCOMPARE(p.styleProperty("color", QWebElement::ComputedStyle), QLatin1String("rgb(255, 0, 0)"));
    QCOMPARE(p.styleProperty("color", QWebElement::InlineStyle), QLatin1String("red"));
}

void tst_QWebElement::textWidthAndFontMetrics()
{
    QString html = "<body>"
        "<p id='small' style='font-size: 10px; font-family: sans-serif;'>x</p>"
        "<p id='big' style='font-size: 40px; font-family: sans-serif;'>x</p>"
        "</body>";
    m_mainFrame->setHtml(html);

    QWebElement small = m_mainFrame->documentElement().findFirst("#small");
    QWebElement big = m_mainFrame->documentElement().findFirst("#big");

    QVERIFY(small.fontAscent() > 0);
    QVERIFY(small.fontDescent() > 0);
    QVERIFY(small.fontLineSpacing() > 0);

    // A larger computed font should produce larger metrics and wider text.
    QVERIFY(big.fontAscent() > small.fontAscent());
    QVERIFY(big.fontDescent() > small.fontDescent());
    QVERIFY(big.fontLineSpacing() > small.fontLineSpacing());

    // Line spacing is built from (at least) ascent + descent.
    QVERIFY(small.fontLineSpacing() >= small.fontAscent() + small.fontDescent() - 1.0);
    QVERIFY(big.fontLineSpacing() >= big.fontAscent() + big.fontDescent() - 1.0);

    QCOMPARE(big.textWidth(QString()), qreal(0));
    QVERIFY(big.textWidth("hello") > small.textWidth("hello"));
    QVERIFY(big.textWidth("hello world") > big.textWidth("hello"));
}

static void countEvent(const QString&, void* counter, uint64_t, uint64_t, const QString&)
{
    ++*static_cast<int*>(counter);
}

void tst_QWebElement::formControlValue()
{
    // "value" on <input> and <textarea> reads and writes the live value, not the
    // content attribute, and removing it reverts to the default value. Setting it on a textarea must not fire input/change: it's
    // the client's own change, not a user edit.
    m_mainFrame->setHtml("<body><input id=i value=initial><textarea id=t>initial</textarea></body>");
    QWebElement input = m_mainFrame->findFirstElement("#i");
    QWebElement textarea = m_mainFrame->findFirstElement("#t");

    QCOMPARE(input.attribute("value"), QString("initial"));
    input.setAttribute("value", "set");
    QCOMPARE(input.attribute("value"), QString("set"));
    input.removeAttribute("value");
    QCOMPARE(input.attribute("value"), QString("initial")); // reverts to the default

    int inputEvents = 0;
    int changeEvents = 0;
    textarea.requestEvent("input", &inputEvents, 0, 0, countEvent);
    textarea.requestEvent("change", &changeEvents, 0, 0, countEvent);

    QCOMPARE(textarea.attribute("value"), QString("initial"));
    textarea.setAttribute("value", "line 1\nline 2");
    QCOMPARE(textarea.attribute("value"), QString("line 1\nline 2"));
    QVERIFY(!textarea.hasAttribute("value"));
    textarea.removeAttribute("value");
    QCOMPARE(textarea.attribute("value"), QString("initial")); // reverts to the default
    QCOMPARE(inputEvents, 0);
    QCOMPARE(changeEvents, 0);

    // A user edit is still reported, and is what "value" then returns.
    textarea.setAttribute("value", "typed");
    textarea.setFocus();
    textarea.setSelectionRange(5, 5);
    QInputMethodEvent commit("", QList<QInputMethodEvent::Attribute>());
    commit.setCommitString(" text");
    m_page->event(&commit);
    QCOMPARE(textarea.attribute("value"), QString("typed text"));
    QCOMPARE(inputEvents, 1);
    textarea.cancelEvent("input");
    textarea.cancelEvent("change");
}

static void recordEvent(const QString& name, void* log, uint64_t, uint64_t, const QString& details)
{
    static_cast<QStringList*>(log)->append(name + ":" + details);
}

void tst_QWebElement::eventDetailModifiers()
{
    // Key (keydown/keyup), mouse and wheel event details end with a modifier mask:
    // 1=Shift, 2=Alt, 4=Ctrl, 8=Meta, always present. keypress stays charCode only.
    m_view->resize(400, 300);
    m_mainFrame->setHtml("<body style='margin:0'>"
                         "<div id=d tabindex=0 style='width:200px;height:100px'>x</div></body>");
    QWebElement div = m_mainFrame->findFirstElement("#d");
    QStringList log;
    for (const char* name : { "keydown", "keyup", "keypress", "click", "mousedown", "wheel" })
        div.requestEvent(name, &log, 0, 0, recordEvent);
    div.setFocus();

    QKeyEvent ctrlSDown(QEvent::KeyPress, Qt::Key_S, Qt::ControlModifier, "\x13");
    QKeyEvent ctrlSUp(QEvent::KeyRelease, Qt::Key_S, Qt::ControlModifier, "\x13");
    m_page->event(&ctrlSDown);
    m_page->event(&ctrlSUp);
    QVERIFY(log.contains("keydown:83,4"));
    QVERIFY(log.contains("keyup:83,4"));

    log.clear();
    QKeyEvent backtab(QEvent::KeyPress, Qt::Key_Backtab, Qt::ShiftModifier);
    m_page->event(&backtab);
    QVERIFY(log.contains("keydown:9,1"));

    log.clear();
    div.setFocus(); // Shift+Tab's default action moved focus away
    QKeyEvent plainA(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "a");
    m_page->event(&plainA);
    QVERIFY(log.contains("keydown:65,0"));
    QVERIFY(log.contains("keypress:97"));

    log.clear();
    QPoint p(50, 50);
    QMouseEvent press(QEvent::MouseButtonPress, p, Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier | Qt::AltModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, p, Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier | Qt::AltModifier);
    m_page->event(&press);
    m_page->event(&release);
    QVERIFY(log.contains("mousedown:1,50,50,50,50,3"));
    QVERIFY(log.contains("click:1,3"));

    log.clear();
    QWheelEvent wheel(p, p, QPoint(), QPoint(0, -120), Qt::NoButton, Qt::MetaModifier, Qt::NoScrollPhase, false);
    m_page->event(&wheel);
    QCOMPARE(log.size(), 1);
    QVERIFY(log.at(0).startsWith("wheel:"));
    QVERIFY(log.at(0).endsWith(",8"));
    QCOMPARE(log.at(0).count(','), 3);

    for (const char* name : { "keydown", "keyup", "keypress", "click", "mousedown", "wheel" })
        div.cancelEvent(name);
}

void tst_QWebElement::appendAndPrepend()
{
    QString html = "<body>"
        "<p>"
            "foo"
        "</p>"
        "<p>"
            "bar"
        "</p>"
    "</body>";

    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement().findFirst("body");

    QCOMPARE(body.findAll("p").count(), 2);
    body.appendInside(body.findFirst("p"));
    QCOMPARE(body.findAll("p").count(), 2);
    QCOMPARE(body.findFirst("p").toPlainText(), QString("bar"));
    QCOMPARE(body.findAll("p").last().toPlainText(), QString("foo"));

    body.appendInside(body.findFirst("p").clone());
    QCOMPARE(body.findAll("p").count(), 3);
    QCOMPARE(body.findFirst("p").toPlainText(), QString("bar"));
    QCOMPARE(body.findAll("p").last().toPlainText(), QString("bar"));

    body.prependInside(body.findAll("p").at(1).clone());
    QCOMPARE(body.findAll("p").count(), 4);
    QCOMPARE(body.findFirst("p").toPlainText(), QString("foo"));

    body.findFirst("p").appendInside("<div>booyakasha</div>");
    QCOMPARE(body.findAll("p div").count(), 1);
    QCOMPARE(body.findFirst("p div").toPlainText(), QString("booyakasha"));

    body.findFirst("div").prependInside("<code>yepp</code>");
    QCOMPARE(body.findAll("p div code").count(), 1);
    QCOMPARE(body.findFirst("p div code").toPlainText(), QString("yepp"));

    // Inserting HTML into an img tag is not allowed, but appending/prepending outside is.
    body.findFirst("div").appendInside("<img src=\"test.png\">");
    QCOMPARE(body.findAll("p div img").count(), 1);

    QWebElement img = body.findFirst("img");
    QVERIFY(!img.isNull());
    img.appendInside("<p id=\"fail1\"></p>");
    QCOMPARE(body.findAll("p#fail1").count(), 0);

    img.appendOutside("<p id=\"success1\"></p>");
    QCOMPARE(body.findAll("p#success1").count(), 1);

    img.prependInside("<p id=\"fail2\"></p>");
    QCOMPARE(body.findAll("p#fail2").count(), 0);

    img.prependOutside("<p id=\"success2\"></p>");
    QCOMPARE(body.findAll("p#success2").count(), 1);


}

// Regression guard: QWebElement's markup-insertion methods (appendInside() etc.) used to
// silently no-op on non-HTMLElement targets, because they called the narrower
// createContextualFragment(HTMLElement*) via an isHTMLElement() guard. SVGElement isn't an
// HTMLElement, so appending markup *inside* an <svg> (its root is fine - it's a child of
// <body>, an HTMLElement - but nothing appended past that point) silently did nothing.
// Fixed by routing through createFragmentForInnerOuterHTML(), the namespace-aware fragment
// parser Element::setInnerHTML() itself uses.
void tst_QWebElement::appendSvgInside()
{
    QWebElement body = m_mainFrame->documentElement().findFirst("body");
    body.setInnerXml("<svg id=\"s\" width=\"100\" height=\"100\"></svg>");

    QWebElement svg = body.findFirst("svg");
    QVERIFY(!svg.isNull());

    svg.appendInside("<circle cx=\"50\" cy=\"50\" r=\"40\" fill=\"red\"></circle>");
    QCOMPARE(body.findAll("svg circle").count(), 1);
    QCOMPARE(body.findFirst("svg circle").attribute("fill"), QString("red"));

    // Geometry attributes on SVG shapes are backed by SVGAnimatedProperty (cx/cy/r, x/y/
    // width/height, d, x1/y1/x2/y2, ...) rather than being plain string attributes like
    // "fill" above. Cover that these still round-trip through QWebElement::attribute()/
    // setAttribute()/toInnerXml() - regression coverage for a suspected sync gap between
    // the animated-property representation and the string attribute map that turned out,
    // on investigation, not to reproduce.
    QWebElement circle = body.findFirst("svg circle");
    QCOMPARE(circle.attribute("cx"), QString("50"));
    QCOMPARE(circle.attribute("cy"), QString("50"));
    QCOMPARE(circle.attribute("r"), QString("40"));

    circle.setAttribute("cx", "70");
    QCOMPARE(circle.attribute("cx"), QString("70"));
    QCOMPARE(svg.toInnerXml(), QString("<circle cx=\"70\" cy=\"50\" r=\"40\" fill=\"red\"></circle>"));

    svg.appendInside("<path d=\"M10 10 L20 20\"></path>");
    QCOMPARE(svg.findFirst("path").attribute("d"), QString("M10 10 L20 20"));

    svg.appendInside("<line x1=\"1\" y1=\"2\" x2=\"3\" y2=\"4\"></line>");
    QWebElement line = svg.findFirst("line");
    QCOMPARE(line.attribute("x1"), QString("1"));
    QCOMPARE(line.attribute("y1"), QString("2"));
    QCOMPARE(line.attribute("x2"), QString("3"));
    QCOMPARE(line.attribute("y2"), QString("4"));

    svg.findFirst("circle").appendOutside("<rect x=\"1\" y=\"2\" width=\"10\" height=\"20\"></rect>");
    QCOMPARE(body.findAll("svg rect").count(), 1);
    QWebElement rect = body.findFirst("svg rect");
    QCOMPARE(rect.attribute("x"), QString("1"));
    QCOMPARE(rect.attribute("y"), QString("2"));
    QCOMPARE(rect.attribute("width"), QString("10"));
    QCOMPARE(rect.attribute("height"), QString("20"));
}

void tst_QWebElement::svgRenderingFidelity()
{
    // Complements appendSvgInside()'s attribute round-trip coverage: does the geometry
    // actually PAINT at the right position/size, not just read back correctly as a string?
    // A 100x100 white canvas with a 40px-radius red circle centered at (50,50) - sample pixels
    // at known in/out-of-circle points around the boundary.
    QString html = "<html><head><style>body{margin:0;background:white;}</style></head>"
        "<body><svg width=\"100\" height=\"100\">"
        "<circle cx=\"50\" cy=\"50\" r=\"40\" fill=\"red\"></circle>"
        "</svg></body></html>";

    QWebPage page;
    QSignalSpy loadSpy(&page, SIGNAL(loadFinished(bool)));
    page.mainFrame()->setHtml(html);
    QCOMPARE(loadSpy.count(), 1);
    page.setViewportSize(QSize(100, 100));

    QImage image(100, 100, QImage::Format_ARGB32);
    QPainter painter(&image);
    painter.fillRect(QRect(0, 0, 100, 100), Qt::white);
    page.mainFrame()->render(&painter, QRect(0, 0, 100, 100));
    painter.end();

    QCOMPARE(image.pixelColor(50, 50), QColor(Qt::red));    // center
    QCOMPARE(image.pixelColor(2, 2), QColor(Qt::white));    // corner, well outside
    QCOMPARE(image.pixelColor(85, 50), QColor(Qt::red));    // just inside right edge
    QCOMPARE(image.pixelColor(95, 50), QColor(Qt::white));  // just outside right edge
    QCOMPARE(image.pixelColor(50, 12), QColor(Qt::red));    // just inside top edge (cy-r+2)
    QCOMPARE(image.pixelColor(50, 5), QColor(Qt::white));   // just outside top edge
}

void tst_QWebElement::insertBeforeAndAfter()
{
    QString html = "<body>"
        "<p>"
            "foo"
        "</p>"
        "<div>"
            "yeah"
        "</div>"
        "<p>"
            "bar"
        "</p>"
    "</body>";

    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement().findFirst("body");
    QWebElement div = body.findFirst("div");

    QCOMPARE(body.findAll("p").count(), 2);
    QCOMPARE(body.findAll("div").count(), 1);

    div.prependOutside(body.findAll("p").last().clone());
    QCOMPARE(body.findAll("p").count(), 3);
    QCOMPARE(body.findAll("p").at(0).toPlainText(), QString("foo"));
    QCOMPARE(body.findAll("p").at(1).toPlainText(), QString("bar"));
    QCOMPARE(body.findAll("p").at(2).toPlainText(), QString("bar"));

    div.appendOutside(body.findFirst("p").clone());
    QCOMPARE(body.findAll("p").count(), 4);
    QCOMPARE(body.findAll("p").at(0).toPlainText(), QString("foo"));
    QCOMPARE(body.findAll("p").at(1).toPlainText(), QString("bar"));
    QCOMPARE(body.findAll("p").at(2).toPlainText(), QString("foo"));
    QCOMPARE(body.findAll("p").at(3).toPlainText(), QString("bar"));

    div.prependOutside("<span>hey</span>");
    QCOMPARE(body.findAll("span").count(), 1);

    div.appendOutside("<span>there</span>");
    QCOMPARE(body.findAll("span").count(), 2);
    QCOMPARE(body.findAll("span").at(0).toPlainText(), QString("hey"));
    QCOMPARE(body.findAll("span").at(1).toPlainText(), QString("there"));
}

void tst_QWebElement::remove()
{
    QString html = "<body>"
        "<p>"
            "foo"
        "</p>"
        "<div>"
            "<p>yeah</p>"
        "</div>"
        "<p>"
            "bar"
        "</p>"
    "</body>";

    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement().findFirst("body");

    QCOMPARE(body.findAll("div").count(), 1);
    QCOMPARE(body.findAll("p").count(), 3);

    QWebElement div = body.findFirst("div");
    div.takeFromDocument();

    QCOMPARE(div.isNull(), false);
    QCOMPARE(body.findAll("div").count(), 0);
    QCOMPARE(body.findAll("p").count(), 2);

    body.appendInside(div);

    QCOMPARE(body.findAll("div").count(), 1);
    QCOMPARE(body.findAll("p").count(), 3);
}

void tst_QWebElement::clear()
{
    QString html = "<body>"
        "<p>"
            "foo"
        "</p>"
        "<div>"
            "<p>yeah</p>"
        "</div>"
        "<p>"
            "bar"
        "</p>"
    "</body>";

    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement().findFirst("body");

    QCOMPARE(body.findAll("div").count(), 1);
    QCOMPARE(body.findAll("p").count(), 3);
    body.findFirst("div").removeAllChildren();
    QCOMPARE(body.findAll("div").count(), 1);
    QCOMPARE(body.findAll("p").count(), 2);
}


void tst_QWebElement::replaceWith()
{
    QString html = "<body>"
        "<p>"
            "foo"
        "</p>"
        "<div>"
            "yeah"
        "</div>"
        "<p>"
            "<span>haba</span>"
        "</p>"
    "</body>";

    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement().findFirst("body");

    QCOMPARE(body.findAll("div").count(), 1);
    QCOMPARE(body.findAll("span").count(), 1);
    body.findFirst("div").replace(body.findFirst("span").clone());
    QCOMPARE(body.findAll("div").count(), 0);
    QCOMPARE(body.findAll("span").count(), 2);
    QCOMPARE(body.findAll("p").count(), 2);

    body.findFirst("span").replace("<p><code>wow</code></p>");
    QCOMPARE(body.findAll("p").count(), 3);
    QCOMPARE(body.findAll("p code").count(), 1);
    QCOMPARE(body.findFirst("p code").toPlainText(), QString("wow"));
}

void tst_QWebElement::encloseContentsWith()
{
    QString html = "<body>"
        "<div>"
            "<i>"
                "yeah"
            "</i>"
            "<i>"
                "hello"
            "</i>"
        "</div>"
        "<p>"
            "<span>foo</span>"
            "<span>bar</span>"
        "</p>"
        "<u></u>"
        "<b></b>"
        "<em>hey</em>"
    "</body>";

    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement().findFirst("body");

    body.findFirst("p").encloseContentsWith(body.findFirst("b"));
    QCOMPARE(body.findAll("p b span").count(), 2);
    QCOMPARE(body.findFirst("p b span").toPlainText(), QString("foo"));

    body.findFirst("u").encloseContentsWith("<i></i>");
    QCOMPARE(body.findAll("u i").count(), 1);
    QCOMPARE(body.findFirst("u i").toPlainText(), QString());

    body.findFirst("div").encloseContentsWith("<span></span>");
    QCOMPARE(body.findAll("div span i").count(), 2);
    QCOMPARE(body.findFirst("div span i").toPlainText(), QString("yeah"));

    QString snippet = ""
        "<table>"
            "<tbody>"
                "<tr>"
                    "<td></td>"
                    "<td></td>"
                "</tr>"
                "<tr>"
                    "<td></td>"
                    "<td></td>"
                "<tr>"
            "</tbody>"
        "</table>";

    body.findFirst("em").encloseContentsWith(snippet);
    QCOMPARE(body.findFirst("em table tbody tr td").toPlainText(), QString("hey"));
}

void tst_QWebElement::encloseWith()
{
    QString html = "<body>"
        "<p>"
            "foo"
        "</p>"
        "<div>"
            "yeah"
        "</div>"
        "<p>"
            "<span>bar</span>"
        "</p>"
        "<em>hey</em>"
        "<h1>hello</h1>"
    "</body>";

    m_mainFrame->setHtml(html);
    QWebElement body = m_mainFrame->documentElement().findFirst("body");

    body.findFirst("p").encloseWith("<br>");
    QCOMPARE(body.findAll("br").count(), 0);

    QCOMPARE(body.findAll("div").count(), 1);
    body.findFirst("div").encloseWith(body.findFirst("span").clone());
    QCOMPARE(body.findAll("div").count(), 1);
    QCOMPARE(body.findAll("span").count(), 2);
    QCOMPARE(body.findAll("p").count(), 2);

    body.findFirst("div").encloseWith("<code></code>");
    QCOMPARE(body.findAll("code").count(), 1);
    QCOMPARE(body.findAll("code div").count(), 1);
    QCOMPARE(body.findFirst("code div").toPlainText(), QString("yeah"));

    QString snippet = ""
        "<table>"
            "<tbody>"
                "<tr>"
                    "<td></td>"
                    "<td></td>"
                "</tr>"
                "<tr>"
                    "<td></td>"
                    "<td></td>"
                "<tr>"
            "</tbody>"
        "</table>";

    body.findFirst("em").encloseWith(snippet);
    QCOMPARE(body.findFirst("table tbody tr td em").toPlainText(), QString("hey"));

    // Enclosing the contents of an img tag is not allowed, but enclosing the img tag itself is.
    body.findFirst("td").appendInside("<img src=\"test.png\">");
    QCOMPARE(body.findAll("img").count(), 1);

    QWebElement img = body.findFirst("img");
    QVERIFY(!img.isNull());
    img.encloseWith("<p id=\"success\"></p>");
    QCOMPARE(body.findAll("p#success").count(), 1);

    img.encloseContentsWith("<p id=\"fail\"></p>");
    QCOMPARE(body.findAll("p#fail").count(), 0);

}

void tst_QWebElement::nullSelect()
{
    m_mainFrame->setHtml("<body><p>Test");

    QWebElementCollection collection = m_mainFrame->findAllElements("invalid{syn(tax;;%#$f223e>>");
    QVERIFY(collection.count() == 0);
}

void tst_QWebElement::firstChildNextSibling()
{
    m_mainFrame->setHtml("<body><!--comment--><p>Test</p><!--another comment--><table>");

    QWebElement body = m_mainFrame->findFirstElement("body");
    QVERIFY(!body.isNull());
    QWebElement p = body.firstChild();
    QVERIFY(!p.isNull());
    QCOMPARE(p.tagName(), QString("P"));
    QWebElement table = p.nextSibling();
    QVERIFY(!table.isNull());
    QCOMPARE(table.tagName(), QString("TABLE"));
    QVERIFY(table.nextSibling().isNull());
}

void tst_QWebElement::lastChildPreviousSibling()
{
    m_mainFrame->setHtml("<body><!--comment--><p>Test</p><!--another comment--><table>");

    QWebElement body = m_mainFrame->findFirstElement("body");
    QVERIFY(!body.isNull());
    QWebElement table = body.lastChild();
    QVERIFY(!table.isNull());
    QCOMPARE(table.tagName(), QString("TABLE"));
    QWebElement p = table.previousSibling();
    QVERIFY(!p.isNull());
    QCOMPARE(p.tagName(), QString("P"));
    QVERIFY(p.previousSibling().isNull());
}

void tst_QWebElement::hasSetFocus()
{
    m_mainFrame->setHtml("<html><body>" \
                            "<input type='text' id='input1'/>" \
                            "<br>"\
                            "<input type='text' id='input2'/>" \
                            "</body></html>");

    QWebElementCollection inputs = m_mainFrame->documentElement().findAll("input");
    QWebElement input1 = inputs.at(0);
    input1.setFocus();
    QVERIFY(input1.hasFocus());

    QWebElement input2 = inputs.at(1);
    input2.setFocus();
    QVERIFY(!input1.hasFocus());
    QVERIFY(input2.hasFocus());
}

void tst_QWebElement::render()
{
    QString html( "<html>"
                    "<head><style>"
                       "body, iframe { margin: 0px; border: none; background: white; }"
                    "</style></head>"
                    "<body><table width='300px' height='300px' border='1'>"
                           "<tr>"
                               "<td>test"
                               "</td>"
                               "<td>test2"
                               "</td>"
                           "</tr>"
                          "</table>"
                    "</body>"
                 "</html>"
                );

    QWebPage page;
    QSignalSpy loadSpy(&page, SIGNAL(loadFinished(bool)));
    page.mainFrame()->setHtml(html);
    QCOMPARE(loadSpy.count(), 1);

    QSize size = page.mainFrame()->contentsSize();
    page.setViewportSize(size);

    // compare table rendered through QWebElement::render to whole page table rendering
    QRect tableRect(0, 0, 300, 300);
    QWebElementCollection tables = page.mainFrame()->findAllElements("table");
    QCOMPARE(tables.count(), 1);

    QImage image3(300, 300, QImage::Format_ARGB32);
    QPainter painter3(&image3);
    painter3.fillRect(tableRect, Qt::white);
    tables[0].render(&painter3);
    painter3.end();

    QImage image4(300, 300, QImage::Format_ARGB32);
    QPainter painter4(&image4);
    page.mainFrame()->render(&painter4, tableRect);
    painter4.end();

    QVERIFY(image3 == image4);

    // Chunked render test reuses page rendered in image4 in previous test
    const int chunkHeight = tableRect.height();
    const int chunkWidth = tableRect.width() / 3;
    QImage chunk(chunkWidth, chunkHeight, QImage::Format_ARGB32);
    QRect chunkRect(0, 0, chunkWidth, chunkHeight);
    for (int x = 0; x < tableRect.width(); x += chunkWidth) {
        QPainter painter(&chunk);
        painter.fillRect(chunkRect, Qt::white);
        QRect chunkPaintRect(x, 0, chunkWidth, chunkHeight);
        tables[0].render(&painter, chunkPaintRect);
        painter.end();

        QVERIFY(chunk == image4.copy(chunkPaintRect));
    }
}

void tst_QWebElement::addElementToHead()
{
    m_mainFrame->setHtml("<html><head></head><body></body></html>");
    QWebElement head = m_mainFrame->findFirstElement("head");
    QVERIFY(!head.isNull());
    QString append = "<script type=\"text/javascript\">var t = 0;</script>";
    head.appendInside(append);
    // Was QEXPECT_FAIL'd against https://bugs.webkit.org/show_bug.cgi?id=102234 - fixed as a
    // side effect of routing QWebElement's markup-insertion methods through
    // createFragmentForInnerOuterHTML() (the same namespace-aware fragment parser
    // Element::setInnerHTML() uses) instead of the narrower Range-oriented
    // createContextualFragment(HTMLElement*).
    QCOMPARE(head.toInnerXml(), append);
}

// hipecore: <script> is inert (no engine, scripting disabled) and the script-execution
// machinery was removed in Bucket 6. The HTML tokenizer still raw-texts <script> content
// and the tree builder still inserts an inert element, so a <script> containing markup or
// "<" must not leak nodes into the DOM, and the parser must not stall.
void tst_QWebElement::scriptAndNoscriptParsing()
{
    // Markup inside <script> stays as text - no stray <b>/<i> elements.
    m_mainFrame->setHtml("<body><div id='d'><script>if (a < b && c > d) { document.write('<b>x</b>'); }</script></div></body>");
    QWebElement d = m_mainFrame->findFirstElement("#d");
    QVERIFY(!d.isNull());
    QCOMPARE(d.findAll("b").count(), 0);
    QCOMPARE(d.findAll("i").count(), 0);
    QCOMPARE(d.findAll("script").count(), 1);
    QVERIFY(d.findFirst("script").toPlainText().contains("a < b"));

    // Content after the </script> parses normally - the parser did not stall.
    m_mainFrame->setHtml("<body><script>var x = 1;</script><p id='after'>reached</p></body>");
    QCOMPARE(m_mainFrame->findFirstElement("#after").toPlainText(), QString("reached"));

    // Case-insensitive </SCRIPT> still terminates the raw-text region.
    m_mainFrame->setHtml("<body><script>1 < 2;</SCRIPT><p id='p2'>ok</p></body>");
    QCOMPARE(m_mainFrame->findFirstElement("#p2").toPlainText(), QString("ok"));

    // External script: element present, inert, no crash / no stall.
    m_mainFrame->setHtml("<body><script src='does-not-exist.js'></script><p id='p3'>ok</p></body>");
    QVERIFY(!m_mainFrame->findFirstElement("script").isNull());
    QCOMPARE(m_mainFrame->findFirstElement("#p3").toPlainText(), QString("ok"));

    // <script type="application/json"> keeps its text payload verbatim.
    m_mainFrame->setHtml("<body><script id='j' type='application/json'>{\"k\": 1}</script></body>");
    QCOMPARE(m_mainFrame->findFirstElement("#j").toPlainText().trimmed(), QString("{\"k\": 1}"));

    // <noscript> content renders as normal elements (scripting is disabled).
    m_mainFrame->setHtml("<body><noscript><p id='ns'>fallback</p></noscript></body>");
    QCOMPARE(m_mainFrame->findFirstElement("#ns").toPlainText(), QString("fallback"));

    // <noscript> in <head> uses the InHeadNoscript insertion mode: <meta>/<link>
    // stay in <head>, anything else bails out to <body>.
    m_mainFrame->setHtml("<html><head><noscript>"
                         "<meta name='m' content='v'><link id='lnk' rel='x' href='y'>"
                         "<p id='stray'>x</p>"
                         "</noscript></head><body></body></html>");
    QVERIFY(!m_mainFrame->findFirstElement("head #lnk").isNull());
    QVERIFY(!m_mainFrame->findFirstElement("body #stray").isNull());
}

void tst_QWebElement::xmlParsing()
{
    // Exercises the libxml2 SAX callbacks now that the parser-paused buffering is
    // gone: nested elements, a comment, a CDATA section, a PI and an inert <script>.
    QString content = "<?xml version=\"1.0\"?>"
                      "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
                      "<head><title>t</title></head>"
                      "<body>"
                      "<!-- a comment -->"
                      "<?target instruction?>"
                      "<div id=\"outer\"><span id=\"inner\">hi</span></div>"
                      "<div id=\"cd\"><![CDATA[a < b & c]]></div>"
                      "<script>var x = 2;</script>"
                      "<p id=\"last\">reached</p>"
                      "</body></html>";
    m_mainFrame->setContent(content.toUtf8(), "application/xhtml+xml");

    QCOMPARE(m_mainFrame->findFirstElement("#inner").toPlainText(), QString("hi"));
    QCOMPARE(m_mainFrame->findFirstElement("#cd").toPlainText(), QString("a < b & c"));
    QCOMPARE(m_mainFrame->findFirstElement("#last").toPlainText(), QString("reached"));
    QCOMPARE(m_mainFrame->findAllElements("script").count(), 1);
}

void tst_QWebElement::hipeLocation()
{
    m_mainFrame->setHtml("<div id=ed contenteditable><p id=a>first</p><p id=b>second</p><p id=c>third</p></div>");
    QWebElement a = m_mainFrame->findFirstElement("#a");
    QWebElement b = m_mainFrame->findFirstElement("#b");
    QWebElement ed = m_mainFrame->findFirstElement("#ed");
    QWebLocationRegistry registry;

    // Bind, look up both ways, through another handle; one number per element.
    QCOMPARE(b.hipeLocation(), quint64(0));
    registry.bind(42, b);
    QCOMPARE(registry.count(), size_t(1));
    QVERIFY(registry.inUse(42));
    QCOMPARE(registry.element(42), b);
    QCOMPARE(m_mainFrame->findFirstElement("#b").hipeLocation(), quint64(42));
    registry.bind(43, b); // already numbered: 43 binds none
    QVERIFY(registry.inUse(43));
    QVERIFY(registry.element(43).isNull());
    QCOMPARE(b.hipeLocation(), quint64(42));
    QWebLocationRegistry::Ranges ranges;
    ranges << qMakePair(quint64(40), quint64(41)) << qMakePair(quint64(43), quint64(50));
    QCOMPARE(registry.firstInUse(ranges), quint64(43));
    registry.free(43);
    QCOMPARE(registry.firstInUse(ranges), quint64(0));

    // Not copied by cloning, not in the DOM.
    QCOMPARE(b.clone().hipeLocation(), quint64(0));
    QVERIFY(!ed.toOuterXml().contains("42"));

    // An editing deletion removes #b; undo re-inserts the same node, still numbered.
    ed.setFocus();
    ed.setSelectionRange(5, 12); // "\nsecond": merges #b away
    m_page->insertText(QString());
    QVERIFY(m_mainFrame->findFirstElement("#b").isNull());
    m_page->triggerAction(QWebPage::Undo);
    QCOMPARE(m_mainFrame->findFirstElement("#b"), b);
    QCOMPARE(m_mainFrame->findFirstElement("#b").hipeLocation(), quint64(42));

    // Free, reuse, undo: the client frees 42 after the deletion and gives it to #a. The returning node has no number.
    ed.setSelectionRange(5, 12);
    m_page->insertText(QString());
    QVERIFY(m_mainFrame->findFirstElement("#b").isNull());
    registry.free(42);
    registry.bind(42, a);
    m_page->triggerAction(QWebPage::Undo);
    QCOMPARE(m_mainFrame->findFirstElement("#b"), b);
    QCOMPARE(b.hipeLocation(), quint64(0));
    QCOMPARE(a.hipeLocation(), quint64(42));
    registry.free(42);
    QCOMPARE(a.hipeLocation(), quint64(0));
    QCOMPARE(registry.count(), size_t(0));
}

void tst_QWebElement::hipeLocationWeak()
{
    m_mainFrame->setHtml("<div id=box><p id=gone>gone</p><p id=kept>kept</p></div>");
    QWebLocationRegistry registry;
    {
        QWebElement gone = m_mainFrame->findFirstElement("#gone");
        registry.bind(1, gone);
        registry.bind(2, m_mainFrame->findFirstElement("#kept"));
        gone.removeFromDocument();
    }
    // Nothing but the registry holds #gone now. Binds trigger a sweep every so often.
    for (quint64 n = 10; n < 3000; n++)
        registry.bind(n, QWebElement());
    QVERIFY(registry.inUse(1)); // still reserved
    QVERIFY(registry.element(1).isNull()); // but its element was released
    QCOMPARE(registry.element(2), m_mainFrame->findFirstElement("#kept")); // in the document: kept

    // Removed by editing: the undo history keeps it alive, so a sweep keeps its number, and undo brings it back.
    m_mainFrame->setHtml("<div id=ed contenteditable><p id=a>first</p><p id=b>second</p></div>");
    QWebLocationRegistry edits;
    edits.bind(1, m_mainFrame->findFirstElement("#b"));
    QWebElement ed = m_mainFrame->findFirstElement("#ed");
    ed.setFocus();
    ed.setSelectionRange(5, 12);
    m_page->insertText(QString());
    QVERIFY(m_mainFrame->findFirstElement("#b").isNull());
    for (quint64 n = 10; n < 3000; n++)
        edits.bind(n, QWebElement());
    QVERIFY(!edits.element(1).isNull());
    m_page->triggerAction(QWebPage::Undo);
    QCOMPARE(m_mainFrame->findFirstElement("#b").hipeLocation(), quint64(1));
}

void tst_QWebElement::hipeLocationBindMarkup()
{
    m_mainFrame->setHtml("<div id=box></div>");
    QWebElement box = m_mainFrame->findFirstElement("#box");
    QWebLocationRegistry registry;
    QList<QPair<QWebElement, QString>> found;
    box.setInnerXml("<p hipe-loc=5>a</p><p hipe-loc=6>b</p><p hipe-loc=6>c</p><p hipe-loc=\" 7\">d</p>"
                    "<p hipe-loc=+8>e</p><p hipe-loc=99>f</p>", &found);
    QWebLocationRegistry::Ranges listed;
    listed << qMakePair(quint64(5), quint64(8));
    QWebLocationRegistry::MarkupResult result = registry.bindMarkup(found, listed);
    QCOMPARE(registry.element(5).toPlainText(), QString("a"));
    QCOMPARE(registry.element(6).toPlainText(), QString("b")); // first carrier wins
    QCOMPARE(result.duplicates, QVector<quint64>() << 6);
    QVERIFY(registry.inUse(7) && registry.element(7).isNull()); // " 7" isn't digits only: 7 found on no element
    QVERIFY(registry.inUse(8) && registry.element(8).isNull()); // nor is "+8"
    QCOMPARE(result.unboundCount, quint64(2));
    QVERIFY(!registry.inUse(99)); // not listed
    QCOMPARE(registry.count(), size_t(4));
}

void tst_QWebElement::appendNewElement()
{
    m_mainFrame->setHtml("<table id=t></table><div id=d></div><svg id=s></svg><math id=m></math>"
                         "<svg><foreignObject id=fo></foreignObject></svg>");
    QWebElement table = m_mainFrame->findFirstElement("#t");
    QWebElement div = m_mainFrame->findFirstElement("#d");

    // No wrappers, nothing dropped: the element asked for, where asked.
    QWebElement tr = table.appendNewElement("tr");
    QCOMPARE(tr.parent(), table);
    QCOMPARE(tr.tagName(), QString("TR"));
    QWebElement td = div.appendNewElement("TD", "cell", "x y", "text");
    QCOMPARE(td.parent(), div);
    QCOMPARE(td.attribute("id"), QString("cell"));
    QCOMPARE(td.attribute("class"), QString("x y"));
    QCOMPARE(td.toPlainText(), QString("text"));

    // Namespaces and SVG letter case, as the parser chooses them.
    QWebElement grad = m_mainFrame->findFirstElement("#s").appendNewElement("lineargradient");
    QCOMPARE(grad.namespaceUri(), QString("http://www.w3.org/2000/svg"));
    QCOMPARE(grad.localName(), QString("linearGradient"));
    QCOMPARE(m_mainFrame->findFirstElement("#m").appendNewElement("mi").namespaceUri(), QString("http://www.w3.org/1998/Math/MathML"));
    QCOMPARE(div.appendNewElement("svg").namespaceUri(), QString("http://www.w3.org/2000/svg"));
    QCOMPARE(m_mainFrame->findFirstElement("#fo").appendNewElement("div").namespaceUri(), QString("http://www.w3.org/1999/xhtml"));

    // Text is data: a leading newline in a pre is kept; \r\n and \r become \n; void elements ignore text.
    QWebElement pre = div.appendNewElement("pre", QString(), QString(), "\n\nabc\r\ndef\rg");
    QCOMPARE(pre.toPlainText(), QString("\n\nabc\ndef\ng"));
    QWebElement br = div.appendNewElement("br", QString(), QString(), "ignored");
    QCOMPARE(br.toInnerXml(), QString());

    // Invalid names are refused; insertNewElementBefore puts the element before this one.
    QVERIFY(div.appendNewElement("not valid").isNull());
    QVERIFY(div.appendNewElement("a<b").isNull());
    QWebElement first = pre.insertNewElementBefore("span", "before");
    QCOMPARE(first.nextSibling(), pre);
    QCOMPARE(first.parent(), div);
}

static quint64 s_eventLocation = 0;
static int s_eventCount = 0;
static void recordLocationEvent(const QString&, void*, uint64_t location, uint64_t, const QString&)
{
    s_eventLocation = location;
    s_eventCount++;
}

void tst_QWebElement::eventReportsCurrentLocation()
{
    m_view->resize(400, 300);
    m_view->show();
    QVERIFY(QTest::qWaitForWindowExposed(m_view));
    m_mainFrame->setHtml("<div id=b style='width:200px;height:100px'>button</div>");
    QWebElement b = m_mainFrame->findFirstElement("#b");
    QWebLocationRegistry registry;
    registry.bind(7, b);
    b.requestEvent("click", nullptr, 7, 0, recordLocationEvent, false, true);

    s_eventCount = 0;
    QTest::mouseClick(m_view, Qt::LeftButton, Qt::NoModifier, b.geometry().center());
    QCOMPARE(s_eventCount, 1);
    QCOMPARE(s_eventLocation, quint64(7));

    registry.free(7); // freed: its own events stop
    QTest::mouseClick(m_view, Qt::LeftButton, Qt::NoModifier, b.geometry().center());
    QCOMPARE(s_eventCount, 1);

    registry.bind(9, b); // renumbered: reported with the current number
    QTest::mouseClick(m_view, Qt::LeftButton, Qt::NoModifier, b.geometry().center());
    QCOMPARE(s_eventCount, 2);
    QCOMPARE(s_eventLocation, quint64(9));
    m_view->hide();
}

void tst_QWebElement::hipeLocationMarkup()
{
    m_mainFrame->setHtml("<div id=box><p id=old>old</p></div><div id=other><b id=o1>o</b></div>");
    QWebElement box = m_mainFrame->findFirstElement("#box");
    QList<QPair<QWebElement, QString>> found;

    // setInnerXml: every carrier listed in document order, with its value; the attribute never reaches the document.
    box.setInnerXml("<p id=a hipe-loc=\"57\">a<span id=b HIPE-LOC=x>b</span></p><svg><rect id=c hipe-loc=\"58\"/></svg>", &found);
    QCOMPARE(found.size(), 3);
    QCOMPARE(found[0].first, m_mainFrame->findFirstElement("#a"));
    QCOMPARE(found[0].second, QString("57"));
    QCOMPARE(found[1].first, m_mainFrame->findFirstElement("#b"));
    QCOMPARE(found[1].second, QString("x"));
    QCOMPARE(found[2].second, QString("58"));
    QVERIFY(!box.toOuterXml().contains("hipe-loc", Qt::CaseInsensitive));

    // appendInside: only the appended elements are listed.
    found.clear();
    box.appendInside("<i id=d hipe-loc=\"59\">d</i>", &found);
    QCOMPARE(found.size(), 1);
    QCOMPARE(found[0].first, m_mainFrame->findFirstElement("#d"));
    QVERIFY(!box.toOuterXml().contains("hipe-loc"));

    // The plain overloads and prependOutside remove it too.
    box.appendInside("<i hipe-loc=\"60\">e</i>");
    box.setInnerXml(box.toInnerXml() + "<i hipe-loc=\"61\">f</i>");
    m_mainFrame->findFirstElement("#d").prependOutside("<i hipe-loc=\"62\">g</i>");
    QVERIFY(!m_mainFrame->documentElement().toOuterXml().contains("hipe-loc"));
}

QTEST_MAIN(tst_QWebElement)
#include "tst_qwebelement.moc"
