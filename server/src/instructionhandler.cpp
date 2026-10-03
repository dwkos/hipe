/*  Copyright (C) 2021 Harrison Schaefer, Thomas Templeton,
                       Amesh Fernando, Scott Guiney
    Copyright (C) 2021-2026 Daniel Kos

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

#include "instructionhandler.h"
#include <set>
#include <cstring>
#include "connection.h"
#include "keylist.h"
#include "sanitation.h"
#include "main.hpp"

#include <QPdfWriter>
#include <QPageSize>
#include <QMarginsF>
#include <QImage>
#include <QPainter>
#include <QBuffer>
#include <QSvgGenerator>
#include <QDesktopServices>
#include <QCoreApplication>

#include <HipeCore/QWebElement>
#include <unistd.h>
#include <iostream>
#include <iomanip> //for debugging

//A value that exceeds the highest value HIPE_OP_ constant for hipe instructions.
#define MAX_OP_CODE 120 //update if ever the max hipe instruction code value exceeds this.

//global variables used for mapping functions to function pointers...
struct {
    //the function pointer will have a different signature if it takes pre-converted
    //arguments.
    union {
        void (*noargs)(Container*, hipe_instruction*, bool, QWebElement);
        void (*withargs)(Container*, hipe_instruction*, bool, QWebElement, std::string[]);
    } ptrtype;

    short numargs; //the number of arguments to pre-convert to std::string

} handlerInfo[MAX_OP_CODE];
//The HIPE_OP number of the instruction is used to index the array.


void initInstructionMap() {
    for(int i=0; i<MAX_OP_CODE; i++) {
    //initialise all elements to nullptrs in case an unknown instruction comes through later.
        handlerInfo[i].ptrtype.noargs = nullptr;
        handlerInfo[i].numargs = 0;
    }

    //populate the handlerInfo array with hipe instruction handlers...

    handlerInfo[HIPE_OP_CLEAR].ptrtype.noargs = handle_CLEAR;

    handlerInfo[HIPE_OP_DELETE].ptrtype.noargs = handle_DELETE;

    handlerInfo[HIPE_OP_FREE_LOCATION].ptrtype.noargs = handle_FREE_LOCATION;

    handlerInfo[HIPE_OP_GET_FIRST_CHILD].ptrtype.noargs = handle_GET_FIRST_CHILD;

    handlerInfo[HIPE_OP_GET_LAST_CHILD].ptrtype.noargs = handle_GET_LAST_CHILD;

    handlerInfo[HIPE_OP_GET_NEXT_SIBLING].ptrtype.noargs = handle_GET_NEXT_SIBLING;

    handlerInfo[HIPE_OP_GET_PREV_SIBLING].ptrtype.noargs = handle_GET_PREV_SIBLING;

    handlerInfo[HIPE_OP_SET_FOCUS].ptrtype.noargs = handle_SET_FOCUS;

    handlerInfo[HIPE_OP_GET_GEOMETRY].ptrtype.noargs = handle_GET_GEOMETRY;

    handlerInfo[HIPE_OP_GET_SCROLL_GEOMETRY].ptrtype.noargs = handle_GET_SCROLL_GEOMETRY;

    handlerInfo[HIPE_OP_GET_FRAME_KEY].ptrtype.noargs = handle_GET_FRAME_KEY;

    handlerInfo[HIPE_OP_GET_X11_XID].ptrtype.noargs = handle_GET_X11_XID;

    handlerInfo[HIPE_OP_SET_ICON].ptrtype.noargs = handle_SET_ICON;

    handlerInfo[HIPE_OP_SET_SRC].ptrtype.noargs = handle_SET_SRC;

    handlerInfo[HIPE_OP_SET_STYLE_SRC].ptrtype.noargs = handle_SET_STYLE_SRC;

    handlerInfo[HIPE_OP_ADD_STYLE_RULE_SRC].ptrtype.noargs = handle_ADD_STYLE_RULE_SRC;

    handlerInfo[HIPE_OP_GET_CARAT_POSITION].ptrtype.noargs = handle_GET_CARAT_POSITION;

    handlerInfo[HIPE_OP_GET_AUDIOVIDEO_STATE].ptrtype.noargs = handle_GET_AUDIOVIDEO_STATE;
    
    handlerInfo[HIPE_OP_APPEND_TAG].ptrtype.withargs = handle_APPEND_TAG;
    handlerInfo[HIPE_OP_APPEND_TAG].numargs = 4;

    handlerInfo[HIPE_OP_INSERT_TAG].ptrtype.withargs = handle_INSERT_TAG;
    handlerInfo[HIPE_OP_INSERT_TAG].numargs = 4;

    handlerInfo[HIPE_OP_SET_TEXT].ptrtype.withargs = handle_SET_TEXT;
    handlerInfo[HIPE_OP_SET_TEXT].numargs = 2;

    handlerInfo[HIPE_OP_APPEND_TEXT].ptrtype.withargs = handle_APPEND_TEXT;
    handlerInfo[HIPE_OP_APPEND_TEXT].numargs = 2;

    handlerInfo[HIPE_OP_GET_BY_ID].ptrtype.withargs = handle_GET_BY_ID;
    handlerInfo[HIPE_OP_GET_BY_ID].numargs = 1;

    handlerInfo[HIPE_OP_ADD_STYLE_RULE].ptrtype.withargs = handle_ADD_STYLE_RULE;
    handlerInfo[HIPE_OP_ADD_STYLE_RULE].numargs = 2;

    handlerInfo[HIPE_OP_ADD_FONT].ptrtype.withargs = handle_ADD_FONT;
    handlerInfo[HIPE_OP_ADD_FONT].numargs = 2;

    handlerInfo[HIPE_OP_SET_TITLE].ptrtype.withargs = handle_SET_TITLE;
    handlerInfo[HIPE_OP_SET_TITLE].numargs = 1;

    handlerInfo[HIPE_OP_SET_ATTRIBUTE].ptrtype.withargs = handle_SET_ATTRIBUTE;
    handlerInfo[HIPE_OP_SET_ATTRIBUTE].numargs = 2;

    handlerInfo[HIPE_OP_SET_STYLE].ptrtype.withargs = handle_SET_STYLE;
    handlerInfo[HIPE_OP_SET_STYLE].numargs = 2;

    handlerInfo[HIPE_OP_EVENT_REQUEST].ptrtype.withargs = handle_EVENT_REQUEST;
    handlerInfo[HIPE_OP_EVENT_REQUEST].numargs = 2;

    handlerInfo[HIPE_OP_EVENT_CANCEL].ptrtype.withargs = handle_EVENT_CANCEL;
    handlerInfo[HIPE_OP_EVENT_CANCEL].numargs = 2;

    handlerInfo[HIPE_OP_SCROLL_BY].ptrtype.withargs = handle_SCROLL_BY;
    handlerInfo[HIPE_OP_SCROLL_BY].numargs = 3;

    handlerInfo[HIPE_OP_SCROLL_TO].ptrtype.withargs = handle_SCROLL_TO;
    handlerInfo[HIPE_OP_SCROLL_TO].numargs = 3;

    handlerInfo[HIPE_OP_GET_ATTRIBUTE].ptrtype.withargs = handle_GET_ATTRIBUTE;
    handlerInfo[HIPE_OP_GET_ATTRIBUTE].numargs = 1;

    handlerInfo[HIPE_OP_GET_STYLE].ptrtype.withargs = handle_GET_STYLE;
    handlerInfo[HIPE_OP_GET_STYLE].numargs = 1;

    handlerInfo[HIPE_OP_GET_SRC].ptrtype.withargs = handle_GET_SRC;
    handlerInfo[HIPE_OP_GET_SRC].numargs = 1;

    handlerInfo[HIPE_OP_CANVAS_QUERY].ptrtype.withargs = handle_CANVAS_QUERY;
    handlerInfo[HIPE_OP_CANVAS_QUERY].numargs = 2;

    handlerInfo[HIPE_OP_FRAME_CLOSE].ptrtype.withargs = handle_FRAME_CLOSE;
    handlerInfo[HIPE_OP_FRAME_CLOSE].numargs = 1;

    handlerInfo[HIPE_OP_TAKE_SNAPSHOT].ptrtype.withargs = handle_TAKE_SNAPSHOT;
    handlerInfo[HIPE_OP_TAKE_SNAPSHOT].numargs = 2;

    handlerInfo[HIPE_OP_USE_CANVAS].ptrtype.withargs = handle_USE_CANVAS;
    handlerInfo[HIPE_OP_USE_CANVAS].numargs = 1;

    handlerInfo[HIPE_OP_CANVAS_ACTION].ptrtype.withargs = handle_CANVAS_ACTION;
    handlerInfo[HIPE_OP_CANVAS_ACTION].numargs = 2;

    handlerInfo[HIPE_OP_CANVAS_SET_PROPERTY].ptrtype.withargs = handle_CANVAS_SET_PROPERTY;
    handlerInfo[HIPE_OP_CANVAS_SET_PROPERTY].numargs = 2;

    handlerInfo[HIPE_OP_REMOVE_ATTRIBUTE].ptrtype.withargs = handle_REMOVE_ATTRIBUTE;
    handlerInfo[HIPE_OP_REMOVE_ATTRIBUTE].numargs = 1;

    handlerInfo[HIPE_OP_GET_CONTENT].ptrtype.withargs = handle_GET_CONTENT;
    handlerInfo[HIPE_OP_GET_CONTENT].numargs = 2;

    handlerInfo[HIPE_OP_MEASURE_TEXT].ptrtype.withargs = handle_MEASURE_TEXT;
    handlerInfo[HIPE_OP_MEASURE_TEXT].numargs = 2;

    handlerInfo[HIPE_OP_GET_RANGE_GEOMETRY].ptrtype.withargs = handle_GET_RANGE_GEOMETRY;
    handlerInfo[HIPE_OP_GET_RANGE_GEOMETRY].numargs = 2;

    handlerInfo[HIPE_OP_CARAT_POSITION].ptrtype.withargs = handle_CARAT_POSITION;
    handlerInfo[HIPE_OP_CARAT_POSITION].numargs = 3;

    handlerInfo[HIPE_OP_FIND_TEXT].ptrtype.withargs = handle_FIND_TEXT;
    handlerInfo[HIPE_OP_FIND_TEXT].numargs = 2;

    handlerInfo[HIPE_OP_AUDIOVIDEO_STATE].ptrtype.withargs = handle_AUDIOVIDEO_STATE;
    handlerInfo[HIPE_OP_AUDIOVIDEO_STATE].numargs = 4;

    handlerInfo[HIPE_OP_DIALOG].ptrtype.withargs = handle_DIALOG;
    handlerInfo[HIPE_OP_DIALOG_INPUT].ptrtype.withargs = handle_DIALOG;
    handlerInfo[HIPE_OP_DIALOG].numargs = 4;
    handlerInfo[HIPE_OP_DIALOG_INPUT].numargs = 4;

    handlerInfo[HIPE_OP_DIALOG_RETURN].ptrtype.withargs = handle_DIALOG_RETURN;
    handlerInfo[HIPE_OP_DIALOG_RETURN].numargs = 4;    

    handlerInfo[HIPE_OP_GET_SELECTION].ptrtype.withargs = handle_GET_SELECTION;
    handlerInfo[HIPE_OP_GET_SELECTION].numargs = 1;

    handlerInfo[HIPE_OP_EDIT_ACTION].ptrtype.withargs = handle_EDIT_ACTION;
    handlerInfo[HIPE_OP_EDIT_ACTION].numargs = 2;

    handlerInfo[HIPE_OP_EDIT_STATUS].ptrtype.withargs = handle_EDIT_STATUS;
    handlerInfo[HIPE_OP_EDIT_STATUS].numargs = 1;

    handlerInfo[HIPE_OP_MESSAGE].ptrtype.withargs = handle_MESSAGE;
    handlerInfo[HIPE_OP_FIFO_ADD_ABILITY].ptrtype.withargs = handle_MESSAGE;
    handlerInfo[HIPE_OP_FIFO_REMOVE_ABILITY].ptrtype.withargs = handle_MESSAGE;
    handlerInfo[HIPE_OP_FIFO_OPEN].ptrtype.withargs = handle_MESSAGE;
    handlerInfo[HIPE_OP_FIFO_CLOSE].ptrtype.withargs = handle_MESSAGE;
    handlerInfo[HIPE_OP_FIFO_RESPONSE].ptrtype.withargs = handle_MESSAGE;
    handlerInfo[HIPE_OP_FIFO_DROP_PEER].ptrtype.withargs = handle_MESSAGE;
    handlerInfo[HIPE_OP_FIFO_GET_PEER].ptrtype.withargs = handle_MESSAGE;
    handlerInfo[HIPE_OP_OPEN_LINK].ptrtype.withargs = handle_MESSAGE;
    handlerInfo[HIPE_OP_EDIT_CONTEXT_REQUEST].ptrtype.withargs = handle_MESSAGE;
    handlerInfo[HIPE_OP_MESSAGE].numargs = 4;
    handlerInfo[HIPE_OP_FIFO_ADD_ABILITY].numargs = 4;
    handlerInfo[HIPE_OP_FIFO_REMOVE_ABILITY].numargs = 4;
    handlerInfo[HIPE_OP_FIFO_OPEN].numargs = 4;
    handlerInfo[HIPE_OP_FIFO_CLOSE].numargs = 4;
    handlerInfo[HIPE_OP_FIFO_RESPONSE].numargs = 4;
    handlerInfo[HIPE_OP_FIFO_DROP_PEER].numargs = 4;
    handlerInfo[HIPE_OP_FIFO_GET_PEER].numargs = 4;
    handlerInfo[HIPE_OP_OPEN_LINK].numargs = 4;
    handlerInfo[HIPE_OP_EDIT_CONTEXT_REQUEST].numargs = 4;

    handlerInfo[HIPE_OP_TOGGLE_CLASS].ptrtype.withargs = handle_TOGGLE_CLASS;
    handlerInfo[HIPE_OP_TOGGLE_CLASS].numargs = 1;

    handlerInfo[HIPE_OP_SET_CURSOR].ptrtype.withargs = handle_SET_CURSOR;
    handlerInfo[HIPE_OP_SET_CURSOR].numargs = 2;

}


void invoke_handler(Container* c, hipe_instruction* instruction, bool locationSpecified, QWebElement location) {
    short opcode = instruction->opcode;
    if(opcode >= MAX_OP_CODE || opcode <0) return; //out of range.
    if(!handlerInfo[opcode].ptrtype.noargs) return; //null function pointer.
    //(union means it doesn't matter what ptrtype we check for null condition)

    std::string args[HIPE_NARGS];
    short requestedArgs = handlerInfo[opcode].numargs;
    //pre-convert args if required
    for(short i=0; i<requestedArgs; i++) {
        args[i] = std::string(instruction->arg[i], instruction->arg_length[i]);
    }


    if(requestedArgs) {
        (handlerInfo[opcode].ptrtype.withargs)(c, instruction, locationSpecified, location, args);
    } else {
        (handlerInfo[opcode].ptrtype.noargs)(c, instruction, locationSpecified, location);
    }

}



void handle_CLEAR(Container* c, hipe_instruction*, bool locationSpecified, QWebElement location) {
    if(!locationSpecified) c->setBody("",true);
    else location.setInnerXml("");
}


void handle_DELETE(Container*, hipe_instruction*, bool locationSpecified, QWebElement location) {
    if(locationSpecified)
        location.removeFromDocument(); //Qt doc says this also makes location a 'null element'
}

void handle_FREE_LOCATION(Container* c, hipe_instruction* instruction, bool, QWebElement) {
    c->removeReferenceableElement(instruction->location);
    c->contentSnapshots.erase(instruction->location); //a reused location mustn't inherit an old GET_CONTENT mode 4 snapshot
    // Abandon any chunked upload still in progress toward this location -- it no longer means
    // anything to the client, so there's no point finishing it later.
    c->pendingBinaryUploads.erase(instruction->location);
}

void handle_GET_FIRST_CHILD(Container* c, hipe_instruction* instruction, bool, QWebElement location) {
    //answer the location request.
    c->client->sendInstruction(HIPE_OP_LOCATION_RETURN, instruction->requestor,
                                c->getIndexOfElement(location.firstChild()));
}

void handle_GET_LAST_CHILD(Container* c, hipe_instruction* instruction, bool, QWebElement location) {
    //answer the location request.
    c->client->sendInstruction(HIPE_OP_LOCATION_RETURN, instruction->requestor,
                                c->getIndexOfElement(location.lastChild()));
}

void handle_GET_NEXT_SIBLING(Container* c, hipe_instruction* instruction, bool, QWebElement location) {
    //answer the location request.
    c->client->sendInstruction(HIPE_OP_LOCATION_RETURN, instruction->requestor,
                                c->getIndexOfElement(location.nextSibling()));
}

void handle_GET_PREV_SIBLING(Container* c, hipe_instruction* instruction, bool, QWebElement location) {
    //answer the location request.
    c->client->sendInstruction(HIPE_OP_LOCATION_RETURN, instruction->requestor,
                                c->getIndexOfElement(location.previousSibling()));
}

void handle_SET_FOCUS(Container*, hipe_instruction*, bool, QWebElement location) {
    location.setFocus();
}

void handle_GET_GEOMETRY(Container* c, hipe_instruction* instruction, bool, QWebElement location) {
    std::string left = std::to_string(location.offsetLeft());
    std::string top = std::to_string(location.offsetTop());
    std::string width = std::to_string(location.offsetWidth());
    std::string height = std::to_string(location.offsetHeight());
    c->client->sendInstruction(HIPE_OP_GEOMETRY_RETURN, instruction->requestor, instruction->location,
                            {left, top, width, height});
}

void handle_GET_SCROLL_GEOMETRY(Container* c, hipe_instruction* instruction, bool, QWebElement location) {
    std::string left = std::to_string(location.scrollLeft());
    std::string top = std::to_string(location.scrollTop());
    std::string width = std::to_string(location.scrollWidth());
    std::string height = std::to_string(location.scrollHeight());
    c->client->sendInstruction(HIPE_OP_GEOMETRY_RETURN, instruction->requestor, instruction->location,
                                {left, top, width, height});
}

void handle_GET_FRAME_KEY(Container* c, hipe_instruction* instruction, bool, QWebElement location) {
    //Check if the location is already represented in the frame table.
    QString hostKey = "";
    FrameData* fd = c->lookupSubFrame(location);
    if(fd) {
        if(!fd->claimed) { //the iframe is vacant, so the request is valid.
            if(!fd->hostKey.size()) {  //if no hostkey yet, generate a new one.
                fd->hostKey = c->keyList->generateContainerKey().c_str();
            }
            fd->requestor = instruction->requestor; //use the requestor value for future clientframe events.
            hostKey = fd->hostKey; //store the host key to return to the client.
        }
    }

    //return the host key to the client if element was found/available, else return blank string.
    c->client->sendInstruction(HIPE_OP_KEY_RETURN, instruction->requestor, instruction->location, {hostKey.toStdString()});

}

void handle_GET_X11_XID(Container* c, hipe_instruction* instruction, bool, QWebElement location) {
    std::string xid = std::to_string(location.x11EmbedTargetXid());
    c->client->sendInstruction(HIPE_OP_X11_XID_RETURN, instruction->requestor, instruction->location, {xid});
}

void handle_SET_ICON(Container* c, hipe_instruction* instruction, bool, QWebElement) {
    c->setIcon(instruction->arg[0], instruction->arg_length[0]);
}


// Sanity limits for chunked (arg[2]=="1", "more chunks follow") HIPE_OP_SET_SRC uploads. Two
// thresholds per resource axis rather than one: a "modest" breach just fails that one upload
// quietly (the location is banned -- further chunks for it are discarded, connection stays alive),
// while an "absurd" breach means the client isn't behaving like any real producer and gets
// disconnected. The gap between the two is deliberate grace room for an honestly oversized (or
// numerous) legitimate transfer to fail quietly instead of losing the whole connection over it.
// Applied uniformly to images AND audio/video -- ordinary video routinely exceeds what was a
// reasonable image-only ceiling, and a generous shared ceiling costs nothing for the image case
// (a 500MB image is already unreasonable regardless). Peak memory per upload is bounded by
// MODEST_BYTES, not ABSURD_BYTES -- see project_hipe_sequential_media_loading_scope.md for why.
static const size_t SET_SRC_MODEST_BYTES = 500ULL * 1024 * 1024; // 500MB
static const size_t SET_SRC_ABSURD_BYTES = 2560ULL * 1024 * 1024; // 2.5GB
static const size_t SET_SRC_MODEST_CONCURRENT_UPLOADS = 8;
static const size_t SET_SRC_ABSURD_CONCURRENT_UPLOADS = 64;

// Which begin/append/finish trio a SET_SRC target uses -- re-derived from the element's tag name
// wherever it's needed rather than stored, per the map's own "doesn't need to know kind" design.
static bool isBinaryMediaTarget(QWebElement& location) {
    QString tag = location.tagName();
    return tag.compare("audio", Qt::CaseInsensitive) == 0 || tag.compare("video", Qt::CaseInsensitive) == 0;
}

void handle_SET_SRC(Container* c, hipe_instruction* instruction, bool, QWebElement location) {
    // arg[2] == "1" means more chunks are still to come for this location; absent/anything else
    // means this is the final (possibly only) chunk. Old clients that only ever send 2 args leave
    // arg_length[2] at 0 (see hipe_instruction_init()), so moreComing is always false for them --
    // this is fully backward compatible with every existing SET_SRC caller.
    bool moreComing = instruction->arg_length[2] == 1 && instruction->arg[2][0] == '1';

    auto pendingIt = c->pendingBinaryUploads.find(instruction->location);

    if (pendingIt != c->pendingBinaryUploads.end()) {
        // Continuing (or finishing) an upload already in progress for this location.
        if (pendingIt->second.banned) {
            // The client already broke this upload (or was refused a slot for it) and kept
            // sending regardless -- that's no longer an honest mistake worth tolerating quietly.
            std::cerr << "hiped: Client kept sending data to a banned SET_SRC location. Disconnected client.\n";
            if (c->client) c->client->disconnect();
            return;
        }

        size_t newTotal = pendingIt->second.bytesSoFar + instruction->arg_length[0];
        if (newTotal > SET_SRC_ABSURD_BYTES) {
            std::cerr << "hiped: Client's SET_SRC upload exceeded the absurd size ceiling. Disconnected client.\n";
            if (c->client) c->client->disconnect();
            return;
        }
        if (newTotal > SET_SRC_MODEST_BYTES) {
            pendingIt->second.banned = true;
            return; // discard this chunk and all future ones for this location; connection stays alive.
        }

        pendingIt->second.bytesSoFar = newTotal;
        if (isBinaryMediaTarget(location))
            location.appendBinaryMediaData(instruction->arg[0], instruction->arg_length[0]);
        else
            location.appendBinaryImageData(instruction->arg[0], instruction->arg_length[0]);
        if (!moreComing) {
            if (isBinaryMediaTarget(location))
                location.finishBinaryMediaData();
            else
                location.finishBinaryImageData();
            c->pendingBinaryUploads.erase(pendingIt);
        }
        return;
    }

    bool isMedia = isBinaryMediaTarget(location);
    std::string mimetype(instruction->arg[1], instruction->arg_length[1]);
    if(!mimetype.size() && !isMedia) mimetype="image/png"; // media has no sensible default -- it needs a codecs-qualified type from the client.

    if (!moreComing) {
        // Whole file in one call -- today's exact behaviour, unchanged, no map entry needed at all.
        location.setAttributeBinaryData("src", mimetype.c_str(), instruction->arg[0], instruction->arg_length[0]);
        return;
    }

    // Starting a new chunked upload -- check the concurrent-uploads axis before committing to one.
    size_t nonBannedCount = 0;
    for (auto& entry : c->pendingBinaryUploads) {
        if (!entry.second.banned) nonBannedCount++;
    }
    if (nonBannedCount >= SET_SRC_ABSURD_CONCURRENT_UPLOADS) {
        std::cerr << "hiped: Client opened an absurd number of concurrent SET_SRC uploads. Disconnected client.\n";
        if (c->client) c->client->disconnect();
        return;
    }
    if (nonBannedCount >= SET_SRC_MODEST_CONCURRENT_UPLOADS) {
        // Reject just this new upload -- whatever's already in flight for other locations is untouched.
        c->pendingBinaryUploads[instruction->location] = Container::PendingBinaryUpload{0, true};
        return;
    }

    if (isMedia) {
        // arg[3] is the client's optional size-hint for chunked audio/video (see
        // HIPE_OP_SET_SRC's doc comment) -- the real, final size of the complete file in bytes,
        // if known in advance. Malformed or absent input both fall back to -1 ("unknown"), the
        // same as a client that never sent it at all.
        qint64 expectedTotalSize = -1;
        if (instruction->arg_length[3]) {
            try {
                expectedTotalSize = std::stoll(std::string(instruction->arg[3], instruction->arg_length[3]));
            } catch(...) {} //malformed size hint -- fall back to unknown.
            if (expectedTotalSize < 0) expectedTotalSize = -1;
        }
        location.beginBinaryMediaData(mimetype.c_str(), expectedTotalSize);
        location.appendBinaryMediaData(instruction->arg[0], instruction->arg_length[0]);
    } else {
        location.beginBinaryImageData(mimetype.c_str());
        location.appendBinaryImageData(instruction->arg[0], instruction->arg_length[0]);
    }
    c->pendingBinaryUploads[instruction->location] = Container::PendingBinaryUpload{instruction->arg_length[0], false};
}

void handle_SET_STYLE_SRC(Container*, hipe_instruction* instruction, bool, QWebElement location) {
    std::string arg[4];
    arg[0] = std::string(instruction->arg[0], instruction->arg_length[0]);
    //don't convert arg[1] here since this may contain a lot of bytes.
    arg[2] = std::string(instruction->arg[2], instruction->arg_length[2]); //mime type
    if(!arg[2].size()) arg[2] = "image/png"; //default to png file format.
    arg[3] = std::string(instruction->arg[3], instruction->arg_length[3]); //supplementary value as suffix.

    std::string dataURI = std::string("data:") + arg[2] + ";base64," + Sanitation::toBase64(instruction->arg[1], instruction->arg_length[1]);
    location.setStyleProperty(arg[0].c_str(), QString("url(\"") + dataURI.c_str() + "\") " + arg[3].c_str());
}

void handle_ADD_STYLE_RULE_SRC(Container* c, hipe_instruction* instruction, bool, QWebElement) {
    std::string css_specifier(instruction->arg[0], instruction->arg_length[0]);
    std::string mimetype(instruction->arg[2], instruction->arg_length[2]);
    if(!mimetype.size()) mimetype="image/png";
    std::string dataURI = std::string("data:") + mimetype + ";base64," + Sanitation::toBase64(instruction->arg[1], instruction->arg_length[1]);

    if(Sanitation::isAllowedCSS(css_specifier))
        c->stylesheet += css_specifier + "{background-image:url(\"" + dataURI + "\");}\n";
    c->applyStylesheet();
}


void handle_GET_CARAT_POSITION(Container* c, hipe_instruction* instruction, bool, QWebElement location) {
    std::string selStart, selEnd; //left empty if the element contains neither the carat nor selected text.

    int anchor, focus;
    if(location.getSelectionRange(&anchor, &focus)) {
        selStart = std::to_string(anchor);
        selEnd = std::to_string(focus);
    }
    c->client->sendInstruction(HIPE_OP_CARAT_POSITION, instruction->requestor, 
            instruction->location, {selStart, selEnd});
}


void handle_GET_AUDIOVIDEO_STATE(Container* c, hipe_instruction* instruction, bool, QWebElement location) {
    WebCore::HTMLMediaElement* mediaElement = location.isMediaElement();
    if (mediaElement != nullptr)
    //if this is a nullptr, it means that the selected element is not a media element, and we cannot compute values for it
    {
        std::string position=location.getMediaPositionString(mediaElement);
        std::string speed = location.getPlaybackRate(mediaElement);
        bool playing = location.isMediaPlaying(mediaElement);
        std::string volume = location.getVolume(mediaElement);
        c->client->sendInstruction(HIPE_OP_AUDIOVIDEO_STATE, instruction->requestor,
        instruction->location, {position, speed, (playing?"1":"0"), volume});
    } else {
        //not an audio or video element: reply with empty arguments, so a client awaiting the reply isn't left waiting.
        c->client->sendInstruction(HIPE_OP_AUDIOVIDEO_STATE, instruction->requestor,
        instruction->location, {"", "", "", ""});
    }

}






//REQUIRES 3 ARGS
//Void elements (e.g. <br>, <img>) can't have content or a closing tag.
static bool isVoidTag(const std::string& tag) {
    static const std::set<std::string> voidTags = {"area", "base", "br", "col", "embed", "hr", "img",
                                                   "input", "link", "meta", "param", "source", "track", "wbr"};
    return voidTags.count(tag) > 0;
}

//Completes the markup of a tag for APPEND_TAG/INSERT_TAG after its id/src attributes: adds the optional
//class list (arg[2]) and initial text content (arg[3], handled as text mode 0), both escaped, and closes the tag. Setting them here
//saves separate TOGGLE_CLASS and SET_TEXT instructions (and a second HTML parse) per element.
static std::string finishTagMarkup(const std::string& tag, const std::string& classes, const std::string& text) {
    std::string markup;
    if(classes.size())
        markup += " class=\"" + Sanitation::sanitisePlainText(classes) + "\"";
    markup += ">";
    if(isVoidTag(tag)) return markup;
    markup += Sanitation::sanitisePlainText(text, Sanitation::SHOWN_AS_TYPED);
    markup += "</" + tag + ">";
    return markup;
}

//The client allocates the location of each tag it appends or inserts (it arrives as the requestor), and
//counts on from it for the next one, whether or not the tag is created. So the location is assigned even when no
//tag was created, to no element: instructions sent to it do nothing. Otherwise the client's next tag would ask for
//a location two past the last one assigned. The client is disconnected if the location is in use or skips ahead.
static void assignTagLocation(Container* c, hipe_instruction* instruction, const QWebElement& element) {
    if(c->assignElementIndex(element, instruction->requestor) == 0) {
        if(c->client) c->client->disconnect(); //Hard disconnection. Will be cleaned up in the next service cycle.
        std::cerr << "hiped: Client tried to assign invalid location value. Disconnected client.\n";
    }
}

void handle_APPEND_TAG(Container* c, hipe_instruction* instruction, bool locationSpecified, QWebElement location, std::string arg[]) {
    arg[1] = Sanitation::sanitisePlainText(arg[1]);
    if(!Sanitation::isValidTagName(arg[0])) {
        assignTagLocation(c, instruction, QWebElement());
        return;
    }

    std::string newTagString = "<";
    newTagString += arg[0];

    //canvases don't function correctly without an ID. Generate one if not provided.
    if(!arg[1].size() && arg[0]=="canvas") {
        arg[1] = c->keyList->generateContainerKey();
        c->keyList->claimKey(arg[1]); //burn through a container key to get a random string out of it.
    }
    if(arg[1].size()) { //apply an ID to the new tag if provided.
        newTagString += " id=\"" + arg[1] + "\"";
    }

    newTagString += finishTagMarkup(arg[0], arg[2], arg[3]);
    QWebElement before = (locationSpecified ? location : c->webElement).lastChild();
    if(!locationSpecified) {
        c->setBody(newTagString, false /*append mode*/);
        location = c->webElement; //webElement may have been redefined in setBody().
    }
    else location.appendInside(newTagString.c_str());

    //If the HTML parser dropped the tag (e.g. a <td> outside a table), no new element was added.
    QWebElement added = location.lastChild();
    assignTagLocation(c, instruction, added != before ? added : QWebElement());
}


