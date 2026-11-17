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

#include <HipeCore/QWebFrame>
#include <HipeCore/QWebPage>
#include <QGraphicsSceneMouseEvent>
#include <QSizePolicy>
#include <QPalette>
#include <QApplication>
#include <QScreen>
#include <QPixmap>
#include <QInputDialog>
#include <QComboBox>
#include <vector>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <sstream>

#include <iostream>

#include <unistd.h>

#include <QCursor>
#include <QOpenGLWidget>
#include <QSurfaceFormat>

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

    //register every iframe's frame with the container whose document it's in, whichever frame of this page that is.
    connect(frame->page(), &QWebPage::frameCreated, &Container::registerNewFrame);
}

ContainerTopLevel::~ContainerTopLevel() {
    frame->page()->clearContextMenuCallback();

    //the client is gone: drop its waiting dialogs, and close the open one without replying.
    dialogQueue.clear();
    if(activeDialog) {
        activeDialog->disconnect();
        delete activeDialog;
        activeDialog = nullptr;
    }

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
        webElement = w->initBoilerplate("<html><head></head><body></body></html>");
        //initialiser. If ommitted, resource images won't display (!)
        initYet = true;
        applyStylesheet(); //the theme and any rules sent so far
        webElement.removeAllChildren();
        //The body is a Tab stop, before the first element: Tab past the last element comes back to it, the same in a
        //frame or top-level. Clients opt out with tabindex -1 or by removing the attribute.
        webElement.setAttribute("tabindex", "0");

        //obtain the initial colour scheme
        //(computed styles are worked out when asked for, so these are ready straight away.)
        std::string fg = webElement.styleProperty("color", QWebElement::ComputedStyle).toStdString();
        fgColorChanged(fg);
        std::string bg = webElement.styleProperty("background-color", QWebElement::ComputedStyle).toStdString();
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

void ContainerTopLevel::queueDialog(std::function<QDialog*()> make) {
    dialogQueue.push_back(make);
    showNextDialog();
}

void ContainerTopLevel::showNextDialog() {
    while(!activeDialog && !dialogQueue.empty()) {
        std::function<QDialog*()> make = dialogQueue.front();
        dialogQueue.pop_front();
        QDialog* d = make();
        if(!d) continue; //answered without a dialog
        activeDialog = d;
        //connected after the request's own reply, so the reply goes first.
        QObject::connect(d, &QDialog::finished, d, [this, d](int) {
            if(activeDialog == d) activeDialog = nullptr;
            d->deleteLater();
            //show the next one once this dialog's own handling is over.
            QTimer::singleShot(0, w, [this]() { showNextDialog(); });
        });
        d->setWindowModality(Qt::WindowModal); //blocks this window only
        d->open();
    }
}

void ContainerTopLevel::showDialog(int64_t requestor, std::string title, std::string prompt,
                    std::string choiceLines, bool editable) {
    queueDialog([this, requestor, title, prompt, choiceLines, editable]() -> QDialog* {
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
                    items << "⸻"; //"⸻", shown if the combo box can't be reached for a real separator
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
            QMessageBox* box = new QMessageBox(QMessageBox::Information, QString(title.c_str()),
                                    QString(prompt.c_str()), QMessageBox::Ok, this->w);
            QObject::connect(box, &QDialog::finished, box, [this, requestor](int) {
                client->sendInstruction(HIPE_OP_DIALOG_RETURN, requestor, 0, {"", "0"});
            });
            return box;
        }

        QInputDialog* input = new QInputDialog(this->w);
        input->setWindowTitle(QString(title.c_str()));
        input->setLabelText(QString(prompt.c_str()));
        input->setComboBoxItems(items);
        input->setComboBoxEditable(editable);
        QComboBox* combo = input->findChild<QComboBox*>();
        if(combo) {
            for(auto it = separatorsAt.rbegin(); it != separatorsAt.rend(); ++it) { //from the end, so positions hold
                combo->removeItem(*it);
                combo->insertSeparator(*it);
            }
            combo->setCurrentIndex(0); //the first real choice
        }

        QObject::connect(input, &QDialog::finished, input,
                    [this, requestor, input, combo, lines, choiceIndex, editable](int result) {
            std::string item = input->textValue().toStdString();
            int selected = combo ? combo->currentIndex() : -1;
            if(result != QDialog::Accepted
                    || (!editable && (selected < 0 || selected >= (int)choiceIndex.size() || choiceIndex[selected] < 0))) {
                //cancelled, or (without real separators) a separator was chosen
                client->sendInstruction(HIPE_OP_DIALOG_RETURN, requestor, 0, {"", "0"});
            } else if(!editable) { //fixed choices; the number of the choice as sent (counting from 1, blanks included).
                client->sendInstruction(HIPE_OP_DIALOG_RETURN, requestor, 0,
                            {lines[choiceIndex[selected]].toStdString(), std::to_string(choiceIndex[selected] + 1)});
            } else { //prompt allowed string entry. Choice position index not relevant.
                client->sendInstruction(HIPE_OP_DIALOG_RETURN, requestor, 0, {item, "1"});
            }
        });
        return input;
    });
}


