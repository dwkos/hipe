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


// Alternative mouse cursor rendering for hardware where a mouse cursor is not adequately provided.
// This version implements a QGraphicsItem for hardware-accelerated rendering in a QGraphicsScene.

#pragma once

#include <QPainter>
#include <QGraphicsItem>
#include <QTimer>
#include <QGraphicsView>
#include <QPointer>

class Container;
class WebGraphicsView;

class MouseCursor : public QObject, public QGraphicsItem {
    Q_OBJECT
    Q_INTERFACES(QGraphicsItem)
    
public:
    MouseCursor(Container* c, WebGraphicsView* view);
    ~MouseCursor();
    
    // QGraphicsItem required methods
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;
    
    // Update cursor state
    void setPressed(bool pressed);
    
public slots:
    void updateCursorPosition();

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;
    
    // New slot to safely handle viewport destruction
    void viewportDestroyed(QObject* obj);

private:
    Container* c;
    WebGraphicsView* m_view;  // We'll use a raw pointer with careful null checking
    bool mousePressed = false;
    QTimer m_updateTimer;
    QPointF m_cursorPos;
    bool m_eventFilterInstalled = false;
    QPointer<QWidget> m_viewport;  // Use QPointer which automatically becomes null when object is destroyed
    QRectF m_cachedBoundingRect;
};
