/*  Copyright (c) 2015-2026 Daniel Kos, General Development Systems

    This file is part of Hipe.

    Hipe is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Hipe is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Hipe.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "containertoplevel.h"
#include "connection.h"
#include "icondata.h"
#include "sanitation.h"
#include "main.hpp"
#include "mousecursor.h"

#include <QtWebKitWidgets/QWebFrame>
#include <QtWebKitWidgets/QWebPage>
#include <QGraphicsSceneMouseEvent>
#include <QSizePolicy>
#include <QPalette>
#include <QDesktopWidget>
#include <QApplication>
#include <QScreen>
#include <QPixmap>
#include <QInputDialog>
#include <QComboBox>
#include <vector>
#include <QFileDialog>
#include <QMessageBox>
#include <sstream>

#include <iostream>

#include <unistd.h>

#include <QCursor>
#include <QGLWidget>

//a client window wraps a WebView (from QGraphicsWebView) object.
//At the top level, clients that request new frames are granted
//system-level windows instead of frames managed by other clients.

ContainerTopLevel::ContainerTopLevel(Connection* bridge, std::string clientName, 
                      int themeIndex) : Container(bridge, clientName, themeIndex) {
    isTopLevel = true;
    initYet = false;
    w = new WebWindow(this);
    frame = w->webView->webItem->page()->mainFrame();

    //set default toplevel icon to Hipe icon in case run in a desktop environment.
    setIcon((char*) hipeicon_rootless128_png, hipeicon_rootless128_png_len);


    //set up context menu events. These are always handled by the top level frame on behalf of all child frames.
    //This is partly because frame->page() points to a common object for all child frames of a tl frame in webkit anyway.
    frame->page()->setContextMenuCallback(contextMenuTriggered, (void*) this);
}

ContainerTopLevel::~ContainerTopLevel() {
    frame->page()->clearContextMenuCallback();

    delete w;
}

Container* ContainerTopLevel::getParent()
{
    return nullptr; //a top level container has no parent.
}

void ContainerTopLevel::setTitle(std::string newTitle) {
    w->setWindowTitle(newTitle.c_str());
}

void ContainerTopLevel::setBody(std::string newBodyHtml, bool overwrite) {
    if(!initYet) {
        webElement = w->initBoilerplate(
            std::string("<html><head><style>")
            + stylesheet
            + "</style><script>var canvascontext;</script></head><body "
            "></body></html>"
        ); //initialiser. If ommitted, resource images won't display (!)
        initYet = true;
        webElement.removeAllChildren();

        //obtain the initial colour scheme
        std::string fg, bg;
        while(!fg.size())  //the frame might not be rendered straight away; during this time these will return blank strings.
            fg = webElement.styleProperty("color", QWebElement::ComputedStyle).toStdString();
        fgColorChanged(fg);
        while(!bg.size())
            bg = webElement.styleProperty("background-color", QWebElement::ComputedStyle).toStdString();
        bgColorChanged(bg);

        //Only once: each requestEvent() adds another listener, and the body element (with its
        //listeners) persists for the life of the frame -- later setBody() calls only replace
        //or append to its children.
        webElement.requestEvent("keyup", (void*)this, 1, 0, _receiveKeyEventOnBody, false);
        webElement.requestEvent("keydown", (void*)this, 0, 0, _receiveKeyEventOnBody, false);
        webElement.requestEvent("dragstart", 0,0,0, _receiveDragStartEvent, true); //catch the event to override default dragging behaviour.
    }
    if(overwrite) {
        webElement.setInnerXml(newBodyHtml.c_str());
        //remove any existing body as the user wishes to overwrite it.
        //c_str() conversion is adequate here since any binary data in the
        //stylesheet will be expressed in base64 anyway.
    } else {
        webElement.appendInside(newBodyHtml.c_str());
    }
}

void ContainerTopLevel::setIcon(const char* imgData, size_t length)
{
    QPixmap iconData;
    iconData.loadFromData((const uchar*) imgData, (uint) length);
    QIcon icon(iconData);
    w->setWindowIcon(icon);
}

std::string ContainerTopLevel::getGlobalSelection(bool asHtml) {
    if(!asHtml) return frame->page()->selectedText().toStdString();
    else return frame->page()->selectedHtml().toStdString();
}

std::string ContainerTopLevel::dialog(std::string title, std::string prompt,
                    std::string choiceLines, bool editable, bool* cancelled, int* choiceNumber) {
    if(choiceNumber) *choiceNumber = 0;
    QStringList lines = ((QString)(choiceLines.c_str())).split("\n");

    //Fixed choices: blank lines only group choices (periscope shows them as empty slots, keeping each choice
    //on its key). Here, a run of blanks between choices becomes one separator, and blanks before the first
    //choice or after the last are dropped. choiceIndex maps each displayed entry to its line number, so the
    //reply numbers choices as sent (-1 marks a separator).
    QStringList items;
    std::vector<int> choiceIndex;
    std::vector<int> separatorsAt; //display positions to turn into real separators
    if(!editable) {
        bool pendingSeparator = false;
        for(int i=0; i<lines.size(); i++) {
            if(lines[i] == "") {
                if(items.size()) pendingSeparator = true;
                continue;
            }
            if(pendingSeparator) {
                separatorsAt.push_back(items.size());
                items << "\u2E3B"; //"⸻", shown if the combo box can't be reached for a real separator
                choiceIndex.push_back(-1);
                pendingSeparator = false;
            }
            items << lines[i];
            choiceIndex.push_back(i);
        }
    } else if(choiceLines.size()) {
        items = lines;
    }

    if(!items.size() && !editable) { //no choices except cancellation.
    //Display a simpler dialog box with only an OK button to dismiss
    //the dialog (functionally equivalent to cancelling the dialog)
        QMessageBox::information(this->w, QString(title.c_str()), QString(prompt.c_str()));
        *cancelled = true;
        return "";
    }

    QInputDialog input;
    input.setWindowTitle(QString(title.c_str()));
    input.setLabelText(QString(prompt.c_str()));
    input.setComboBoxItems(items);
    input.setComboBoxEditable(editable);
    QComboBox* combo = input.findChild<QComboBox*>();
    if(combo) {
        for(auto it = separatorsAt.rbegin(); it != separatorsAt.rend(); ++it) { //from the end, so positions hold
            combo->removeItem(*it);
            combo->insertSeparator(*it);
        }
        combo->setCurrentIndex(0); //the first real choice
    }

    bool ok = input.exec() == QDialog::Accepted;
    std::string item = input.textValue().toStdString();
    int selected = combo ? combo->currentIndex() : -1;
    if(!ok || (!editable && (selected < 0 || selected >= (int)choiceIndex.size() || choiceIndex[selected] < 0))) {
        *cancelled = true; //cancelled, or (without real separators) a separator was chosen
        return "";
    }
    *cancelled = false;
    if(!editable) {
        item = lines[choiceIndex[selected]].toStdString();
        if(choiceNumber) *choiceNumber = choiceIndex[selected] + 1;
    }
    return item;
}


std::string ContainerTopLevel::selectFileResource(std::string defaultName, std::string metadata, std::string& accessMode) {
//accessModeStr will be modified by ref. to remove items not available. In particular, random seek modes are not
//available since we are dealing with native files (not a FIFO-host) and we do not intercept such requests.
//(The client does not know that these are native files and is thus forbidden from using calls like lseek)

    //check access modes and order of precedence... Only r and w are supported.
    bool r=false;
    bool w=false;
    size_t readPosition, writePosition;
    readPosition = accessMode.find("r");
    writePosition = accessMode.find("w");
    if(readPosition != std::string::npos) r = true;
    if(writePosition != std::string::npos) w = true;

    bool initialRead = r && (!w || readPosition < writePosition);

    if(!(r|w)) { //error, none of the specified access modes are r or w.
        QMessageBox::information(this->w, "Error", "Required access mode not supported");
        return "";
    }
    
    //parse out the metadata...
    std::stringstream ss(metadata); //allow reading incrementally as a stream
    std::string caption;
    std::getline(ss,caption); //the first line is the dialog caption. (src, dest)

    //now for the remaining lines in the metadata...
    bool oneGoodPattern=false; //if we have at least one supported pattern, this becomes true.
    std::string metaLine;
    std::string qtFilterStr;
    while(std::getline(ss,metaLine)) {

        //split into 2 strings by the Colon : -- left string is extension patterns. right string is description.
        size_t splitPosition = metaLine.find(":");
        std::string pattern, description;
        if(splitPosition != std::string::npos) { //":" was found
            pattern = metaLine.substr(0,splitPosition);
            description = metaLine.substr(splitPosition+1);
        } else { //":" was not found.
            pattern = metaLine; //the pattern must then be the whole thing.
            description = "";
        }

        if(pattern.find("/") != std::string::npos) continue; //skip whole-directory patterns--don't support here
        else oneGoodPattern = true;

        std::string filterEntry; //in here, compose a filter entry in the format Qt requires.
        filterEntry = description + " (";

        //for extension patterns, split by semicolons. (e.g. "bmp;jpg;jpeg;gif")
        size_t startPosition=0; //the starting index of the next file type pattern
        size_t endPosition;
        do {
            endPosition = pattern.find(";",startPosition);
            if(endPosition == std::string::npos) endPosition = pattern.size();
            //technically we have found end position +1 (we don't want the semicolon or end of str itself)

            std::string patternSegment = pattern.substr(startPosition, endPosition-startPosition);

            //stick the patternSegment into Qt format and build it into the filter.
            //A lone "*" means any file, including files with no extension, so it must not become "*.*".
            if(patternSegment == "*") filterEntry += "* ";
            else filterEntry += "*." + patternSegment + " ";

            //next segment will start after endPosition;
            startPosition = endPosition+1;
        } while(startPosition < pattern.size());

        filterEntry += ")";

        //ignore patterns that contain "/" -- opening a directory this way is unsupported.

        //reformat back into a Qt style filter string: Description (*.aaa *.bbb);;Next filer (*.* *.abc);;...

        if(qtFilterStr.size() > 0) qtFilterStr += ";;";
        qtFilterStr += filterEntry;

    }

    if(!oneGoodPattern) {
        //if the client only wants a whole directory, like a project directory, we can't service that
        //at the top level, since can only provide files, not 'smart' FIFOs here.
        QMessageBox::information(this->w, "Error", "Required access type not supported");
        return "";
    }


    QString filename;
    if(initialRead)
        filename = QFileDialog::getOpenFileName(this->w, caption.c_str(), defaultName.c_str(), qtFilterStr.c_str());
    else
        filename = QFileDialog::getSaveFileName(this->w, caption.c_str(), defaultName.c_str(), qtFilterStr.c_str());
    //it seems that Qt always returns the absolute path, which is exactly what we promise to give the user.
    //(relative paths would be a problem if hiped has a different working directory to the application)
    //(realpath() can also be used in such cases, but not required due to Qt giving us what we want.)

    accessMode = (r && w) ? (initialRead ? "rw" : "wr") : (r ? "r" : "w");

    return filename.toStdString();
}


void ContainerTopLevel::editContextMenuRequested() {
    receiveMessage(HIPE_OP_EDIT_CONTEXT_REQUEST, 0, {"","","",""}, nullptr);
}



WebWindow::WebWindow(Container* cc)
{
    this->cc = cc;

    setWindowTitle("Hipe client");

    if(fillscreen) {
    //in the absense of a window manager, this is a 'root' window that fills
    //the screen.
        move(0,0);
        resize(QApplication::primaryScreen()->availableSize().width(),
                        QApplication::primaryScreen()->availableSize().height());
    }

    webView = new WebGraphicsView();
    setCentralWidget(webView);

    // Set the background color explicitly for the window and view
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::white);
    setPalette(pal);
    webView->setPalette(pal);

    webView->setFocus();

    // Install event filter for capturing events
    installEventFilter(this);

    //if hipe cursor management enabled, create and add the cursor to the scene
    if(embedCursor) {
        mCursor = new MouseCursor(cc, webView);
    }

    //Don't display the window yet, until boilerplate markup is added in initBoilerplate.
    //This should reduce a little flicker (e.g. white background appears for an instant before being restyled.)
}

bool WebWindow::eventFilter(QObject* obj, QEvent* event)
{
    // Let the default handler process the event first
    return QMainWindow::eventFilter(obj, event);
}

QWebElement WebWindow::initBoilerplate(std::string html)
//If at least the boilerplate "<html><head></head><body></body></html>" is not specified,
//WebKit doesn't behave itself, for example Qt resource images do not display.
//RETURNS: the <body> element in the HTML boilerplate, where content can now be placed.
{
    webView->webItem->setHtml(html.c_str());
    if(fillscreen) //Now we're ready to show the window on the screen.
        showFullScreen();
    else
        show();
    QWebElement we = webView->webItem->page()->mainFrame()->documentElement();

    return we.lastChild(); //body tag becomes the webElement in the base class.
}

void WebWindow::resizeEvent(QResizeEvent*) {
    webView->resize(centralWidget()->size());
}

void WebWindow::closeEvent(QCloseEvent* event)
{
    cc->containerClosed();
    event->ignore();
}

WebGraphicsView::WebGraphicsView() : QGraphicsView()
{
    // Create a scene with a solid background color
    scene = new QGraphicsScene(this);
    scene->setBackgroundBrush(Qt::white);
    setScene(scene);
    
    // Configure view settings for compatibility
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    // Configure OpenGL rendering if enabled
    if (useOpenGL) {
        QGLWidget* glWidget = new QGLWidget(QGLFormat(QGL::SampleBuffers));
        glWidget->setAutoFillBackground(false);
        setViewport(glWidget);
        
        // Additional OpenGL-specific optimizations
        setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);
    } else {
        // Standard rendering settings for software mode
        setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    }
    
    setCacheMode(QGraphicsView::CacheBackground);
    
    // Basic rendering hints
    setRenderHint(QPainter::Antialiasing, true);
    setRenderHint(QPainter::SmoothPixmapTransform, true);
    
    // Create the QGraphicsWebView and add it to the scene
    webItem = new WebView();
    scene->addItem(webItem);
    //Make the web item the scene's focus item. QGraphicsView only passes key events on to the
    //scene's focus item, and WebKit only treats the page as focused (so that SET_FOCUS places a
    //working caret) once that item receives focus. Without this, nothing had focus until the user
    //clicked in the window: typing was dropped, even with no window manager to take focus away.
    webItem->setFocus();

    // Enable basic hardware acceleration features
    QWebSettings* settings = webItem->page()->settings();
    
    if (useOpenGL) {
        settings->setAttribute(QWebSettings::AcceleratedCompositingEnabled, true);
    } else {
        settings->setAttribute(QWebSettings::AcceleratedCompositingEnabled, false);
    }
    
    settings->setAttribute(QWebSettings::WebGLEnabled, true);  // Keep WebGL enabled for content
    settings->setAttribute(QWebSettings::TiledBackingStoreEnabled, useOpenGL);
    
    // Properly size the web item
    webItem->resize(size());
}

void WebView::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsWebView::mousePressEvent(event);
    //WebKit has now moved focus to the frame that was clicked.
    Container::noteUserInput(page()->currentFrame());
}

void WebView::keyPressEvent(QKeyEvent* event) {
    Container::noteUserInput(page()->currentFrame());
    QGraphicsWebView::keyPressEvent(event);
}

bool WebView::focusNextPrevChild(bool next) {
//Qt sends Tab and Shift+Tab here instead of to keyPressEvent(). The page handles them (moving the focus between its
//elements, or not, if the client cancels it), and they never move the focus away from the page: it's the only item
//in the window. (Returning false did that, and Qt then delivered the same Tab to the page a second time.)
    Container::noteUserInput(page()->currentFrame());
    QGraphicsWebView::focusNextPrevChild(next);
    return true;
}

void WebGraphicsView::resizeEvent(QResizeEvent* event)
{
    QGraphicsView::resizeEvent(event);
    
    if (webItem) {
        webItem->resize(size());
        webItem->setPos(0, 0);
        scene->setSceneRect(0, 0, width(), height());
    }
}

