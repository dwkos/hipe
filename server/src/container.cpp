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

#include "container.h"
#include "connection.h"
#include "containerframe.h"
#include "containertoplevel.h"
#include "sanitation.h"
#include "main.hpp"
#include "instructionhandler.h"

#include <HipeCore/QWebPage>
#include <QInputMethodEvent>
#include <stdio.h>

#include <iostream>

std::string Container::globalStyleSheets[];
int Container::maxLoadedStyleSheetIndex = 0;

Container::Container(Connection* bridge, std::string clientName, int themeIndex) : QObject()
{
    this->client = bridge;

    keyList = new KeyList(clientName);

    if(themeIndex > maxLoadedStyleSheetIndex || themeIndex < 0) {
    //if the requested theme index was not allocated, default to 1 (default loaded theme)
    //or 0 (no themes loaded).
        themeIndex = (maxLoadedStyleSheetIndex > 0) ? 1 : 0;
    }
    stylesheet = globalStyleSheets[themeIndex].c_str();
    //initialise our stylesheet rules to any global rules that have been loaded
    //in from a CSS file.

    stylesheet += " ";
}

Container::~Container()
{
    delete keyList;
}

void Container::applyStylesheet() {

    if(!initYet) return;
    //no-op. Styles will be applied in the <head> when setBody is called.

    //appending new style rules after </head> is not supposed to be valid, but we might get away with it.
    webElement.appendInside(QString("<style>") + stylesheet.c_str() + "</style>");
    stylesheet = ""; //clear after application.
}


void Container::receiveInstruction(hipe_instruction instruction)
//POLICY NOTES: Qt's webkit DOM functions require QStrings extensively. However
//we prefer to avoid coupling our application too closely to Qt due to the
//Qt Company's neglect of webkit bindings.
//THEREFORE, use C++11 standard types where possible/efficient to do so,
//and only convert to QString type where it is necessary to do so.
{

    //uint64_t requestor = instruction.requestor;
    bool locationSpecified = (bool) instruction.location;
    //if location not specified, set it to the body element of the container.
    QWebElement location = locationSpecified ? getReferenceableElement(instruction.location)
                                             : webElement; //may need to update this after calling setBody!!

    invoke_handler(this, &instruction, locationSpecified, location);
    return;
}

void Container::containerClosed()
//Called when the container is requested to be closed by the user.
//(We need to disconnect the connection to the client and free all the
//associated resources of this instance.)
//This function sends a message to the client to request disconnection at the
//client's end. The client needs to check for this message and deal with it.
{
    //client->deleteLater();
    client->sendInstruction(HIPE_OP_FRAME_CLOSE, 0, 0);
}

Container* Container::requestNew(std::string key, std::string clientName, 
                                uint64_t pid, int themeIndex, Connection* c) {
    if(keyList->claimKey(key)) {
        //find the relevant frame in the subFrames list.
        for(FrameData& fd : subFrames) {
            if(fd.hostKey.toStdString() == key) { //found it.
                fd.claimed = true; //mark as claimed.
                fd.hostKey = ""; //now claimed, key not reusable.
                fd.clientName = clientName;
                fd.title = clientName;
                fd.pid = pid;
                receiveSubFrameEvent(HIPE_FRAME_EVENT_CLIENT_CONNECTED, fd.wf, clientName);
                return (Container*) new ContainerFrame(c, clientName, fd.wf, themeIndex, this);
            }
        }
    }
    return nullptr;
}