//REQUIRES 3 ARGS
void handle_INSERT_TAG(Container* c, hipe_instruction* instruction, bool locationSpecified, QWebElement location, std::string arg[]) {
    arg[1] = Sanitation::sanitisePlainText(arg[1]);
    if(!Sanitation::isValidTagName(arg[0])
            || !locationSpecified) { //can't prepend a tag outside the body element!
        assignTagLocation(c, instruction, QWebElement());
        return;
    }

    std::string newTagString = "<";
    newTagString += arg[0];
    //canvases don't function correctly without an ID. Generate one if not provided.
    if(!arg[1].size() && arg[0]=="canvas") {
        arg[1] = c->keyList->generateContainerKey();
        c->keyList->claimKey(arg[1]); //burn through a container key to get a random string out of it.
    }
    if(arg[1].size()) { //apply an ID to the new tag if provided.
        newTagString += " id=\"" + arg[1] + "\"";
    }
    newTagString += finishTagMarkup(arg[0], arg[2], arg[3]);

    QWebElement before = location.previousSibling();
    location.prependOutside(newTagString.c_str());

    //If the HTML parser dropped the tag (e.g. a <td> outside a table), no new element was added.
    QWebElement added = location.previousSibling();
    assignTagLocation(c, instruction, added != before ? added : QWebElement());
}


