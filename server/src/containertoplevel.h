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

#ifndef CLIENTWINDOW_H
#define CLIENTWINDOW_H

#include <QMainWindow>
#include <HipeCore/QWebView>
#include <HipeCore/QWebElement>
#include <QCloseEvent>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <HipeCore/QGraphicsWebView>
#include <QDialog>
#include <deque>
#include <functional>

#include "container.h"

// Forward declarations
class Container;
class MouseCursor;

class WebView : public QGraphicsWebView {
    Q_OBJECT
public:
    WebView() : QGraphicsWebView() {}
protected:
    //record which frame the user clicked or typed in (see Container::noteUserInput()).
    void mousePressEvent(QGraphicsSceneMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    bool focusNextPrevChild(bool next) override;
};

class WebGraphicsView : public QGraphicsView {
    Q_OBJECT
public:
    WebGraphicsView();
    QGraphicsWebView* webItem;
    QGraphicsScene* scene;
    
protected:
    void resizeEvent(QResizeEvent* event);
    void focusInEvent(QFocusEvent* event) override;
    bool viewportEvent(QEvent* event) override;
private:
    void keepItemFocus();
};

class WebWindow : public QMainWindow {
    Q_OBJECT
public:
    WebWindow(Container* cc);

    QWebElement initBoilerplate(std::string html);

    WebGraphicsView* webView;
    QPalette pal;
    MouseCursor* mCursor;

private:
    Container* cc;

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

public slots:
    void resizeEvent(QResizeEvent *);
    void closeEvent(QCloseEvent *);
};



//--------------------------
//THE CLASS YOU'RE HERE FOR:

class ContainerTopLevel : public Container
{

public:
    explicit ContainerTopLevel(Connection*, std::string clientName, int themeIndex);
    ~ContainerTopLevel();

    Container* getParent();

    std::string getGlobalSelection(bool asHtml);
    //if asHtml is false, the selection is returned as plain text.

    //The dialogs below are modal to this window only, and never block hiped: each call returns at once and
    //the reply is sent to the client when the user answers. A window shows one dialog at a time; requests
    //made meanwhile wait their turn. Unanswered dialogs go away with the window.

    void showDialog(int64_t requestor, std::string title, std::string prompt, std::string choices, bool editable);
    //HIPE_OP_DIALOG (editable false) or HIPE_OP_DIALOG_INPUT (true). choices is newline-separated.
    //Replies HIPE_OP_DIALOG_RETURN: the choice selected or entered, and for fixed choices the chosen line's
    //number in choices (from 1, blank lines counted, so it matches the numbering under a framing manager),
    //"1" for entered text, or "" and "0" if cancelled.

    void selectFileResource(int64_t requestor, std::string defaultName, std::string metadata, std::string accessMode);
    //implements top-level FIFO functionality through the OS's native open/save as interface.
    //Ordinary files are used instead of real FIFOs here.
    //The metadata argument spans multiple lines and contains caption and file filter info,
    //consistent with what's passed to HIPE_OP_FIFO_GET_PEER. Replies HIPE_OP_FIFO_RESPONSE.

    void editContextMenuRequested();
private:
    WebWindow* w;

    std::deque<std::function<QDialog*()>> dialogQueue; //each entry builds a dialog, its reply already connected
    //to finished(), or sends its reply itself and returns nullptr.
    QDialog* activeDialog = nullptr;
    void queueDialog(std::function<QDialog*()> make);
    void showNextDialog();
    void sendFileResponse(int64_t requestor, std::string filepath, std::string accessMode);
protected:
    void setTitle(std::string newTitle);
    void setBody(std::string newBodyHtml, bool overwrite=true);
    void setIcon(const char* imgData, size_t length);
};

#endif // CLIENTWINDOW_H
