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

#ifndef CLIENTCONTAINER_H
#define CLIENTCONTAINER_H

#include <HipeCore/QWebElement>
#include <HipeCore/QWebFrame>
#include <QObject>
#include <QAction>
#include <stack>
#include <list>
#include <map>
#include <string>
#include <chrono>
#include "ExpArray.hh"
#include "common.h"
#include "keylist.h"

class Connection;

#define MAX_N_STYLESHEETS 20
//the number of elements allocated to the the stylesheets array, 
//where each element index corresponds to a different CSS theme.


class FrameData {
//stores metadata and references relevant to a particular child container.
public:
    QWebElement we; //reference to the <iframe> tag
    QWebFrame* wf; //reference to the frame object which matches the <iframe> tag.

    bool claimed = false; //true if the iframe has been claimed by a client.
    QString hostKey; //once claimed, this becomes an empty string.

    uint64_t requestor; //to be attached when sending back child frame events.
    uint64_t keyUpRequestor;
    uint64_t keyDownRequestor; //requstors for keyup and keydown events have to be
    //stored separately for iframes, since the event is generated artificially
    //by the container and not by the client application itself.
    std::string clientName;
    std::string title;
    pid_t pid; //client process ID. pid_t defined in <types.h>

    std::string fg, bg; //foreground and background colours as CSS values.

    bool operator==(const FrameData& other);
};


class Container : public QObject
{
    Q_OBJECT
public:
    friend class ContainerManager;
    static std::string globalStyleSheets[MAX_N_STYLESHEETS];
    static int maxLoadedStyleSheetIndex;


    Container(Connection* client, std::string clientName, int themeIndex);
    ~Container();

    void receiveInstruction(hipe_instruction instruction);

    virtual void setTitle(std::string newTitle)=0;
    virtual void setIcon(const char* imgData, size_t length)=0;
    virtual void setBody(std::string newBodyHtml, bool overwrite=true)=0;

    QColor fg, bg,  cursorColor1, cursorColor2, cursorColor3;
    //stores the current foreground and background colors, and derivative colors
    // for drawing secondary details like embedded mouse cursor.

    void applyStylesheet();
    //apply stylesheet after changes. If <body> was not opened yet (!initYet)
    //then this is a no-op as styling gets applied when setBody is called.
    //Otherwise, this call causes a <style> tag to be appended inside the
    //<body> tag; which is not technically valid HTML but should do the trick.

    void containerClosed();

    Container* requestNew(std::string key, std::string clientName, uint64_t pid,
                        int themeIndex, Connection* c);
    //request a new sub-frame that is managed by this container. Returns nullptr if the
    //key is not held by this container, else creates a new ContainerFrame object and
    //returns its pointer.

    void receiveSubFrameEvent(short evtType, QWebFrame* sender, std::string detail);
    //called by a sub-frame (ContainerFrame object) of this container to indicate that
    //the frame has been modified in a way that should be reported to the framing client.

    void receiveMessage(char opcode, int64_t requestor, const std::vector<std::string>& args, QWebFrame* sender, bool propagateToParent=false);
    //called by another container object to transmit an instruction (e.g. HIPE_OP_MESSAGE)
    //from a direct parent/child frame's client to this container's client.
    //sender should be used only if sender is a child of the recipient. If it's
    //the parent, this should be indicated by passing a nullptr.
    //This can also be used to transmit events across frame boundaries
    //(the parent will see the event as originating from the client frame element).

    void keyEventOnChildFrame(QWebFrame* origin, bool keyUp, QString keycode);
    //if keyup is false, it was a keydown event.
    //This function is called from a child container instructing this container
    //that a keyup/keydown event has occurred on the body element of this frame
    //(or has propagated from a child frame of *that* frame).
    //The event should be propagated up to the top level so the framing manager
    //can intercept global keyboard shortcuts.
    //It should also trigger a simulated event on the frame to this client,
    //if this client has requested keydown/keyup events on this frame.

    virtual Container* getParent()=0; //returns the parent container, or nullptr if it's a top level container.

    char editActionStatus(char action);
    void triggerEditAction(char action);
    void insertText(const std::string& text);
    //inserts text at the caret (replacing any selection) in the focused editable element, as one undoable step.
    //Empty text deletes the selected text, and does nothing if none is selected.

    bool containsFrame(QWebFrame* f);
    //true if f is this container's frame or a frame nested inside it.

    static void noteUserInput(QWebFrame* f);
    //records that the user just interacted (clicked or pressed a key) in frame f, for
    //hadRecentUserInput(). The container owning f is stamped, unless it's a top-level one.
    void noteUserInput();
    //records user input on behalf of this container, e.g. a top-level frame relaying
    //the user's answer to a dialog.
    bool hadRecentUserInput();
    //true if this container, or one of its ancestors below the top level, received user
    //input within the last few seconds. Gates actions such as paste, which would otherwise
    //let a framed app read the clipboard whenever it likes.


    bool bgColorChanged(std::string newColor);
    bool fgColorChanged(std::string newColor);
    //call to update the container on its new fg or bg color.
    //The container will store the new color and update the embedded cursor if applicable
    //Returns true if the new color is different to the old color, otherwise
    //returns false if the color has not changed.


