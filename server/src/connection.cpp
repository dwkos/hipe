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


#include "connection.h"
#include "connectionmanager.h"
#include "container.h"
#include "common.h"
#include "main.hpp"
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include <iostream>

// An instruction backlog this deep suggests a client is burst-inserting content (many appends
// with no wait in between, e.g. building a big list or scattering many positioned elements) --
// extend hipecore's layout-batching-delay window the same way a table tag arriving does, so
// non-table bulk mutations get some of the same coalescing. Checked once per drain rather than
// per instruction, since the whole backlog gets processed together regardless.
//
// The bump scales with how far over the threshold the backlog actually is, rather than a flat
// amount -- a live trace on a real burst showed the backlog reaching 86 while a flat 50ms bump
// was in effect, meaning the layout timer could still fire while dozens more mutations were still
// queued. LAYOUT_BATCHING_BUMP_MS_BASE (50ms) matches the flat bump HTMLTableElement/
// HTMLTablePartElement already use on the hipecore side, applied right at the threshold; it grows
// by LAYOUT_BATCHING_BUMP_MS_PER_EXTRA for each instruction beyond that, capped at
// LAYOUT_BATCHING_BUMP_MS_MAX.
//
// LAYOUT_BATCHING_BUMP_MS_MAX was originally 250ms (the same ceiling stock WebCore's old one-time
// initial-layout window used). Lowered to 80ms after removing the client-side round-trip-per-
// element bug from biglist.c/bigtable.c (see their own history) exposed how oversized that was:
// a microsecond-precision trace of a 4808-instruction table-build burst showed the entire backlog
// already queued (client had finished sending before hiped's main loop even checked once), fully
// drained in ~41ms, then hiped sitting idle for the remaining ~210ms of the 250ms window before
// laying out -- ~86% of total latency was pure wait, not work. 80ms keeps roughly 2x headroom over
// that measured drain time (for real bursts even larger than this one, or client/network jitter on
// a non-loopback connection) while cutting worst-case wasted tail latency by ~68%.
static const size_t LAYOUT_BATCHING_BUMP_THRESHOLD = 8;
static const int LAYOUT_BATCHING_BUMP_MS_BASE = 50;
static const int LAYOUT_BATCHING_BUMP_MS_PER_EXTRA = 2;
static const int LAYOUT_BATCHING_BUMP_MS_MAX = 80;

Connection::Connection(int clientFD)
{
    this->clientPID = 0; //to be obtained once data starts flowing.

    this->clientFD = clientFD;
    container = nullptr;

    instruction_decoder_init(&currentInstruction);

    registerConnection(this, clientFD); //con->socketDescriptor());
}

Connection::~Connection()
//this happens in the main thread, in the service cycle.
//When connections are serviced, disconnected instances are cleaned up and unmapped in that thread.
{
    disconnect(); //mark as disconnected if not already done.
    instruction_decoder_clear(&currentInstruction);
    delete container;
}

void Connection::sendInstruction(char opcode, uint64_t requestor, uint64_t location, const std::vector<std::string>& args)
{
    if(!connected) return;
    hipe_instruction instruction;
    hipe_instruction_init(&instruction);
    instruction.opcode = opcode;
    instruction.requestor = requestor;
    instruction.location = location;
    for(size_t i=0; i<args.size(); i++) {
        if(args[i].size()) {
            instruction.arg[i] = (char*) args[i].data();
            instruction.arg_length[i] = args[i].size();
        }
    }

    sendInstruction(instruction);
}

void Connection::sendInstruction(hipe_instruction& instruction)
{
    if(!connected) return;
    std::lock_guard<std::mutex> guard(mWriteProtect);

    instruction_encoder outgoingInstruction;
    instruction_encoder_init (&outgoingInstruction);
    instruction_encoder_encodeinstruction(&outgoingInstruction, instruction);

    ssize_t charsWritten;
    const char* bufferToWrite = (const char*) outgoingInstruction.encoded_output;
    size_t bytesRemaining = outgoingInstruction.encoded_length;
    while(bytesRemaining > 0) {
        charsWritten = write(clientFD, bufferToWrite, bytesRemaining);
        if(charsWritten < 1) {
            disconnect(); //has been disconnected at other end perhaps.
            break;
        } else {
            bytesRemaining -= charsWritten;
            bufferToWrite += charsWritten;
        }
    }

    instruction_encoder_clear(&outgoingInstruction);
}