void Container::receiveSubFrameEvent(short evtType, QWebFrame* sender, std::string detail)
//called from a sub-frame when an event affecting that subframe takes place.
{
    std::string evtTypeString = " "; evtTypeString[0] = (char) evtType;

    //find the sender in subFrames list...
    for(FrameData& sf : subFrames) {
        if(sf.wf == sender) { //found it.
            if(evtType == HIPE_FRAME_EVENT_TITLE_CHANGED)
                sf.title = detail;
            else if(evtType == HIPE_FRAME_EVENT_BACKGROUND_CHANGED) {
                if(detail.compare(sf.bg) != 0) { 
                //check frame metadata in sf, if background has not changed, return without sending event.
                    sf.bg = detail;
                } else
                    return; //no change to background colour.
            } else if(evtType == HIPE_FRAME_EVENT_COLOR_CHANGED) { //foreground colour
                if(detail.compare(sf.fg) != 0) { //similarly as for background colour
                    sf.fg = detail;
                } else
                    return; //no change to foreground colour.
            }

            if(evtType == HIPE_FRAME_EVENT_CLIENT_CONNECTED) //this event has an extra detail arg: the process ID.
                client->sendInstruction(HIPE_OP_FRAME_EVENT, sf.requestor, findReferenceableElement(sf.we),
                                    {evtTypeString, detail, std::to_string(sf.pid)});
            else
                client->sendInstruction(HIPE_OP_FRAME_EVENT, sf.requestor, findReferenceableElement(sf.we),
                                    {evtTypeString, detail});

            if(evtType == HIPE_FRAME_EVENT_CLIENT_DISCONNECTED) {
                sf.claimed = false; //mark as no longer claimed by a client.
                //sf.wf = nullptr; //clear the QWebFrame pointer which is no longer valid. (isn't it?)
                sf.clientName = ""; //clear the client name.
                sf.title = ""; //clear the title.
                sf.pid = 0; //clear the process ID.
            }

            break;
        }
    }
}

void Container::receiveMessage(char opcode, int64_t requestor, const std::vector<std::string>& args, QWebFrame* sender, bool propagateToParent) {
//If the sender is the parent of this frame, a nullptr should be passed as sender.
//If the sender is a child frame, we'll resolve the child frame's location from the perspective of this frame.
//If propagateToParent is set, all parents of this container will see this message as originating from their relevant child frame.

    size_t location = 0;

    //if it came from the parent we send a 0 for location. Otherwise we need to identify the child frame it came from.
    if(sender) { //need to resolve location of child frame that sent this.
        FrameData* sf = lookupSubFrame(sender);
        if(sf) location = getIndexOfElement(sf->we);
    }

    client->sendInstruction(opcode, requestor, location, args);

    //propagate to parent (and grandparent, etc.) if flag specified.
    if(propagateToParent && getParent())
        getParent()->receiveMessage(opcode, requestor, args, webElement.webFrame(), true);
}

void Container::keyEventOnChildFrame(QWebFrame* origin, bool keyUp, QString keycode) {
//if keyup is false, it was a keydown event.
//This function is called from a child container instructing this container that a keyup/keydown event has
//occurred on the body element of this frame (or has propagated from a child frame of *that* frame).
//The event should be propagated up to the top level so the framing manager can intercept global keyboard shortcuts.
//It should also trigger a simulated event on the frame to this client, if this client has requested keydown/keyup
//events on this frame.

    size_t location=0;
    QWebElement childFrame;

    FrameData* sf = lookupSubFrame(origin);
    if(sf) {
        location = getIndexOfElement(sf->we);
        childFrame = sf->we; //get the web element of the child frame.
    }

    //Determine if the client has requested keydown/keyup events on this element.
    //Fire off an event if so.
    if(keyUp && childFrame.handlesEvent("keyup"))
        client->sendInstruction(HIPE_OP_EVENT, (sf ? sf->keyUpRequestor : 0), location, {"keyup", keycode.toStdString()});
    else if(!keyUp && childFrame.handlesEvent("keydown"))
        client->sendInstruction(HIPE_OP_EVENT, (sf ? sf->keyDownRequestor : 0), location, {"keydown", keycode.toStdString()});

    //propagate the key event to the body element too, for global key handling.
    //This also propagates the key event to the parent frame of this one, etc.
    _receiveKeyEventOnBody(keyUp ? "keyup" : "keydown", (void*)this, keyUp, 0, keycode);

    //if(getParent()) { //propagate this up to *our* parent and so on, in case they need this keyboard event.
    //    getParent()->keyEventOnChildFrame(webElement.webFrame(), keyUp, keycode);
    //}
}


