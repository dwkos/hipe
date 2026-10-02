/*
    Copyright (C) 2025 Daniel Kos

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


// Implements a mouse cursor sprite for hardware-accelerated rendering in a QGraphicsScene

#include "mousecursor.h"
#include "container.h"
#include "containertoplevel.h"

#include <QApplication>
#include <QCursor>
#include <QGraphicsScene>
#include <QScreen>

MouseCursor::MouseCursor(Container* c, WebGraphicsView* view) 
    : QGraphicsItem(nullptr), c(c), m_view(view), mousePressed(false)
{
    // Hide the system cursor
    QApplication::setOverrideCursor(Qt::BlankCursor);
    
    // Set cursor to always be on top
    setZValue(1000);
    
    // Add this cursor to the scene
    if (view && view->scene) {
        view->scene->addItem(this);
    }

    // Calculate the bounding rectangle. Note: this assumes the primary
    // screen is the one where the view is located, and the screen size
    // will not change during this top-level contianer's lifetime.
    QScreen* primaryScreen = QApplication::primaryScreen();
    if (primaryScreen) {
        QRect screenGeometry = primaryScreen->geometry();
        m_cachedBoundingRect = QRectF(
            -screenGeometry.width(), -screenGeometry.height(),
            screenGeometry.width() * 3, screenGeometry.height() * 3
        );
    } else { //fallback to a reasonably large rectangle
        m_cachedBoundingRect = QRectF(-2000, -2000, 4000, 4000);
    }
    
    // Install event filter on the view's viewport to capture mouse events
    if (view && view->viewport()) {
        // Store viewport in a QPointer for automatic nullification
        m_viewport = view->viewport();
        
        // Install event filter
        m_viewport->installEventFilter(this);
        m_eventFilterInstalled = true;
    }
    
    // Set up timer for cursor position updates
    m_updateTimer.setInterval(33); // ~30 FPS
    connect(&m_updateTimer, &QTimer::timeout, this, &MouseCursor::updateCursorPosition);
    m_updateTimer.start();
    
    // Initialize cursor position
    updateCursorPosition();
}

MouseCursor::~MouseCursor()
{
    // Stop the timer first to prevent any more updates
    m_updateTimer.stop();
    
    // Safely remove event filter only if it was installed and viewport still exists
    if (m_eventFilterInstalled && !m_viewport.isNull()) {
        m_viewport->removeEventFilter(this);
    }
    
    // Restore the system cursor
    QApplication::restoreOverrideCursor();
}

void MouseCursor::viewportDestroyed(QObject* obj)
{
    // Mark the event filter as no longer installed since the viewport is gone
    m_eventFilterInstalled = false;
}

QRectF MouseCursor::boundingRect() const
{
    return m_cachedBoundingRect;
}

void MouseCursor::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    QRectF rect = boundingRect();
    painter->setRenderHint(QPainter::Antialiasing, false);

    // Use integer coordinates for the cursor position
    int x = qRound(m_cursorPos.x());
    int y = qRound(m_cursorPos.y());

    // Draw crosshair lines
    painter->setPen(QPen(!mousePressed ? c->cursorColor1 : c->cursorColor3, 1));
    painter->drawLine(x, qRound(rect.top()), x, qRound(rect.bottom()));
    painter->drawLine(qRound(rect.left()), y, qRound(rect.right()), y);
    
    // Draw center circle
    if (!mousePressed) {
        painter->setPen(QPen(c->cursorColor2, 3));
        painter->drawEllipse(x-8, y-8, 16, 16);
        painter->setPen(QPen(c->cursorColor3, 1));
        painter->drawEllipse(x-8, y-8, 16, 16);
    } else {
        painter->setPen(QPen(c->cursorColor2, 3));
        painter->drawEllipse(x-6, y-6, 12, 12);
        painter->setPen(QPen(c->cursorColor3, 1));
        painter->drawEllipse(x-6, y-6, 12, 12);
    }
}

bool MouseCursor::eventFilter(QObject* obj, QEvent* event)
{
    // Only process events if the view still exists and this is its viewport
    if (m_view && obj == m_viewport) {
        switch (event->type()) {
            case QEvent::MouseButtonPress:
                mousePressed = true;
                update();
                break;
                
            case QEvent::MouseButtonRelease:
                mousePressed = false;
                update();
                break;
                
            case QEvent::MouseMove: {
                QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);
                m_cursorPos = m_view->mapToScene(mouseEvent->pos());
                update();
                break;
            }
            
            default:
                break;
        }
    }
    
    return QObject::eventFilter(obj, event);
}

void MouseCursor::setPressed(bool pressed)
{
    if (mousePressed != pressed) {
        mousePressed = pressed;
        update();
    }
}

void MouseCursor::updateCursorPosition()
{
    if(!m_view) return;
    // Only update if the view still exists

    QPoint globalPos = QCursor::pos();
    QPoint viewPos = m_view->mapFromGlobal(globalPos);
    QPointF scenePos = m_view->mapToScene(viewPos);
    
    if (m_cursorPos != scenePos) { //avoid unnecessary repaints
        m_cursorPos = scenePos;
        update();
    }

}