void Connection::runInstruction(hipe_instruction* instruction)
{
    if(!connected) return;
    if(instruction->opcode == HIPE_OP_REQUEST_CONTAINER) {

        //Get credentials for the client process.
        socklen_t credLen = sizeof(struct ucred);
        struct ucred pidCredentials;
        if(0==getsockopt(clientFD, SOL_SOCKET, SO_PEERCRED, &pidCredentials, &credLen)) {
            //got client creds.
            this->clientPID = pidCredentials.pid;
        }
        if(!clientPID) //fallback to self-reported PID if above approach fails.
            this->clientPID = instruction->requestor;

        int themeIndex=1; //1 is the default value for theme index.
        if(instruction->arg_length[2]) {
        //theme index is specified. Where multiple css files were specified at the
        //hiped command line this indicates the index of the theme to be used
        //(or if 0 means that no loaded theme should be used at all.)
            try {
                themeIndex = std::stoi(
                    std::string(instruction->arg[2],instruction->arg_length[2]));
            } catch(...) {
                themeIndex = 1; //if invalid, fallback to default theme.
            }
        }


        container = requestContainerFromKey(std::string(instruction->arg[0],
                    instruction->arg_length[0]), std::string(instruction->arg[1],
                    instruction->arg_length[1]), this->clientPID, themeIndex, this);
        //send the result of the container request (arg1 represents approved/denied)
        sendInstruction(HIPE_OP_CONTAINER_GRANT, 0,0, {(container ? "1":"0"),"0"}); 
        //new client awaits this confirmation that its key has been approved.
    } else if(container) { //allow other instructions only if a container request has already been granted.
        //send the instruction to the container.
        container->receiveInstruction(*instruction);
    } else {
        //error. Access was not granted, so no other instructions are permitted.
        sendInstruction(HIPE_OP_SERVER_DENIED, 0,0); //access denied.
    }
}

bool Connection::service() {
//The hiped event loop iterates over each activeConnection and calls the
//service() function in each, returning to an idle state if all connections
//return false (unproductive call). The purpose of service() is to check if an
//incoming instruction has been queued by the socket thread and service it in
//the primary/GUI thread; by modifying the GUI appropriately.
    if(!connected) return false;
    std::lock_guard<std::mutex> guard(mIncomingInstructions);
    if(incomingInstructions.empty()) return false; //unproductive call.
#ifdef HAVE_HIPECORE
    if(container && incomingInstructions.size() >= LAYOUT_BATCHING_BUMP_THRESHOLD) {
        size_t extra = incomingInstructions.size() - LAYOUT_BATCHING_BUMP_THRESHOLD;
        int bumpMs = LAYOUT_BATCHING_BUMP_MS_BASE + (int)extra * LAYOUT_BATCHING_BUMP_MS_PER_EXTRA;
        if(bumpMs > LAYOUT_BATCHING_BUMP_MS_MAX) bumpMs = LAYOUT_BATCHING_BUMP_MS_MAX;
        container->webElement.bumpLayoutBatchingDelay(bumpMs);
    }
#endif
    hipe_instruction* hi;
    while(!incomingInstructions.empty()) {
        hi = incomingInstructions.front();
        incomingInstructions.pop();
        runInstruction(hi);
        hipe_instruction_clear(hi);
        delete hi;
        if(!connected) return false;
    }
    return true; //this was a productive call.
}

void Connection::disconnect() {
    connected = false;
    shutdown(clientFD, SHUT_RDWR);
    close(clientFD);
}

void Connection::_readyRead()
{
    if(!connected) return;
    ssize_t bufferedChars; //the number of characters that have been read into the buffer. Must be <=READ_BUFFER_SIZE

    bool queuedAny = false;

    //attempt to read new characters
    bufferedChars = read(clientFD, readBuffer, READ_BUFFER_SIZE); //blocking call!
    //can return -1 if connection closed, or 0 when no more ready.

    if(bufferedChars <= 0) { //connection closed
        disconnect();
        return;
    } else for(int p=0; p<bufferedChars;) { //let's process our input! (Iterate for each character we've read in)
        //(after the loop, the main thread is woken if any instruction was completed.)
        p += instruction_decoder_feed(&currentInstruction, readBuffer+p, bufferedChars-p);
        if(instruction_decoder_iscomplete(&currentInstruction)) { //check if instruction is complete.
            hipe_instruction* newInstruction = new hipe_instruction;
            hipe_instruction_move(newInstruction, &(currentInstruction.output));
            std::lock_guard<std::mutex> guard(mIncomingInstructions);
            incomingInstructions.push(newInstruction);
            instruction_decoder_clear(&currentInstruction);
            queuedAny = true;
        }
    }
    if(queuedAny) ConnectionManager::wake(); //don't make the instructions wait for the next poll tick.
}