char Container::editActionStatus(char action) {
//for a particular action (e.g. 'x' is cut, 'i' is italic toggle, etc.
//returns a char to indicate the status of that action:
//'0' -- available and not toggled
//'1' -- available and toggled
//'e' -- not enabled/not applicable in current context
//The returned status will depend on what element the user currently has
//focused and whether the user has selected content.

    if(action == 't') //insert text has no QAction of its own; it's available wherever a line break could be typed.
        return frame->page()->action(QWebPage::InsertLineSeparator)->isEnabled() ? '0' : 'e';

    auto actionObj = getEditQtAction(action);
    if(!actionObj) return 'e'; //not an edit code we know
    if(!actionObj->isEnabled()) return 'e'; //not enabled
    if(actionObj->isChecked()) return '1';
    else return '0';
}

void Container::triggerEditAction(char action) {
//the action to be done is specified by a char: 'x', 'c', 'v' or 'V'
    auto actionObj = getEditQtAction(action);
    if(actionObj) actionObj->trigger();
}

void Container::insertText(const std::string& text) {
    QWebPage* page = frame->page();
    //The text is inserted as typing would insert it: it replaces the selection, can be undone, and does nothing
    //where the user couldn't type. Newlines become line breaks, and where the content keeps its white space
    //(e.g. a <pre>) the whole text goes in as a single edit, however many lines it has. Empty text deletes
    //the selected text.
    page->insertText(QString::fromUtf8(text.data(), text.size()));
}

bool Container::containsFrame(QWebFrame* f) {
    for(; f; f = f->parentFrame())
        if(f == frame) return true;
    return false;
}

void Container::noteUserInput(QWebFrame* f) {
    //find the container that owns f (f may be a plain frame inside a client's document).
    for(; f; f = f->parentFrame()) {
        Connection* conn = identifyFromFrame(f);
        if(conn && conn->container) {
            conn->container->noteUserInput();
            return;
        }
    }
}

void Container::noteUserInput() {
    if(!isTopLevel) //input at the top level says nothing about which framed app the user meant.
        lastUserInput = std::chrono::steady_clock::now();
}

bool Container::hadRecentUserInput() {
    const auto window = std::chrono::seconds(3);
    const auto now = std::chrono::steady_clock::now();
    for(Container* x = this; x && !x->isTopLevel; x = x->getParent())
        if(x->lastUserInput.time_since_epoch().count() && now - x->lastUserInput < window)
            return true;
    return false;
}

QAction* Container::getEditQtAction(char action) {
    return frame->page()->action(Sanitation::editCodeLookup(action));
}


void Container::_receiveKeyEventOnBody(const QString& eventName, void* containerPtr, uint64_t isKeyUp, uint64_t requestor, const QString& eventDetails)
//keyup and keydown events are treated as a special case when they happen on the body element.
//The event is not sent to the client directly; instead this callback is ALWAYS called, since we want to receive
//the event and propagate it up the client tree regardless of whether the user has asked to be notified of it.
{
    Container* _this = (Container*) containerPtr;

    if(isKeyUp && _this->reportKeyupOnBody)
        Connection::_receiveUIEvent(eventName, _this->client, 0, _this->keyUpOnBodyRequestor, eventDetails);
    else if(!isKeyUp && _this->reportKeydownOnBody)
        Connection::_receiveUIEvent(eventName, _this->client, 0, _this->keyDownOnBodyRequestor, eventDetails);

    // the whole point of this function is that we'll now notify the parent of the event.
    // if the parent has requested keydown or keyup events on this frame, we'll fire off an event on that iframe.
    // Regardless, we then propagate to *that* element's parent as well.
    if(_this->getParent()) { //propagate this up to *our* parent and so on, in case they need this keyboard event.
        _this->getParent()->keyEventOnChildFrame(_this->webElement.webFrame(), (bool)isKeyUp, eventDetails);
    }

}


