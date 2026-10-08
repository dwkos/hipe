/*  Copyright (c) 2016-2026 Daniel Kos, General Development Systems

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

#include "containerframe.h"
#include "connection.h"
#include <HipeCore/QWebPage>

//ContainerFrame is an alternative version of ContainerTopLevel, that exists within an iframe
//of another.

ContainerFrame::ContainerFrame(Connection* bridge, std::string clientName, 
                QWebFrame* frame, int themeIndex, Container* parent)
                 : Container(bridge, clientName, themeIndex) {
    initYet = false;
    this->frame = frame;
    this->parent = parent;

    connect(frame, SIGNAL(destroyed()), this, SLOT(frameDestroyed()));
}

ContainerFrame::~ContainerFrame()
{
    if(frame) {
        frame->setHtml(""); //clear the frame's contents if it still exists.
        if(parent)
            parent->receiveSubFrameEvent(HIPE_FRAME_EVENT_CLIENT_DISCONNECTED, frame, "");
    }
}

Container* ContainerFrame::getParent()
{
    return parent;
}

void ContainerFrame::setBody(std::string newBodyHtml, bool overwrite)
{
    if(!parent || !frame) return;
    if(!initYet) {
        frame->setHtml(QString("<html><head><style>") + stylesheet.c_str() + "</style></head><body></body></html>");
        stylesheet = ""; //clear already-applied stylesheet data.
        webElement = frame->documentElement().lastChild();
        initYet = true;

        //alert parent of fg/bg colour scheme
        //(computed styles are worked out when asked for, so these are ready straight away.)
        std::string fg = webElement.styleProperty("color", QWebElement::ComputedStyle).toStdString();
        fgColorChanged(fg);
        std::string bg = webElement.styleProperty("background-color", QWebElement::ComputedStyle).toStdString();
        bgColorChanged(bg);

        getParent()->receiveSubFrameEvent(HIPE_FRAME_EVENT_BACKGROUND_CHANGED, frame, bg);
        getParent()->receiveSubFrameEvent(HIPE_FRAME_EVENT_COLOR_CHANGED, frame, fg);

        //Only once: each requestEvent() adds another listener, and the body element (with its
        //listeners) persists for the life of the frame -- later setBody() calls only replace
        //or append to its children.
        webElement.requestEvent("keyup", (void*)this, 1, 0, _receiveKeyEventOnBody, false);
        webElement.requestEvent("keydown", (void*)this, 0, 0, _receiveKeyEventOnBody, false);
        webElement.requestEvent("dragstart", 0,0,0, _receiveDragStartEvent, true); //catch the event to override default dragging behaviour.
        //Presses anywhere in the frame bubble up to <html>, which clients can't address, so their own requests for
        //and cancellations of mouse events never touch these. (setHtml("") in the destructor removes them.)
        QWebElement html = frame->documentElement();
        html.requestEvent("mousedown", (void*)this, 0, 0, _receiveMouseEventOnDocument, false);
        html.requestEvent("mouseup", (void*)this, 0, 0, _receiveMouseEventOnDocument, false);
    }
    if(overwrite) webElement.setInnerXml(newBodyHtml.c_str());
    else webElement.appendInside(newBodyHtml.c_str()); //c_str() conversion is adequate since any binary data will be in safe base64 encoding.
}

void ContainerFrame::setTitle(std::string newTitle)
{
    if(!parent || !frame) return;
    parent->receiveSubFrameEvent(HIPE_FRAME_EVENT_TITLE_CHANGED, frame, newTitle);
}

void ContainerFrame::setIcon(const char* imgData, size_t length)
{
    if(!parent || !frame) return;
    parent->receiveSubFrameEvent(HIPE_FRAME_EVENT_ICON_CHANGED, frame, std::string(imgData, length));
}