//REQUIRES 2 ARGS
void handle_SET_TEXT(Container* c, hipe_instruction*, bool locationSpecified, QWebElement location, std::string arg[]) {
    arg[0] = Sanitation::sanitisePlainText(arg[0], Sanitation::textModeFromArg(arg[1]));
    if(!locationSpecified) c->setBody(arg[0]);
    else location.setInnerXml(arg[0].c_str());
}


//REQUIRES 2 ARGS
void handle_APPEND_TEXT(Container* c, hipe_instruction*, bool locationSpecified, QWebElement location, std::string arg[]) {
    arg[0] = Sanitation::sanitisePlainText(arg[0], Sanitation::textModeFromArg(arg[1]));
    if(!locationSpecified) c->setBody(arg[0], false);
    else location.appendInside(arg[0].c_str());
}


//REQUIRES 1 ARG
void handle_GET_BY_ID(Container* c, hipe_instruction* instruction, bool, QWebElement, std::string arg[]) {
    c->client->sendInstruction(HIPE_OP_LOCATION_RETURN, instruction->requestor,
                c->getIndexOfElement(c->webElement.findFirst(QString("#") + arg[0].c_str())));
}


//REQUIRES 2 ARGS
void handle_ADD_STYLE_RULE(Container* c, hipe_instruction*, bool, QWebElement, std::string arg[]) {
    if(Sanitation::isAllowedCSS(arg[0]) && Sanitation::isAllowedCSS(arg[1]))
        c->stylesheet += arg[0] + "{" + arg[1] + "}\n";
    c->applyStylesheet();
}