void Container::frameDestroyed()
{
    frame = nullptr;
    //delete client; //not here -causes 'circular' cleanup.
    client->disconnect();  //the connection will now be cleaned up in next cycle.
    //in turn, the defunct connection will destroy this container object.
}


size_t Container::assignElementIndex(const QWebElement& w, size_t newIndex) {
//Manually assigns the given index to the element, allowing any index value that is
//unused and does not exceed the previous maximum index value by 1.
//returns 0 if unable to assign.
    if(newIndex < 1) return 0;

    //grow the array if needed and within allowed range
    size_t oldSize = referenceableElement.size();
    if(newIndex > oldSize) {
        if(newIndex > maxElementIndexUsed+1)
            return 0; //invalid, requested index is too large.
        referenceableElement.setSize(newIndex);
        for(size_t i=oldSize+1; i<=newIndex; i++)
            referenceableElement[i]=nullptr;
        //fill new elements to nullptr
        if(maxElementIndexUsed < newIndex) maxElementIndexUsed = newIndex;
    }

    //check if requested element is free, assign if so.
    if(referenceableElement[newIndex]) {
        //not free!
        return 0;
    }
    referenceableElement[newIndex] = new QWebElement(w);
    
    return newIndex;

}



bool Container::bgColorChanged(std::string newColor) {
    int r,g,b;

    //require 3 color components.
    if(sscanf(newColor.c_str(), "rgb(%d, %d, %d)", &r,&g,&b) != 3) {
        //not in expected form or background unspecified...
        return false;
    }

    QColor c(r,g,b);
    if(c==bg) return false; //no change
    bg=c;

    if(cursorSymbol.size()) {
        webElement.setStyleProperty("cursor", "none"); //cursor has to be reset before changing it.
        webElement.setStyleProperty("cursor", Sanitation::mouseCursorFromUnicode(cursorSymbol, fg.name().toStdString(), bg.name().toStdString(), cursorHotspot).c_str());
    }
    
    c.setAlpha(127);
    cursorColor2=c; //used for crosshair cursor mode only

    return true;
}

bool Container::fgColorChanged(std::string newColor) {
    int r,g,b;

    //require 3 color components.
    if(sscanf(newColor.c_str(), "rgb(%d, %d, %d)", &r,&g,&b) != 3) {
        //not in expected form or background unspecified...
        return false;
    }

    QColor c(r,g,b);
    if(c==fg) return false; //no change
    fg=c;

    if(cursorSymbol.size()) {
        webElement.setStyleProperty("cursor", "none"); //cursor has to be reset before changing it.
        webElement.setStyleProperty("cursor", Sanitation::mouseCursorFromUnicode(cursorSymbol, fg.name().toStdString(), bg.name().toStdString(), cursorHotspot).c_str());
    }
    
    cursorColor3=c;
    c.setAlpha(63);
    cursorColor1=c;
    
    return true;
}

void Container::removeReferenceableElement(size_t i)
{
    if(i>referenceableElement.size() || i<1) return;
    delete referenceableElement[i];
    referenceableElement[i] = nullptr;
    if(i < firstFreeElementAfter) firstFreeElementAfter = i;

    if(i==referenceableElement.size()) {
    //might be an opportunity to truncate the array if one or more elements
    //at the end are now vacant.
        while(i > 0 && !referenceableElement[i]) //element [0] is unused, so stop there.
            i--;
        referenceableElement.setSize(i);
        //shrink the array if the last element was removed. (The underlying
        //class then optimises this to avoid too much actual resizing.)
    }

}

QWebElement Container::getReferenceableElement(size_t i)
{
    if(i>referenceableElement.size() || !referenceableElement[i]) {
        return QWebElement();
        //A location the client was never given, or has freed, refers to no element: instructions sent
        //to it do nothing. (A client can easily hold a stale location, so this isn't treated as a fault.)
    } else if(i <= 0) {
        return QWebElement();
        //there is no zeroth element. Reserve it to mean 'not applicable'.
    } else if(referenceableElement[i]) {
        return *referenceableElement[i];
    }
    else return QWebElement();
}

