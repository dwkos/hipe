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

#ifndef SERVER_H
#define SERVER_H

#include <QTimer>
#include <QElapsedTimer>
#include <atomic>

//The name 'connection manager' is no longer relevant. This class manages some
//Qt events.

class ConnectionManager : public QObject
{
    Q_OBJECT
public:
    explicit ConnectionManager(QObject *parent = 0);

    static void wake();
    //Thread-safe: asks the main thread to service connections as soon as it can, instead of at the
    //next timer tick. Called by the socket thread when it has queued incoming instructions, so the
    //first instruction after an idle spell isn't left waiting for the slow poll interval.
private:
    static ConnectionManager* instance;
    static std::atomic<bool> wakePending; //at most one wake-up queued at a time
    QTimer* timer;
    // Tracks how long ago the last productive serviceConnections() tick was, so a burst with
    // real gaps between individual events (e.g. coalesced resize/scroll/mousemove, which only
    // arrive as fast as the underlying input does) doesn't fall back to the slow poll interval
    // the instant a single tick finds nothing pending. Invalid until the first productive tick.
    QElapsedTimer sinceLastProductive;
signals:
public slots:
    void onTimerEvent(); //bound to the main event loop.
};

#endif // SERVER_H
