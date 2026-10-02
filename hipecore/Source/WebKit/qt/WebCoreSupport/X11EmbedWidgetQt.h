/*
 * Copyright (C) 2026 Daniel Kos
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
#ifndef X11EmbedWidgetQt_h
#define X11EmbedWidgetQt_h

#include "Widget.h"

QT_BEGIN_NAMESPACE
class QWindow;
QT_END_NAMESPACE

class QWebPageClient;

namespace WebCore {

// A Widget that owns a pair of native child X11 windows kept positioned and sized to this
// element's CSS render box on every layout pass (the same layout-driven geometry sync
// RenderWidget already gives any Widget it owns), so a client can XReparentWindow a foreign X11
// window into it (see QWebElement::x11EmbedTargetXid()).
//
// Two windows, not one, so that a client's reparented content gets hard-clipped to whatever
// ancestor (an overflow:hidden box, a scrolled pane, ...) currently constrains this element's own
// visible area, without the client ever needing to know that happened:
//   - m_outerWindow: owned by us, sized/positioned to the *visible* (ancestor-clip-intersected)
//     portion of the element's box. This is the one X11 actually enforces clipping against.
//   - m_innerWindow: owned by us, a child of m_outerWindow, always sized to the element's *full*
//     unclipped CSS box - this is what xid() reports and what a client reparents into. It's
//     positioned within m_outerWindow with a <=0 offset in each axis whenever that side is
//     clipped away, so the correct sub-region lines up with what m_outerWindow actually shows;
//     X11's ordinary parent-clips-child behavior does the rest, with zero client cooperation.
//
// Deliberately not a PluginView: there is no NPAPI plugin instance behind
// this, so it does not go through PluginPackage/NPWindow/dispatchNPEvent at
// all. It also does not use Widget's own setPlatformWidget()/platformWidget()
// storage, because QWebPageClient::setWidgetVisible() (what Widget::show()/
// hide() delegate to) only knows how to toggle a QWidget's visibility, not a
// bare child QWindow's - so this class tracks its QWindows itself and pushes
// visibility to them directly.
class X11EmbedWidgetQt final : public Widget {
public:
    X11EmbedWidgetQt();
    virtual ~X11EmbedWidgetQt();

    // The X11 window ID of the inner content window, or 0 if it hasn't been created yet (created
    // lazily, the first time this widget is parented). Always the element's full unclipped size -
    // see the class comment for why a client reparenting into this doesn't need to care about
    // ancestor clipping itself.
    quintptr xid() const { return m_xid; }

    // RAII: for the scope's lifetime, every X11EmbedWidgetQt's paint() grabs and blits its
    // embedded content instead of leaving its region untouched for the (in this scope, absent)
    // native window to show through live. Held by the one call site that produces a snapshot
    // (QWebFrameAdapter::renderContentsForSnapshot()) around the whole top-level paint - nested
    // FrameViews (a docked client's own frame, e.g.) paint underneath it too, so this covers them
    // the same way. Deliberately not detected from the destination QPainter's device at each
    // individual paint() call: confirmed live that a nested FrameView's paint dispatch does not
    // reliably propagate the real target device that deep - QPainter::device()->devType() came
    // back QInternal::UnknownDevice for a docked client's embedded content during an actual
    // HIPE_OP_TAKE_SNAPSHOT, even though paint() itself was correctly reached.
    class SnapshotScope {
    public:
        SnapshotScope() { ++s_depth; }
        ~SnapshotScope() { --s_depth; }
        SnapshotScope(const SnapshotScope&) = delete;
        SnapshotScope& operator=(const SnapshotScope&) = delete;
    };

private:
    void setParent(ScrollView*) override;
    void setFrameRect(const IntRect&) override;
    void frameRectsChanged() override;
    void invalidateRect(const IntRect&) override { }
    void show() override;
    void hide() override;
    void setParentVisible(bool) override;
    void paint(GraphicsContext&, const IntRect&) override;

    QWebPageClient* pageClient() const;
    void createPlatformWindows();
    void updateGeometry();

    // Grabs the reparented foreign window's current pixels via XComposite and blits them into
    // context in place of the (normally invisible) native window - see the .cpp for why paint()
    // only does this while s_capturingSnapshot is set, never for the live on-screen case.
    void paintCapturedContent(GraphicsContext&);

    QWindow* m_outerWindow { nullptr };
    QWindow* m_innerWindow { nullptr };
    quintptr m_xid { 0 };
    static int s_depth;
};

}

#endif