size_t Container::findReferenceableElement(const QWebElement& we) {
    if(we.isNull()) return 0;
    for(size_t i=referenceableElement.size(); i>=1; i--) {
    //count down so that the last-added reference is more likely to be checked first.
        if(referenceableElement[i] && *(referenceableElement[i])==we)
            return i;
    }
    return 0;
}

size_t Container::getIndexOfElement(const QWebElement& location)
{
    size_t locElement = 0;
    if(!location.isNull()) {
        locElement = findReferenceableElement(location);
        //check if the element is already on the list

        if(!locElement) return 0;
        //return 0 if the element doesn't have an assigned index/location ID
    }
    return locElement;
}


bool FrameData::operator==(const FrameData& other) {
    return (this->we == other.we);
}


void Container::cleanUpSubFrames() {
//removes any subframes that no longer exist in the document.

    bool itemRemoved = false;
    
    do {
        itemRemoved = false; //reset the flag for this iteration.
        for(FrameData& fd : subFrames) {
            bool found = false;
            auto frames = webElement.webFrame()->childFrames();
            for(QWebFrame* frame : frames) {
                if(fd.wf == frame) {
                    found = true; //found the frame, so it still exists.
                    break;
                }
            }
            if(!found) {
                //frame no longer exists, remove it from the subFrames list.
                itemRemoved = true; //at least one item was removed.
                subFrames.remove(fd);
                break; //break to avoid iterator invalidation after removing an item.
            }
        }
    } while(itemRemoved); //repeat until no more items are removed.
}


void Container::registerNewFrame(QWebFrame* wf) {
    Connection* conn = identifyFromFrame(wf->parentFrame());
    if(conn && conn->container)
        conn->container->addNewSubFrame(wf);
}

void Container::addNewSubFrame(QWebFrame* wf) {
    //do preliminary cleanup of any subframes that no longer exist.
    cleanUpSubFrames();
    //wf is a new frame, so an entry that already has its address belongs to a deleted frame.
    subFrames.remove_if([wf](const FrameData& fd) { return fd.wf == wf; });

    //Create a new FrameData object for this subframe.
    FrameData fd;
    fd.we = wf->ownerElement(); //the <iframe> element.
    fd.wf = wf;
    fd.claimed = false; //not connected yet.
    fd.hostKey = ""; //hostkey will be generated on request.
    fd.requestor = 0; //no requestor yet,
    fd.keyUpRequestor = 0; //no keyup requestor yet,
    fd.keyDownRequestor = 0; //no keydown requestor yet,
    fd.clientName = ""; //no client name yet,
    fd.title = ""; //no title yet,
    fd.pid = 0; //no process ID yet,
    fd.fg = ""; //no foreground colour yet,
    fd.bg = ""; //no background colour yet.

    subFrames.push_back(fd); //add new entry to the table.
}

FrameData* Container::lookupSubFrame(const QWebElement& we) {
    //To be called when a subframe is looked up by its <iframe> element.
    //Returns a pointer to the FrameData object for that frame, or nullptr if not found.

    for(FrameData& fd : subFrames) {
        if(fd.we == we) {
            return &fd; //found it.
        }
    }
    return nullptr; //not found.
}

FrameData* Container::lookupSubFrame(QWebFrame* wf) {
    //To be called when a subframe is looked up by its QWebFrame object.
    //Returns a pointer to the FrameData object for that frame, or nullptr if not found
    for(FrameData& fd : subFrames) {
        if(fd.wf == wf) {
            return &fd; //found it.
        }
    }
    return nullptr; //not found.
}


void Container::contextMenuTriggered(void* userPtr, const QPoint&, const QWebElement&) {
    Container* c = (Container*) userPtr;
    if(!c) return;

    c->editContextMenuRequested();
}

