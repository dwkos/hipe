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


#include "connectionmanager.h"
#include "connection.h"
#include "main.hpp"
#include <QTimer>

//The global server object accepts and manages connections.
//ONE connection propagates ONE client connection thread,
//which enables ONE client

//The socket server that listens for and accepts new connections


ConnectionManager* ConnectionManager::instance = nullptr;
std::atomic<bool> ConnectionManager::wakePending(false);

void ConnectionManager::wake() {
    if(instance && !wakePending.exchange(true))
        QMetaObject::invokeMethod(instance, "onTimerEvent", Qt::QueuedConnection);
}

ConnectionManager::ConnectionManager(QObject *parent) :
    QObject(parent)
{
    instance = this;
    timer = new QTimer();
    timer->connect(timer, SIGNAL(timeout()), this, SLOT(onTimerEvent()));
    timer->start(40); //ms interval.
}



// A burst of coalesced outbound events (resize/scroll/mousemove/wheel - see Connection::
// flushPendingCoalescedEvents()) only arrives as fast as the underlying input does, which can
// have real gaps between individual samples wider than one fast-poll tick. Staying on the fast
// poll for this long after the last productive tick, rather than reverting to the slow one the
// instant a single tick finds nothing pending, keeps the whole burst flowing at close to the
// fast rate instead of oscillating between the two.
static const int BURST_GRACE_MS = 50;

void ConnectionManager::onTimerEvent() {
//when a timer event occurs in the main hiped event loop,
//go thru the connection list and service all events.
    wakePending = false; //set before servicing: instructions queued from now on need a new wake-up
    timer->stop();
    bool productive = serviceConnections(); //returns true if the call was productive
    if(productive) {
        serviceConnections();
        serviceConnections();
        serviceConnections(); //add more calls -- performance experiment.
        serviceConnections();
        serviceConnections();
        serviceConnections();
        serviceConnections();
        sinceLastProductive.start();
    }

    if(productive || (sinceLastProductive.isValid() && sinceLastProductive.elapsed() < BURST_GRACE_MS)) {
        timer->start(1);  //next event should therefore be scheduled sooner.
    } else {
        timer->start(80); //nothing much going on right now.
    }
}