bool Connection::isCoalescibleEvent(const std::string& eventName) {
    return eventName == "resize" || eventName == "scroll" || eventName == "mousemove" || eventName == "wheel";
}

//Parses a wheel event detail, "deltaX,deltaY,deltaMode,mask". Returns false if it isn't in that form.
static bool parseWheelDetail(const std::string& detail, double& dx, double& dy, std::string& modeAndMask) {
    size_t c1 = detail.find(','), c2 = c1 == std::string::npos ? c1 : detail.find(',', c1 + 1);
    if(c2 == std::string::npos) return false;
    try {
        size_t used;
        dx = std::stod(detail.substr(0, c1), &used);
        if(used != c1) return false;
        dy = std::stod(detail.substr(c1 + 1, c2 - c1 - 1), &used);
        if(used != c2 - c1 - 1) return false;
    } catch(...) {
        return false;
    }
    modeAndMask = detail.substr(c2 + 1);
    return true;
}

static std::string deltaString(double value) {
    char buf[32];
    snprintf(buf, sizeof buf, "%.3f", value);
    std::string s = buf;
    s.erase(s.find_last_not_of('0') + 1);
    if(s.back() == '.') s.pop_back();
    if(s == "-0") s = "0";
    return s;
}

void Connection::_receiveUIEvent(const QString& eventName, void* connectionPtr, uint64_t location, uint64_t requestor, const QString& eventDetails) {
    Connection* _this = (Connection*) connectionPtr;
    std::string name = eventName.toStdString();

    if (isCoalescibleEvent(name)) {
        auto key = std::make_tuple(name, location, requestor);
        std::string detail = eventDetails.toStdString();
        if (name == "wheel") {
            //Wheel deltas are relative, so a burst (a fast spin, or a mouse that sends several events per
            //notch) is merged by adding the deltas up, not by keeping the latest. Only events with the same
            //deltaMode and modifier mask are merged; otherwise the pending one is sent first.
            double dx, dy;
            std::string modeAndMask;
            if (!parseWheelDetail(detail, dx, dy, modeAndMask)) {
                _this->sendInstruction(HIPE_OP_EVENT, requestor, location, {name, detail});
                return;
            }
            auto pending = _this->pendingCoalescedEvents.find(key);
            if (pending != _this->pendingCoalescedEvents.end()) {
                double px, py;
                std::string pendingModeAndMask;
                if (parseWheelDetail(pending->second.eventDetails, px, py, pendingModeAndMask)
                        && pendingModeAndMask == modeAndMask) {
                    pending->second.eventDetails = deltaString(px + dx) + "," + deltaString(py + dy) + "," + modeAndMask;
                    return;
                }
                _this->sendInstruction(HIPE_OP_EVENT, requestor, location, {name, pending->second.eventDetails});
                _this->pendingCoalescedEvents.erase(pending);
            }
        }
        _this->pendingCoalescedEvents[key] = {requestor, detail};
        return;
    }

    _this->sendInstruction(HIPE_OP_EVENT, requestor, location, {name, eventDetails.toStdString()});
}

bool Connection::flushPendingCoalescedEvents() {
    if (pendingCoalescedEvents.empty()) return false;

    // Move out and clear first: sendInstruction()'s blocking write() shouldn't be able to
    // hold up newer resize/scroll/mousemove/wheel events from being coalesced correctly while
    // it's stuck waiting on a slow client.
    auto pending = std::move(pendingCoalescedEvents);
    pendingCoalescedEvents.clear();

    for (auto& entry : pending) {
        const std::string& eventName = std::get<0>(entry.first);
        uint64_t location = std::get<1>(entry.first);
        sendInstruction(HIPE_OP_EVENT, entry.second.requestor, location, {eventName, entry.second.eventDetails});
    }
    return true;
}