//REQUIRES 2 ARGS -- third arg is extracted from the instruction directly due to
//the larger size of its contents.
void handle_ADD_FONT(Container* c, hipe_instruction* instruction, bool, QWebElement, std::string arg[]) {
//arg[0] is font family name, arg[1] is mime type, arg[2] is raw data.
    if(Sanitation::isAllowedCSS(arg[0]) && Sanitation::isAllowedCSS(arg[1]))
        c->stylesheet += "@font-face {font-family:\"" + arg[0] + "\"; src:url(\"data:"
                + arg[1] + ";base64,"
                + Sanitation::toBase64(instruction->arg[2],instruction->arg_length[2])
                + "\");}\n";
    c->applyStylesheet();
}


//REQUIRES 1 ARG
void handle_SET_TITLE(Container* c, hipe_instruction*, bool, QWebElement, std::string arg[]) {
    c->setTitle(arg[0]);
}


//REQUIRES 2 ARGS
void handle_SET_ATTRIBUTE(Container*, hipe_instruction*, bool, QWebElement location, std::string arg[]) {
    location.setAttribute(QString(arg[0].c_str()), QString(arg[1].c_str()));
}


//REQUIRES 2 ARGS
void handle_SET_STYLE(Container* c, hipe_instruction*, bool locationSpecified, QWebElement location, std::string arg[]) {
    if(!locationSpecified) {  //styling the body element (location 0)
        //we need to be sure the body has been initialised first.
        if(c->webElement.isNull())
            c->setBody("");
        c->webElement.setStyleProperty(arg[0].c_str(), arg[1].c_str());

        //check if foreground/background colors for this frame have changed.
        QString fg, bg;
        while(!bg.size()) 
        //poll repeatedly until we get a non-null response, if required (frame might not have rendered yet).
            bg = c->webElement.styleProperty("background-color", QWebElement::ComputedStyle);
        while(!fg.size())
            fg = c->webElement.styleProperty("color", QWebElement::ComputedStyle);

        //check if foreground or background colours are defined by this client. If so, notify the parent, and the
        //parent will update its own metadata for this frame, to determine whether to send the relevant event.
        if(c->bgColorChanged(bg.toStdString())) {
            if(c->getParent())  //notify parent frame of new background color
                c->getParent()->receiveSubFrameEvent(HIPE_FRAME_EVENT_BACKGROUND_CHANGED, 
                        c->webElement.webFrame(), bg.toStdString());
        }
        if(c->fgColorChanged(fg.toStdString())) {
            if(c->getParent())  //notify parent frame of new foreground color
                c->getParent()->receiveSubFrameEvent(HIPE_FRAME_EVENT_COLOR_CHANGED, 
                        c->webElement.webFrame(), fg.toStdString());
        }
    } else {  //styling another element
        location.setStyleProperty(arg[0].c_str(), arg[1].c_str());
    }
}