void ContainerTopLevel::sendFileResponse(int64_t requestor, std::string filepath, std::string accessMode) {
    //separate filename and extension parts...
    size_t strPos = filepath.rfind("/");
    if(strPos == std::string::npos) strPos = 0; else strPos++;
    std::string filename = filepath.substr(strPos);
    strPos = filename.rfind("."); //isolate file extension
    std::string fileType;
    if(strPos != std::string::npos) {
        fileType = filename.substr(strPos+1);
    }
    receiveMessage(HIPE_OP_FIFO_RESPONSE, requestor, {filepath, accessMode, filename, fileType}, nullptr);
}


void ContainerTopLevel::selectFileResource(int64_t requestor, std::string defaultName, std::string metadata,
                    std::string accessMode) {
//The access mode in the reply drops modes not available. In particular, random seek modes are not
//available since we are dealing with native files (not a FIFO-host) and we do not intercept such requests.
//(The client does not know that these are native files and is thus forbidden from using calls like lseek)
  queueDialog([this, requestor, defaultName, metadata, accessMode]() -> QDialog* {
    //an error is shown, then the request answered with no file and the access mode unchanged.
    auto errorBox = [this, requestor, accessMode](const char* text) -> QDialog* {
        QMessageBox* box = new QMessageBox(QMessageBox::Information, "Error", text, QMessageBox::Ok, this->w);
        QObject::connect(box, &QDialog::finished, box, [this, requestor, accessMode](int) {
            sendFileResponse(requestor, "", accessMode);
        });
        return box;
    };

    //check access modes and order of precedence... Only r and w are supported.
    bool r=false; //read or write wanted
    bool wr=false;
    size_t readPosition, writePosition;
    readPosition = accessMode.find("r");
    writePosition = accessMode.find("w");
    if(readPosition != std::string::npos) r = true;
    if(writePosition != std::string::npos) wr = true;

    bool initialRead = r && (!wr || readPosition < writePosition);

    if(!(r|wr)) { //error, none of the specified access modes are r or w.
        return errorBox("Required access mode not supported");
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
        return errorBox("Required access type not supported");
    }

    QFileDialog* fd = new QFileDialog(this->w, caption.c_str(), defaultName.c_str(), qtFilterStr.c_str());
    if(initialRead) {
        fd->setAcceptMode(QFileDialog::AcceptOpen);
        fd->setFileMode(QFileDialog::ExistingFile);
    } else {
        fd->setAcceptMode(QFileDialog::AcceptSave);
        fd->setFileMode(QFileDialog::AnyFile);
    }
    std::string grantedMode = (r && wr) ? (initialRead ? "rw" : "wr") : (r ? "r" : "w");
    QObject::connect(fd, &QDialog::finished, fd, [this, requestor, fd, grantedMode](int result) {
        //Qt gives the absolute path, which is exactly what we promise to give the user.
        //(relative paths would be a problem if hiped has a different working directory to the application)
        std::string filename;
        if(result == QDialog::Accepted && !fd->selectedFiles().isEmpty())
            filename = fd->selectedFiles().first().toStdString();
        sendFileResponse(requestor, filename, grantedMode);
    });
    return fd;
  });
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
        QOpenGLWidget* glWidget = new QOpenGLWidget;
        QSurfaceFormat format = QSurfaceFormat::defaultFormat();
        format.setSamples(4); //multisampling, as QGL::SampleBuffers gave with the old QGLWidget.
        glWidget->setFormat(format);
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

void WebGraphicsView::keepItemFocus() {
    //The web item is the only item in the scene and must always be its focus item, or key events stop at the scene.
    //A click into an inactive window (on Wayland) gives the view focus before the window is activated: the scene
    //can't hand focus to an item while it is inactive, and once activated it had no focus item left to restore.
    if(scene->isActive() && hasFocus() && scene->focusItem() != webItem)
        webItem->setFocus(Qt::ActiveWindowFocusReason);
}

void WebGraphicsView::focusInEvent(QFocusEvent* event) {
    QGraphicsView::focusInEvent(event);
    keepItemFocus();
}

bool WebGraphicsView::viewportEvent(QEvent* event) {
    bool r = QGraphicsView::viewportEvent(event); //(passes WindowActivate on to the scene)
    if(event->type() == QEvent::WindowActivate)
        keepItemFocus();
    return r;
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

