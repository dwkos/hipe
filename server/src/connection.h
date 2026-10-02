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

/// Connection class. Objects of this class read input instructions from the connected
/// client application one at a time, assemble a preamble first then
/// a complete instruction, then pass the instruction to either the containerManager
/// or the client's container object to deal with.
///
///Lifetime:
/// - Created by connectionManager, then left to be autonomous.
/// - Discards itself when connection is closed.
/// - Makes requests to ContainerManager, and exchanges instructions with a container object provided by same.

//OWNERSHIP:
//The connection owns the on-screen container object. It is responsible for
//deleting the container object when destroyed.

#ifndef CONNECTION_H
#define CONNECTION_H

#define READ_BUFFER_SIZE 65536 //large enough to take a buffered client's whole flush (see libhipe's
//hipe_set_buffered()) in one read, instead of one read per few instructions.

#include <QObject>
#include <queue>
#include <map>
#include <tuple>
#include <utility>
#include "common.h"
#include "container.h"
#include <mutex>

class Connection
{
private:
    bool connected = true;
public:
    Connection(int clientFD);
    ~Connection();

    void sendInstruction(char opcode, uint64_t requestor, uint64_t location, const std::vector<std::string>& args = {});
    void sendInstruction(hipe_instruction& instruction);
    Container* container;

    bool service();
    //The hiped event loop iterates over each activeConnection and calls the
    //service() function in each, returning to an idle state if all connections
    //return false (unproductive call). The purpose of service() is to check if
    //an incoming instruction has been queued by the socket thread and service
    //it in the primary/GUI thread; by modifying the GUI appropriately.

    inline bool isConnected() {return connected;}
    void disconnect();

    void _readyRead();

    static void _receiveUIEvent(const QString& eventName, void* connectionPtr, uint64_t location, uint64_t requestor, const QString& eventDetails);

    // Sends any resize/scroll/mousemove events still pending from coalescing (see
    // _receiveUIEvent). Called once per serviceConnections() tick from main.cpp,
    // same GUI-thread loop that already visits every live Connection -- no extra
    // locking needed since nothing else touches pendingCoalescedEvents off-thread.
    // Returns true if anything was actually sent, so a coalescible-event burst with no
    // *inbound* client traffic still counts as "productive" and keeps ConnectionManager's
    // adaptive poll timer at its fast interval instead of idling back to 80ms.
    bool flushPendingCoalescedEvents();

private:
    int clientFD; //socket descriptor of the connection.
    pid_t clientPID;

    char readBuffer[READ_BUFFER_SIZE];
    //where we put characters that have been read in over the connection.

    instruction_decoder currentInstruction;

    void runInstruction(hipe_instruction* instruction);
    //deals with a completed instruction -- sends it to wherever it's needed.

    std::queue<hipe_instruction*> incomingInstructions;

    std::mutex mIncomingInstructions;
    //instruction queue is filled by the incomingInstruction thread, and emptied
    //in the main thread. Mutual exclusion must be enforced to ensure no two
    //threads access the queue simultaneously.

    std::mutex mWriteProtect;
    //mutex to enforce atomicity of write calls when sending messages back
    //to client.

    // Events for which only the latest value per (eventName, location) is worth keeping --
    // a burst of these (a live resize/scroll/mouse drag outrunning the client's processing
    // rate) collapses to one instruction per flush instead of flooding the socket with
    // now-stale intermediate values. _receiveUIEvent() overwrites entries here instead of
    // sending immediately; flushPendingCoalescedEvents() sends whatever's left standing.
    static bool isCoalescibleEvent(const std::string& eventName);

    struct PendingUIEvent {
        uint64_t requestor;
        std::string eventDetails;
    };
    std::map<std::tuple<std::string, uint64_t, uint64_t>, PendingUIEvent> pendingCoalescedEvents;
    //keyed by (eventName, location, requestor): separate requests for the same event each get their own
    //stream, as they do for events that aren't coalesced.

};

#endif // CONNECTION_H
