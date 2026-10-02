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
#include "config.h"
#include "X11EmbedWidgetQt.h"

#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "HostWindow.h"
#include "QWebPageClient.h"
#include "RenderWidget.h"

#include <QImage>
#include <QPainter>
#include <QWindow>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xcomposite.h>

namespace WebCore {

// XComposite plumbing for grabbing a reparented foreign window's pixels - used only by
// paintCapturedContent(), see its definition below for why/when that runs. A private connection
// of our own, opened lazily and kept for the process's lifetime: infrequent, synchronous use (a
// handful of round-trips per capture, never per on-screen paint) doesn't justify sharing Qt's own
// xcb connection or WebCore's PlatformDisplay - both would need extra plumbing this stays free of.
static Display* compositeDisplay()
{
    static Display* display = XOpenDisplay(nullptr);
    return display;
}

static bool compositeExtensionAvailable(Display* display)
{
    static bool checked = false;
    static bool available = false;
    if (!checked) {
        int eventUnused, errorUnused;
        available = display && XCompositeQueryExtension(display, &eventUnused, &errorUnused);
        checked = true;
    }
    return available;
}

int X11EmbedWidgetQt::s_depth = 0;

X11EmbedWidgetQt::X11EmbedWidgetQt()
{
}

X11EmbedWidgetQt::~X11EmbedWidgetQt()
{
    delete m_outerWindow; // also destroys m_innerWindow, its QObject child.
}

QWebPageClient* X11EmbedWidgetQt::pageClient() const
{
    if (!is<FrameView>(parent()))
        return nullptr;
    HostWindow* hostWindow = downcast<FrameView>(*parent()).hostWindow();
    return hostWindow ? hostWindow->platformPageClient() : nullptr;
}

void X11EmbedWidgetQt::createPlatformWindows()
{
    QWebPageClient* client = pageClient();
    QWindow* parentWindow = client ? client->ownerWindow() : nullptr;
    if (!parentWindow)
        return;

    m_outerWindow = new QWindow(parentWindow);
    m_outerWindow->setFlags(Qt::SubWindow);
    m_outerWindow->create(); // Forces the underlying native (X11) window into existence.

    m_innerWindow = new QWindow(m_outerWindow);
    m_innerWindow->setFlags(Qt::SubWindow);
    m_innerWindow->create();
    m_innerWindow->setVisible(true); // Always mapped; m_outerWindow's visibility is what actually hides it.

    m_xid = static_cast<quintptr>(m_innerWindow->winId());

    // Redirect m_innerWindow's subwindows (i.e. whatever a client XReparentWindow()s into it, now
    // or later) to off-screen storage from the start, not only around a capture in
    // paintCapturedContent(). CompositeRedirectAutomatic still presents them on-screen normally -
    // this only additionally makes their pixels namable as a pixmap. Redirecting lazily, only for
    // the duration of a capture, would risk reading a freshly-redirected backing pixmap that the
    // (possibly idle) foreign client hasn't drawn into yet; redirecting for the widget's whole
    // life means it has always been tracking the client's real output by the time we read it.
    Display* display = compositeDisplay();
    if (compositeExtensionAvailable(display))
        XCompositeRedirectSubwindows(display, static_cast<Window>(m_xid), CompositeRedirectAutomatic);
}

void X11EmbedWidgetQt::updateGeometry()
{
    if (!m_innerWindow || !is<FrameView>(parent()))
        return;

    if (!isVisible()) {
        m_outerWindow->setVisible(false);
        return;
    }

    FrameView& frameView = downcast<FrameView>(*parent());
    IntRect objectRect(frameView.contentsToWindow(frameRect().location()), frameRect().size());

    IntRect visibleRect = objectRect;
    if (RenderWidget* renderWidget = RenderWidget::find(this))
        visibleRect.intersect(frameView.windowClipRectForFrameOwner(&renderWidget->frameOwnerElement(), true));

    if (visibleRect.isEmpty()) {
        m_outerWindow->setVisible(false);
        return;
    }

    m_outerWindow->setGeometry(visibleRect.x(), visibleRect.y(), visibleRect.width(), visibleRect.height());
    m_outerWindow->setVisible(true);

    // <=0 in each axis whenever that side of the element is currently clipped away - see the
    // class comment in the header for why that's exactly what we want here.
    IntSize innerOffset = objectRect.location() - visibleRect.location();
    m_innerWindow->setGeometry(innerOffset.width(), innerOffset.height(), objectRect.width(), objectRect.height());
}

void X11EmbedWidgetQt::setParent(ScrollView* scrollView)
{
    Widget::setParent(scrollView);

    if (scrollView && !m_innerWindow)
        createPlatformWindows();

    updateGeometry();
}

void X11EmbedWidgetQt::setFrameRect(const IntRect& rect)
{
    if (rect != frameRect())
        Widget::setFrameRect(rect);
    updateGeometry();
}

void X11EmbedWidgetQt::frameRectsChanged()
{
    // Called (instead of setFrameRect(), which only fires when the contents-relative frame rect
    // itself changes) when this element's screen position moves for a reason that doesn't touch
    // that rect - chiefly scrolling an ancestor. updateGeometry() re-does the contentsToWindow()
    // conversion, which is scroll-offset-dependent, so this is the actual fix for that case.
    updateGeometry();
}

void X11EmbedWidgetQt::show()
{
    setSelfVisible(true);
    updateGeometry();
}

void X11EmbedWidgetQt::hide()
{
    setSelfVisible(false);
    updateGeometry();
}

void X11EmbedWidgetQt::setParentVisible(bool visible)
{
    Widget::setParentVisible(visible);
    updateGeometry();
}

void X11EmbedWidgetQt::paint(GraphicsContext& context, const IntRect&)
{
    // We normally have no pixels of our own to draw - the reparented content shows through
    // m_innerWindow directly - but RenderWidget::paintContents() calls this on every
    // PaintPhaseForeground pass regardless of widget type, which makes it a reliable, frequent
    // safety net: re-derive our geometry here too, not just from setFrameRect()/
    // frameRectsChanged(). Those only fire when *something WebCore notices changed* triggers them,
    // and an ancestor frame (e.g. an iframe a framing manager repositions asynchronously, after
    // this widget already existed) can move without ever reaching us that way, leaving the window
    // visibly stuck until some unrelated event happens to trigger a recompute. A paint of this
    // element's own region, by contrast, reliably happens shortly after any such move actually
    // takes visible effect.
    updateGeometry();

    if (context.paintingDisabled())
        return;

    // The one exception to "no pixels of our own": while a SnapshotScope is alive (see the header),
    // this paint's destination isn't the live on-screen surface at all - a QImage or QPdfWriter,
    // behind HIPE_OP_TAKE_SNAPSHOT - so there is no X server compositing our native window into the
    // result, and without our own pixels the element would just be a hole.
    //
    // This was originally self-detected per-call from the destination QPainter's device
    // (QInternal::Image/::Printer) rather than a flag threaded in from the snapshot call site, to
    // keep the whole feature local to the widget. Confirmed live that doesn't work: for a docked
    // client's own frame (i.e. reached through at least one nested FrameView, exactly the real
    // hipexwm/periscope case this exists for), paint() is correctly reached during the snapshot,
    // but context.platformContext()->device()->devType() came back QInternal::UnknownDevice rather
    // than the real top-level target's type - Qt/WebKit's nested-frame paint dispatch doesn't
    // reliably propagate it that deep. A flag held around the whole top-level
    // renderContentsForSnapshot() call doesn't depend on that propagation at all.
    if (s_depth <= 0)
        return;

    paintCapturedContent(context);
}

void X11EmbedWidgetQt::paintCapturedContent(GraphicsContext& context)
{
    if (!m_innerWindow)
        return;

    Display* display = compositeDisplay();
    if (!compositeExtensionAvailable(display))
        return;

    // hipecore is never told the XID of whatever a client reparented into m_innerWindow (see the
    // class comment) - find it the same way any compositor would, by asking the server what
    // m_innerWindow's current children are. This is only reached for the infrequent capture case,
    // not on every paint, so a round-trip here is fine.
    Window root, parentUnused;
    Window* children = nullptr;
    unsigned int childCount = 0;
    if (!XQueryTree(display, static_cast<Window>(m_xid), &root, &parentUnused, &children, &childCount) || !childCount) {
        if (children)
            XFree(children);
        return;
    }

    // XQueryTree documents its result as bottom-to-top stacking order - children[0] is only the
    // right pick when there's a single reparented child. hipexwm's transient-dialog case puts two
    // windows here: a decoration/chrome sibling it creates and maps first (bottommost), then the
    // real dialog reparented in afterward and explicitly raised above it (see parentinghelper.c's
    // adoptWindow()) - exactly what m_innerWindow normally shows through live on screen. Scan from
    // the top down and grab the first mapped one, so this matches that live rendering instead of
    // always capturing whatever happens to be at the bottom of the stack.
    Window target = None;
    for (unsigned int i = childCount; i-- > 0; ) {
        XWindowAttributes candidateAttributes;
        if (XGetWindowAttributes(display, children[i], &candidateAttributes) && candidateAttributes.map_state == IsViewable) {
            target = children[i];
            break;
        }
    }
    XFree(children);
    if (target == None)
        return;

    XWindowAttributes attributes;
    if (!XGetWindowAttributes(display, target, &attributes) || attributes.map_state != IsViewable)
        return;

    Pixmap pixmap = XCompositeNameWindowPixmap(display, target);
    if (!pixmap)
        return;

    XImage* image = XGetImage(display, pixmap, 0, 0, attributes.width, attributes.height, AllPlanes, ZPixmap);
    XFreePixmap(display, pixmap);
    if (!image)
        return;

    // Only handle the common case directly - a 32-bit-per-pixel visual, which is what Qt itself
    // creates m_innerWindow with and so what any client that resizes itself to match typically ends
    // up rendering at too. Anything else is left as a hole, same as before this feature existed,
    // rather than writing a general depth/mask conversion nothing here exercises.
    if (image->bits_per_pixel == 32) {
        QImage snapshot(reinterpret_cast<uchar*>(image->data), image->width, image->height,
            image->bytes_per_line, QImage::Format_RGB32);
        // Format_RGB32 has no real alpha channel - X11 leaves that byte's bits meaningless
        // (typically zeroed) - but drawImage() blending it directly onto an
        // ARGB32_Premultiplied destination doesn't reliably treat it as opaque: confirmed live,
        // the destination's alpha channel came back straight from that meaningless byte instead,
        // leaving the captured content almost fully transparent (a black background with faint
        // content once flattened) in the PNG snapshot path - see
        // QWebFrameAdapter::renderContentsForSnapshot()'s ARGB32_Premultiplied canvas. Converting
        // to Format_ARGB32_Premultiplied explicitly first forces real alpha=0xff, which
        // drawImage() then blends correctly. The PDF/SVG snapshot paths render to opaque devices
        // with no destination alpha channel of their own, so this was invisible there.
        QImage opaque = snapshot.copy().convertToFormat(QImage::Format_ARGB32_Premultiplied);
        // RenderWidget::paintContents() only translates the context by (this widget's on-page
        // position minus frameRect()'s own location) before calling paint() - which is zero
        // whenever frameRect() is already kept in sync with layout (the normal case), meaning the
        // context here is generally NOT pre-translated to this widget's own origin at all. Every
        // other Widget::paint() override draws at its own frameRect(), not at a local (0,0) -
        // confirmed live: with two separate embed targets on the page, this drew both at device
        // (0,0), so the later one fully clobbered the earlier one instead of each landing at its
        // own position (e.g. a transient dialog's own embed vs. its owner's).
        context.platformContext()->drawImage(QRect(frameRect().x(), frameRect().y(), frameRect().width(), frameRect().height()), opaque);
    }

    XDestroyImage(image);
}

}