    static void contextMenuTriggered(void* userPtr, const QPoint& globalPos, const QWebElement& element);
    virtual void editContextMenuRequested() {}


    QWebFrame* frame;

    Connection* client;
    QWebElement webElement;
    std::string stylesheet; //build up the stylesheet before we apply it.

    QWebElement currentCanvas; //the <canvas> element most recently selected by
    //HIPE_OP_USE_CANVAS, acted on by later HIPE_OP_CANVAS_ACTION/CANVAS_SET_PROPERTY
    //instructions, which the client API sends without specifying a location each time.

    bool isTopLevel = false; //some instructions are only permitted to be carried out
    //by the top level frame.

    std::chrono::steady_clock::time_point lastUserInput; //see noteUserInput()
protected:
    bool initYet; //becomes true once boilerplate html has been set.


    QAction* getEditQtAction(char action);
    //utility function to convert our hipe EDIT action codes (e.g. 'x'==cut, 'c'==copy)
    //into a pointer to the corresponding QAction object with methods to trigger that
    //action and check its toggle state.

    static void _receiveKeyEventOnBody(const QString& eventName, void* containerPtr, uint64_t isKeyUp,
                                        uint64_t requestor, const QString& eventDetails);

    static void _receiveDragStartEvent(const QString&, void*, uint64_t,
                                        uint64_t, const QString&) {};
protected slots:
    void frameDestroyed(); //conneected to the QWebFrame's destroyed() signal.



protected:
private:
    ///This block of variables/functions provides pointer protection of web
    ///element references so a handle (array element number) can safely be given
    ///to external processes, and invalid references can be detected.
    AutoExpArray<QWebElement*> referenceableElement;
    size_t firstFreeElementAfter=1;
    //store the location of the first free element, or a smaller element number, to speed insertions.
    size_t maxElementIndexUsed=0;
    //store the maximum location index used so far in this container to disallow
    //growing the index too quickly.
public:
    //when an element is created, we store its location as an element number here, then share the element number
    //(not a QWebElement--dangerous!) with the client.
    size_t assignElementIndex(const QWebElement& w, size_t newIndex);
    void removeReferenceableElement(size_t);
    QWebElement getReferenceableElement(size_t); //resolve a reference integer.
    size_t findReferenceableElement(const QWebElement&);
    size_t getIndexOfElement(const QWebElement&);
    //the location assigned to the element, or 0 if it has none (lookups never assign one).

    //flags to handle keyup/down events on body as a special case (since this event needs to propagate to the framing manager for special window manipulation keys)
    bool reportKeydownOnBody=false; //has the client requested keydown events on the body element?
    bool reportKeyupOnBody=false; //has the client requested keyup events on the body element?
    uint64_t keyDownOnBodyRequestor=0; //corresponding requestors if the client requests these key events.
    uint64_t keyUpOnBodyRequestor=0;
    
    std::string cursorSymbol; //the unicode character used for the mouse cursor. Only set if HIPE_OP_SET_CURSOR has been used.
    std::string cursorHotspot; //the cursor's hotspot argument (arg[1] of HIPE_OP_SET_CURSOR), if any.

    // Tracks a chunked HIPE_OP_SET_SRC upload in progress for a given location (see
    // handle_SET_SRC() in instructionhandler.cpp for the two-threshold sanity policy that reads
    // and writes this). Covers any element type SET_SRC chunking supports (img, audio, video) --
    // the map itself doesn't need to know which kind a given upload is; handle_SET_SRC re-derives
    // that from the element's tag name whenever it needs to. bytesSoFar mirrors what's already
    // been handed to WebCore -- kept here rather than queried back from it, so a chunk can be
    // rejected *before* growing the real buffer. banned means this location produced a broken
    // upload (sanity limit already breached) and every further chunk for it should be discarded
    // outright rather than accumulated; finish() or a matching FREE_LOCATION erase the entry,
    // banned or not.
    struct PendingBinaryUpload {
        size_t bytesSoFar = 0;
        bool banned = false;
    };
    std::map<hipe_loc, PendingBinaryUpload> pendingBinaryUploads;

    std::map<hipe_loc, QString> contentSnapshots;
    //for HIPE_OP_GET_CONTENT mode 4: the text last returned for each location, which the next read's
    //changes are reported against. Dropped when the location is freed.

public:
    KeyList* keyList;
    std::list<FrameData> subFrames;
    //table of subframes, mapping web element of an iframe to its corresponding
    //child frame object and host-key (if assigned).

    void cleanUpSubFrames();
    //clean up subframes that no longer exist in the document.

    void addNewSubFrame(QWebFrame* wf);
    //Called when the engine creates the frame of an <iframe> in this container's document (see
    //registerNewFrame()). Allows metadata to be associated with the iframe, such as its host key and
    //event requestors.
    static void registerNewFrame(QWebFrame* wf);
    //Connected to the page's frameCreated() signal: adds the new frame to the subframe table of the
    //container whose document holds its <iframe>. Every iframe is registered however it was inserted.
    FrameData* lookupSubFrame(const QWebElement& we);
    FrameData* lookupSubFrame(QWebFrame* wf);
};

#endif // CLIENTCONTAINER_H