//REQUIRES 1 ARG
void handle_EVENT_REQUEST(Container* c, hipe_instruction* instruction, bool locationSpecified, QWebElement location, std::string arg[]) {
    arg[0] = Sanitation::toLower(arg[0].c_str(), arg[0].size()); //sanitise against user overriding event attributes with uppercase equivalents.

    //special case: iframes need the requestor value to be stored separately for keydown/keyup events.
    if(locationSpecified && (arg[0] == "keydown" || arg[0] == "keyup")) {
        //check if the location is an iframe. If so, store the requestor values in the frame data.
        FrameData* fd = c->lookupSubFrame(location);
        if(fd) { //if this is a subframe, store the requestor values in the frame data.
            if(arg[0] == "keydown") fd->keyDownRequestor = instruction->requestor;
            else if(arg[0] == "keyup") fd->keyUpRequestor = instruction->requestor;
        }
    }

    //arg[1]: the default actions to cancel ("code,modifiers" rules separated by ';', "-" for none), replacing any
    //set by an earlier request. A contextmenu's is cancelled unless the client says otherwise: requesting the event
    //has always replaced the framing manager's edit menu.
    std::string rules = arg[1];
    if(rules.empty() && arg[0] == "contextmenu") rules = "*";
    if(rules == "-") rules = "";
    location.setDefaultPrevention(arg[0].c_str(), rules.c_str()); //location is the body for location 0.

    if(arg[0] == "keydown" && !locationSpecified) { //keydown on body element is a special case.
        c->reportKeydownOnBody=true;
        c->keyDownOnBodyRequestor=instruction->requestor;
    } else if(arg[0] == "keyup" && !locationSpecified) { //keyup on body element is a special case.
        c->reportKeyupOnBody=true;
        c->keyUpOnBodyRequestor=instruction->requestor;
    } else {
        location.requestEvent(arg[0].c_str(), c->client, instruction->location, instruction->requestor,
                                Connection::_receiveUIEvent, false);
    }
}


//REQUIRES 2 ARGS
void handle_EVENT_CANCEL(Container* c, hipe_instruction* instruction, bool, QWebElement location, std::string arg[]) {
    if(arg[0] == "keydown" && !instruction->location) { //keydown on body element is a special case.
        c->reportKeydownOnBody=false;
        c->keyDownOnBodyRequestor=0;
        location.setDefaultPrevention("keydown", ""); //(cancelEvent() below would remove hiped's own listener too.)
    } else if(arg[0] == "keyup" && !instruction->location) { //keyup on body element is a special case.
        c->reportKeyupOnBody=false;
        c->keyUpOnBodyRequestor=0;
        location.setDefaultPrevention("keyup", "");
    } else {
        location.cancelEvent(arg[0].c_str());
    }
    if(arg[1] == "1") { //reply requested. Send back an EVENT_CANCEL instruction to tell the client it can clean up event listeners for this event now.
        c->client->sendInstruction(HIPE_OP_EVENT_CANCEL, instruction->requestor,
                                    instruction->location, {arg[0], arg[1]});
    }
}


//REQUIRES 3 ARGS
void handle_SCROLL_BY(Container*, hipe_instruction*, bool, QWebElement location, std::string arg[]) {
    //if arg[2] is "%", then the units are percentage of scroll track. Otherwise
    //units are pixels of positive offet at the top-left of the viewable area.
    bool percentage = false;
    if(arg[2] == "%")
        percentage = true;

    if(arg[0].size()) { //left offset which may have decimal places if zoomed in.
        try {
            float leftVal = std::stof(arg[0]); //may throw exception if invalid.
            if (!percentage)
                location.setScrollLeft(location.scrollLeft() + (int) leftVal);
            else
            location.setScrollLeft(location.scrollLeft() + (int) (leftVal * (location.scrollWidth() - location.clientWidth()) / 100));
        } catch(...) {} //just do nothing on error.
    }
    if(arg[1].size()) { //top offset
        try {
            float topVal = std::stof(arg[1]); //may throw exception if invalid.
            if(!percentage)
                location.setScrollTop(location.scrollTop() + (int) topVal);
            else
                location.setScrollTop(location.scrollTop() + (int) (topVal * (location.scrollHeight() - location.clientHeight()) / 100));
        } catch(...) {} //just do nothing on error.
    }
}


//REQUIRES 3 ARGS
void handle_SCROLL_TO(Container*, hipe_instruction*, bool, QWebElement location, std::string arg[]) {
    //if arg[2] is "%", then the units are percentage of scroll track. Otherwise
    //units are pixels of positive offet at the top-left of the viewable area.
    bool percentage = false;
    if(arg[2] == "%")
        percentage = true;

    if(arg[0].size()) { //left offset which may have decimal places if zoomed in.
        try {
            float leftVal = std::stof(arg[0]); //may throw exception if invalid.
            if(!percentage)
                location.setScrollLeft((int) leftVal);
            else
                location.setScrollLeft((int) (leftVal * (location.scrollWidth() - location.clientWidth()) / 100));

        } catch(...) {} //just do nothing on error.
    }
    if(arg[1].size()) { //top offset
        try {
            float topVal = std::stof(arg[1]); //may throw exception if invalid.
            if(!percentage)
                location.setScrollTop((int) topVal);
            else
                location.setScrollTop((int) (topVal * (location.scrollHeight() - location.clientHeight()) / 100));
        } catch(...) {} //just do nothing on error.
    }
}


//REQUIRES 1 ARG
void handle_GET_ATTRIBUTE(Container* c, hipe_instruction* instruction, bool, QWebElement location, std::string arg[]) {
    QString attrVal = location.attribute(arg[0].c_str());
    /*if (attrVal == "" || attrVal == NULL){
        attrVal = "None";  //needed? why?
    }*/
    c->client->sendInstruction(HIPE_OP_ATTRIBUTE_RETURN, instruction->requestor,
                            instruction->location,
                            {arg[0], attrVal.toStdString()});
}


//REQUIRES 1 ARG
void handle_GET_STYLE(Container* c, hipe_instruction* instruction, bool, QWebElement location, std::string arg[]) {
    //ComputedStyle resolves cascaded/inherited rules too (e.g. a loaded theme stylesheet's "body"
    //rule), unlike handle_GET_ATTRIBUTE's attribute() call above - see this opcode's own comment in
    //hipe_instruction.h for why that distinction is the whole point of this instruction existing.
    QString resolved = location.styleProperty(arg[0].c_str(), QWebElement::ComputedStyle);
    c->client->sendInstruction(HIPE_OP_STYLE_RETURN, instruction->requestor,
                            instruction->location,
                            {arg[0], resolved.toStdString()});
}


//REQUIRES 1 ARG
void handle_GET_SRC(Container* c, hipe_instruction* instruction, bool, QWebElement location, std::string arg[]) {
    QByteArray data;
    QString mimeType, error;
    bool ok;

    ok = location.getSrcData(QString(arg[0].c_str()), data, mimeType, error);

    // Keep these alive until sendInstruction() below returns -- payload.arg[] below
    // points directly at their buffers, same zero-copy shape as HIPE_OP_FILE_RETURN's
    // own construction in handle_TAKE_SNAPSHOT.
    std::string mimeTypeStd = mimeType.toStdString();
    std::string errorStd = error.toStdString();

    hipe_instruction payload;
    hipe_instruction_init(&payload);
    payload.opcode = HIPE_OP_SRC_RETURN;
    payload.requestor = instruction->requestor;
    payload.location = instruction->location;
    if (ok && data.size() > 0) {
        payload.arg[0] = data.data();
        payload.arg_length[0] = data.size();
        payload.arg[1] = const_cast<char*>(mimeTypeStd.c_str());
        payload.arg_length[1] = mimeTypeStd.size();
        payload.arg[2] = 0; payload.arg_length[2] = 0;
    } else {
        payload.arg[0] = 0; payload.arg_length[0] = 0;
        payload.arg[1] = 0; payload.arg_length[1] = 0;
        payload.arg[2] = const_cast<char*>(errorStd.c_str());
        payload.arg_length[2] = errorStd.size();
    }
    c->client->sendInstruction(payload);
}


//REQUIRES 2 ARGS
void handle_CANVAS_QUERY(Container* c, hipe_instruction* instruction, bool, QWebElement, std::string arg[]) {
    QString result, error;
    bool ok;

    ok = c->currentCanvas.canvasQuery(QString(arg[0].c_str()), QString(arg[1].c_str()), result, error);

    c->client->sendInstruction(HIPE_OP_CANVAS_QUERY_RETURN, instruction->requestor, instruction->location,
        {ok ? result.toStdString() : std::string(), ok ? std::string() : error.toStdString()});
}


//REQUIRES 1 ARG
void handle_FRAME_CLOSE(Container* c, hipe_instruction*, bool, QWebElement location, std::string arg[]) {
    //find the relevant client
    FrameData* fd = c->lookupSubFrame(location);
    if(fd) {
        Connection* target = identifyFromFrame(fd->wf); //find the corresponding container.
        if(target) {
            if(!arg[0].size() || arg[0][0] == '\0')
                target->disconnect(); //Hard disconnection. Will be cleaned up in the next service cycle.
            else
                target->container->containerClosed(); //soft close request.
        }
    } 
}


//REQUIRES 2 ARGS
void handle_TAKE_SNAPSHOT(Container* c, hipe_instruction* instruction, bool, QWebElement, std::string arg[]) {
    std::string fmt = Sanitation::toLower(arg[0].c_str(), arg[0].size());
    if(fmt != "pdf" && fmt != "png" && fmt != "svg") return; //only these formats are supported.

    QWebFrame* frame = c->webElement.webFrame();
    QSize contents = frame->contentsSize();

    //Render the whole frame in screen media, at 1:1 scale, with no pagination and no
    //print stylesheet - i.e. a true snapshot of what's on screen, not a print preview.
    //renderContentsForSnapshot() (hipecore addition) wraps
    //WebCore::FrameView::paintContentsForSnapshot(). The output is built straight into
    //an in-memory buffer: QPdfWriter, QSvgGenerator and QImage::save() all take a
    //QIODevice, so no temporary file is needed (the old QPrinter path required
    //setOutputFileName()).
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    bool rendered = false;

    //guard against degenerate / absurd content sizes (the PNG buffer is w*h*4 bytes).
    bool sizeOk = contents.width() > 0 && contents.height() > 0
               && (qint64)contents.width() * contents.height() <= 64LL * 1024 * 1024;

    if(sizeOk && fmt == "png") {
        QImage img(contents, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        frame->renderContentsForSnapshot(&p);
        p.end();
        rendered = img.save(&buffer, "PNG");
    } else if(sizeOk && fmt == "pdf") {
        QPdfWriter pdf(&buffer);
        pdf.setResolution(96); //1 painter unit == 1 CSS pixel
        pdf.setPageSize(QPageSize(QSizeF(contents.width() / 96.0, contents.height() / 96.0),
                                 QPageSize::Inch, QString(), QPageSize::ExactMatch));
        pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
        QPainter p(&pdf);
        frame->renderContentsForSnapshot(&p);
        p.end(); //flushes the page; the PDF trailer is written when 'pdf' is destroyed below.
        rendered = true;
    } else if(sizeOk && fmt == "svg") {
        //Note: text comes out as outlined vector paths, not <text> elements referencing
        //a font by name, so (unlike a typical SVG export) there's no font-availability
        //dependency in the viewer. Known gap: a CSS gradient background combined with a
        //plain (non-rounded) border loses its fill entirely in the SVG output - a
        //QSvgGenerator paint-engine limitation in how it translates the clipped fill
        //WebKit uses whenever a border is present. Gradients alone, or gradients with
        //border-radius (no border), render correctly. Use "pdf"/"png" when a bordered
        //gradient needs to look right.
        QSvgGenerator svg;
        svg.setOutputDevice(&buffer);
        svg.setSize(contents);
        svg.setViewBox(QRect(QPoint(0, 0), contents));
        svg.setResolution(96); //1 painter unit == 1 CSS pixel, matches the PDF path
        QPainter p(&svg);
        frame->renderContentsForSnapshot(&p);
        p.end();
        rendered = true;
    }
    buffer.close();

    //Send the rendered bytes (or an error) back to the client. 'out' outlives the
    //synchronous sendInstruction() call, which is all its buffer needs to survive.
    hipe_instruction payload;
    hipe_instruction_init(&payload);
    payload.opcode = HIPE_OP_FILE_RETURN;
    payload.requestor = instruction->requestor;
    payload.location = instruction->location;
    if(rendered && out.size() > 0) {
        payload.arg[0] = out.data();
        payload.arg_length[0] = out.size();
        payload.arg[1] = 0; payload.arg_length[1] = 0;
    } else {
        payload.arg[0] = 0; payload.arg_length[0] = 0;
        payload.arg[1] = (char*) "Snapshot error.";
        payload.arg_length[1] = 15;
    }

    c->client->sendInstruction(payload);
}


//REQUIRES 1 ARG
void handle_USE_CANVAS(Container* c, hipe_instruction*, bool, QWebElement location, std::string arg[]) {
    // QWebElement::useCanvasContext() validates the context type itself and no-ops on
    // anything unsupported.
    c->currentCanvas = location;
    c->currentCanvas.useCanvasContext(QString(arg[0].c_str()));
}


//REQUIRES 2 ARGS
void handle_CANVAS_ACTION(Container* c, hipe_instruction*, bool, QWebElement, std::string arg[]) {
    // Applies to whichever <canvas> the most recent HIPE_OP_USE_CANVAS selected --
    // this instruction carries no location of its own (matches the existing client
    // calling convention, e.g. hipe/api/test/canvas.c never passes one here).
    c->currentCanvas.canvasAction(QString(arg[0].c_str()), QString(arg[1].c_str()));
}


//REQUIRES 2 ARGS
void handle_CANVAS_SET_PROPERTY(Container* c, hipe_instruction*, bool, QWebElement, std::string arg[]) {
    c->currentCanvas.canvasSetProperty(QString(arg[0].c_str()), QString(arg[1].c_str()));
}


//REQUIRES 1 ARG
void handle_REMOVE_ATTRIBUTE(Container*, hipe_instruction*, bool, QWebElement location, std::string arg[]) {
    location.removeAttribute(arg[0].c_str());
}


//REQUIRES 1 ARG
//Number of characters (Unicode code points) in n UTF-16 units: n minus the surrogate pairs.
static int codePointCount(const QChar* data, int n) {
    const ushort* u = reinterpret_cast<const ushort*>(data);
    int pairs = 0;
    for(int i=0; i+1<n; i++)
        if((u[i] & 0xFC00) == 0xD800 && (u[i+1] & 0xFC00) == 0xDC00) { pairs++; i++; }
    return n - pairs;
}

//Length of the common start of a and b (up to n UTF-16 units), comparing in blocks with memcmp.
static int commonPrefix(const QChar* a, const QChar* b, int n) {
    const int block = 2048;
    int i = 0;
    while(i + block <= n && !memcmp(a + i, b + i, block * sizeof(QChar))) i += block;
    while(i < n && a[i] == b[i]) i++;
    return i;
}

//Length of the common end of a (length la) and b (length lb), at most n units, comparing in blocks.
static int commonSuffix(const QChar* a, int la, const QChar* b, int lb, int n) {
    const int block = 2048;
    int i = 0;
    while(i + block <= n && !memcmp(a + la - i - block, b + lb - i - block, block * sizeof(QChar))) i += block;
    while(i < n && a[la-1-i] == b[lb-1-i]) i++;
    return i;
}

//GET_CONTENT mode 4: reports how the element's text (as mode 0 returns it) has changed since the previous
//mode-4 read by this client: {new text of the changed span, start, old length, "delta"}, in characters.
//The first read, or one with arg[1] == "full", returns the whole text with "full".
static void sendContentChanges(Container* c, hipe_instruction* instruction, const QString& current, bool full) {
    auto snapshot = c->contentSnapshots.find(instruction->location);
    if(full || snapshot == c->contentSnapshots.end()) {
        c->contentSnapshots[instruction->location] = current;
        c->client->sendInstruction(HIPE_OP_CONTENT_RETURN, instruction->requestor, instruction->location,
                                   {current.toStdString(), "0", "0", "full"});
        return;
    }
    const QString& previous = snapshot->second;
    const QChar* a = previous.constData();
    const QChar* b = current.constData();
    int la = previous.size(), lb = current.size();
    //smallest changed span: longest common start, then longest common end not overlapping it,
    //never splitting a surrogate pair.
    int start = commonPrefix(a, b, std::min(la, lb));
    if(start > 0 && a[start-1].isHighSurrogate()) start--;
    int end = commonSuffix(a, la, b, lb, std::min(la - start, lb - start));
    if(end > 0 && end < lb - start && b[lb-end].isLowSurrogate()) end--;
    int oldSpan = la - start - end, newSpan = lb - start - end;
    std::string startChars = std::to_string(codePointCount(b, start));
    std::string oldChars = std::to_string(codePointCount(a + start, oldSpan));
    std::string newText = current.mid(start, newSpan).toStdString();
    snapshot->second = current;
    c->client->sendInstruction(HIPE_OP_CONTENT_RETURN, instruction->requestor, instruction->location,
                               {newText, startChars, oldChars, "delta"});
}

//REQUIRES 2 ARGS
void handle_GET_CONTENT(Container* c, hipe_instruction* instruction, bool, QWebElement location, std::string arg[]) {
    //get inner content of (extract data from) location.
    std::string contentStr;

    if(arg[0] == "4") { //the changes since the last mode-4 read (see sendContentChanges())
        QString current = location.toTextContent();
        sendContentChanges(c, instruction, current, arg[1] == "full");
        return;
    }

    if(arg[0] == "0" || arg[0] == "") { //default: unformatted/plain text (textContent: independent of rendering)
        contentStr = location.toTextContent().toStdString();
    } else if(arg[0] == "1") { //html-formatted content requested from element.
        contentStr = location.toInnerXml().toStdString();
    } else if(arg[0] == "2") { //text as laid out (line breaks preserved), including clipped/hidden text
        contentStr = location.toLayoutText().toStdString();
    } else if(arg[0] == "3") { //some form elements require data to be read via a value attribute
        contentStr = location.attribute("value").toStdString();
    }
    c->client->sendInstruction(HIPE_OP_CONTENT_RETURN, instruction->requestor,
                                       instruction->location, {contentStr});
}


//Parses a whole string as an int; false if it isn't one (or is out of range).
static bool parseWholeInt(const std::string& str, int& value) {
    try {
        size_t used;
        value = std::stoi(str, &used);
        return used == str.size();
    } catch(...) {
        return false;
    }
}

//Formats a CSS pixel value compactly: at most 2 decimals, no trailing zeros.
static std::string pixelString(double value) {
    char buf[32];
    snprintf(buf, sizeof buf, "%.2f", value);
    std::string s = buf;
    s.erase(s.find_last_not_of('0') + 1);
    if(s.back() == '.') s.pop_back();
    if(s == "-0") s = "0";
    return s;
}

//REQUIRES 2 ARGS
void handle_MEASURE_TEXT(Container* c, hipe_instruction* instruction, bool, QWebElement location, std::string arg[]) {
    std::string width, height, ascent, descent; //left empty if the element can't be measured.
    double fontSize = 0;
    if(arg[1].size()) {
        try { fontSize = std::stod(arg[1]); } catch(...) { fontSize = 0; }
    }
    QVector<qreal> metrics = location.measureText(QString::fromStdString(arg[0]), fontSize);
    if(metrics.size() == 4) {
        width = pixelString(metrics[0]);
        height = pixelString(metrics[1]);
        ascent = pixelString(metrics[2]);
        descent = pixelString(metrics[3]);
    }
    c->client->sendInstruction(HIPE_OP_TEXT_METRICS, instruction->requestor, instruction->location,
                            {width, height, ascent, descent});
}

//REQUIRES 2 ARGS
void handle_GET_RANGE_GEOMETRY(Container* c, hipe_instruction* instruction, bool, QWebElement location, std::string arg[]) {
    std::string rects; //left empty if there is nothing to report.
    int start, end;
    bool valid = true;
    if(!arg[0].size() && !arg[1].size()) { //the element's current caret (the selection's focus)
        int anchor;
        valid = location.getSelectionRange(&anchor, &start);
        end = start;
    } else {
        valid = parseWholeInt(arg[0], start);
        if(!arg[1].size()) end = start;
        else valid = valid && parseWholeInt(arg[1], end);
    }
    if(valid) {
        for(const QRectF& r : location.rangeGeometry(start, end)) {
            if(rects.size()) rects += ";";
            rects += pixelString(r.x()) + "," + pixelString(r.y()) + "," + pixelString(r.width()) + "," + pixelString(r.height());
        }
    }
    c->client->sendInstruction(HIPE_OP_RANGE_GEOMETRY, instruction->requestor, instruction->location, {rects});
}

//REQUIRES 3 ARGS (arg[2] optional: "1" scrolls the selection into view)
void handle_CARAT_POSITION(Container*, hipe_instruction*, bool, QWebElement location, std::string arg[]) {
    //set the selection anchor/focus position...
    //(note, this instruction may also be sent from Hipe to indicate a current carat position)
    //arg[0] is the selection anchor; arg[1] the selection focus, if specified.
    if(!arg[0].size()) return; //empty: GET_CARAT_POSITION's "not in this element" reply. Leave it alone.
    if(!arg[1].size()) arg[1] = arg[0]; //if unspecified, focus=anchor means cursor without selection.
    //Offsets are characters (code points); negative values count from the end, -1 being after the last
    //character. hipecore resolves and clamps them.
    int anchor, focus;
    try {
        size_t anchorEnd, focusEnd;
        anchor = std::stoi(arg[0], &anchorEnd);
        focus = std::stoi(arg[1], &focusEnd);
        if(anchorEnd != arg[0].size() || focusEnd != arg[1].size()) return; //trailing junk, e.g. "3x".
    } catch(...) {
        return; //not a number, or out of range: ignore the instruction.
    }
    location.setSelectionRange(anchor, focus, arg[2] == "1");
}


//REQUIRES 1 ARG
void handle_FIND_TEXT(Container* c, hipe_instruction* instruction, bool locationSpecified, QWebElement location, std::string arg[]) {
    //Find within the location given (an element of the client's own document, or an <iframe> it owns,
    //including everything nested in it) or the client's whole document. A client can't name anything
    //outside its own document and child frames, so it can never search a sibling's or parent's content.
    std::string count = "0", index = "0", wrapped = "0", capped;
    if(!locationSpecified) location = c->webElement;
    const std::string& options = arg[1];
    QVector<int> result = location.findText(QString::fromStdString(arg[0]),
            options.find('b') != std::string::npos, options.find('c') != std::string::npos,
            options.find('w') != std::string::npos, 1000);
    count = std::to_string(result[0]);
    index = std::to_string(result[1]);
    wrapped = result[2] ? "1" : "0";
    if(result[3]) capped = "+";
    c->client->sendInstruction(HIPE_OP_FIND_RESULT, instruction->requestor, instruction->location,
                               {count, index, wrapped, capped});
}


//REQUIRES 4 ARGS
void handle_AUDIOVIDEO_STATE(Container*, hipe_instruction*, bool, QWebElement location, std::string arg[]) {
    double argValue;
    WebCore::HTMLMediaElement* mediaElement = location.isMediaElement();
    if (mediaElement != nullptr) {
    //make sure that the element we are operating on is a media element
        if(sscanf(arg[0].c_str(), "%lf", &argValue) == 1) 
        {
            location.setCurrentTime(mediaElement, argValue);
        }
        if(sscanf(arg[1].c_str(), "%lf", &argValue) == 1) 
        {
            location.setPlaybackRate(mediaElement, argValue);
        }
        if(arg[2].size()) 
        {  //arg is specified.
            location.setMediaPlaying(mediaElement, arg[2]);
        }
        if(arg[3].size()) 
        {
            if(sscanf(arg[3].c_str(), "%lf", &argValue) == 1)
            {
                location.setVolume(mediaElement, argValue);
            }
                
        }

    }
}


//REQUIRES 4 ARGS - HANDLES BOTH HIPE_OP_DIALOG and HIPE_OP_DIALOG_INPUT
void handle_DIALOG(Container* c, hipe_instruction* instruction, bool, QWebElement, 
                    std::string arg[]) {

    Container* target = c->getParent();

    if(!target) { //we are the top level. Dialog is handled here directly
        bool cancelled;
        bool editable = (bool) (instruction->opcode == HIPE_OP_DIALOG_INPUT);

        int choiceNumber;
        std::string userChoice = ((ContainerTopLevel*)c)->dialog(arg[0], 
                                    arg[1], arg[2], editable, &cancelled, &choiceNumber);

        if(!cancelled) { //dialog wasn't cancelled
            std::string itemIndexStr = "";

            if(!editable) { //fixed choices; the number of the choice as sent (counting from 1, blanks included).
                itemIndexStr = std::to_string(choiceNumber);
            } else { //prompt allowed string entry. Choice position index not relevant.
                itemIndexStr="1"; //Return the index value 1 for non-cancellation
            }
            c->client->sendInstruction(HIPE_OP_DIALOG_RETURN, instruction->requestor,
                            0, {userChoice, itemIndexStr});

        } else { //cancelled
            c->client->sendInstruction(HIPE_OP_DIALOG_RETURN, instruction->requestor, 
                            0, {"","0"});
        }
    } else { //relay to parent frame.
        target->receiveMessage(instruction->opcode, instruction->requestor, 
                {arg[0],arg[1],arg[2],arg[3]}, c->webElement.webFrame(), false);
        //this sends the instruction to the parent's client.
    }
}


//REQUIRES 4 ARGS - HANDLES BOTH HIPE_OP_DIALOG
void handle_DIALOG_RETURN(Container* c, hipe_instruction* instruction, bool locationSpecified, QWebElement location, std::string arg[]) {
    if(locationSpecified) {
    //The fact we're receiving from the client and not sending this,
    //means a location is mandatory. We simply relay this instruction to the child frame.

        Container* target = nullptr;
        //find the relevant child frame client
        FrameData* fd = c->lookupSubFrame(location);
        //the child frame may have already disconnected; guard the lookup.
        Connection* tc = fd ? identifyFromFrame(fd->wf) : nullptr;
        if(tc) target = tc->container;

        if(target) {
            //The user answering a dialog shown by the top level (the framing manager) counts as
            //user input for the app it was shown for, e.g. for a paste chosen from that dialog.
            //(Relays by other frames don't count: they could forge the answer.)
            if(c->isTopLevel) target->noteUserInput();
            target->receiveMessage(HIPE_OP_DIALOG_RETURN, instruction->requestor, {arg[0],arg[1],
                    arg[2],arg[3]}, nullptr, false);
        }
    }
}


//REQUIRES 1 ARG
void handle_GET_SELECTION(Container* c, hipe_instruction* instruction, bool, QWebElement location, std::string arg[]) {
    std::string selectedText = ""; //if nothing is selected, or the functionality is
    //outside the client's purview to see, a blank string will be returned.
    if(arg[0] == "1") { //a top-level window has requested the global selection.
        if(c->isTopLevel) {
            selectedText = ((ContainerTopLevel*)c)->getGlobalSelection(false);
        }
    } else { //get local (this frame's) selection.
        selectedText = location.getSelection().toStdString();
    }
    //return the contents of the selection...
    c->client->sendInstruction(HIPE_OP_CONTENT_RETURN, instruction->requestor,
                                       instruction->location, {selectedText});
}


//Resolves the container an EDIT_ACTION/EDIT_STATUS applies to (the requester's own, or the child frame
//named by location) and decides whether the requester may act on it now. Every client in a top-level
//window is a frame of one QWebPage, and edit actions act on the page's focused frame, so:
//- the top level (framing manager) is trusted: if it names a child whose frames don't contain the focus,
//  focus is moved to that child so the action applies to it;
//- any other frame may only act while the focus is inside the target's own frames, so it can never act
//  on another client's selection or undo history.
//Returns the target, or nullptr if the child is gone or the requester may not act.
static Container* editTarget(Container* c, bool locationSpecified, QWebElement location) {
    Container* target = c;
    if(locationSpecified) {
        FrameData* fd = c->lookupSubFrame(location);
        //resolve the child frame's connection; it may have already disconnected.
        Connection* tc = fd ? identifyFromFrame(fd->wf) : nullptr;
        if(!tc) return nullptr;
        target = tc->container;
    }
    bool focusInTarget = target->containsFrame(c->frame->page()->currentFrame());
    if(c->isTopLevel) {
        if(locationSpecified && !focusInTarget) target->frame->setFocus();
        return target;
    }
    return focusInTarget ? target : nullptr;
}

//REQUIRES 2 ARGS
void handle_EDIT_ACTION(Container* c, hipe_instruction*, bool locationSpecified, QWebElement location, std::string arg[]) {
    if(!arg[0].size()) return;
    char code = arg[0][0];
    Container* target = editTarget(c, locationSpecified, location);
    if(!target) return;
    //Pasting hands the clipboard's contents to whichever client has focus, so a framed client may only
    //paste in response to the user: shortly after they clicked or typed in it, or answered a dialog the
    //top level showed for it.
    if(!c->isTopLevel && (code == 'v' || code == 'V') && !c->hadRecentUserInput()) return;
    if(code == 't') target->insertText(arg[1]);
    else target->triggerEditAction(code);
}


//REQUIRES 1 ARG
void handle_EDIT_STATUS(Container* c, hipe_instruction* instruction, bool locationSpecified, QWebElement location, std::string arg[]) {
    //for this op, the user specifies a string of edit function codes to check,
    //e.g. "xcvbiu" to check cut,copy,paste,bold,italic,underline.
    //A frame that may not act on the target now (see editTarget()) gets 'e' (not available) for every code.
    Container* target = editTarget(c, locationSpecified, location);

    std::string resultingStates = "";
    for(size_t i=0; i<arg[0].size(); i++) //for each edit function to be checked
        resultingStates += target ? target->editActionStatus(arg[0][i]) : 'e';

    c->client->sendInstruction(HIPE_OP_EDIT_STATUS, instruction->requestor,
                            instruction->location, {arg[0], resultingStates});
}


//REQUIRES 4 ARGS - handles several different instructions including FIFO_* and OPEN_LINK
void handle_MESSAGE(Container* c, hipe_instruction* instruction, bool locationSpecified, QWebElement location, std::string arg[]) {
    //Determine whether we need to send the message to the parent frame or a child frame.
    Container* target = nullptr;
    QWebFrame* sourceframe = nullptr;
    if(locationSpecified) {
        //find the relevant child frame client
        FrameData* fd = c->lookupSubFrame(location);
        //the destination frame may have already disconnected (common during a FIFO
        //exchange when a peer quits mid-transfer); guard against a null connection.
        Connection* tc = fd ? identifyFromFrame(fd->wf) : nullptr;
        if(tc) target = tc->container;
    } else { //send to parent element
        target = c->getParent();
        sourceframe = c->webElement.webFrame();
    }
    if(target) { //send the instruction to the destination. (at top level, target is nullptr)
        target->receiveMessage(instruction->opcode, instruction->requestor,
                {arg[0], arg[1], arg[2], arg[3]}, sourceframe);
    } else if(locationSpecified) {
    //the destination child frame has gone away (e.g. a FIFO peer quit mid-transfer).
    //Nothing to relay to and no top-level fallback applies here - drop the message.
        return;
    } else if(instruction->opcode == HIPE_OP_FIFO_GET_PEER && c->isTopLevel) {
    //special case for HIPE_OP_FIFO_GET_PEER instruction where a frame tries to
    //send it outside the top level. This would normally mean an application wishes
    //to import or export a file but is running in a top-level window in another
    //desktop environment. In this case, display an open/save dialog in order to
    //give the client application a real file to work with.

        std::string accessModeStr = arg[1];

        std::string filepath = ((ContainerTopLevel*)c)->selectFileResource(arg[0],
                   arg[2], accessModeStr);
        //accessModeStr is modified by-reference to now reflect the actual access modes granted.
        //(Only r and w are supported at top level)

        //separate filename and extension parts...
        size_t strPos = filepath.rfind("/");
        if(strPos == std::string::npos) strPos = 0; else strPos++;
        std::string filename = filepath.substr(strPos);
        strPos = filename.rfind("."); //isolate file extension
        std::string fileType;
        if(strPos != std::string::npos) {
            fileType = filename.substr(strPos+1);
        }

        //send reply to client
        c->receiveMessage(HIPE_OP_FIFO_RESPONSE, instruction->requestor, 
                   {filepath, accessModeStr, filename, fileType}, nullptr);
        
    } else if(instruction->opcode == HIPE_OP_OPEN_LINK) {
    //special case for opening URL in user's browser if already running at top level.
        QDesktopServices::openUrl(QUrl(arg[0].c_str()));
    }
}


//REQUIRES 1 ARG
void handle_TOGGLE_CLASS(Container*, hipe_instruction*, bool, QWebElement location, std::string arg[]) {
    location.toggleClass(arg[0].c_str());
}


//REQUIRES 1 ARG
void handle_SET_CURSOR(Container* c, hipe_instruction*, bool locationSpecified, QWebElement location, std::string arg[]) {
    //set the cursor to a unicode character, using foreground and background colors of the container.
    
    location.setStyleProperty("cursor", "default"); //reset cursor first. Otherwise webkit fails to set the cursor correctly.
    location.setStyleProperty("cursor", 
        Sanitation::mouseCursorFromUnicode(arg[0], c->fg.name().toStdString(), c->bg.name().toStdString(), arg[1]).c_str());

    //for the body element, store the cursor character and hotspot so it can be regenerated if the fg/bg colors change.
    if(!locationSpecified) {
        c->cursorSymbol = arg[0];
        c->cursorHotspot = arg[1];
    }
    
}
