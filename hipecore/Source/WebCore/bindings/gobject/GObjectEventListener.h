/*
 *  Copyright (C) 2010, 2011 Igalia S.L.
 *  Copyright (C) 2025-2026 General Development Systems
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

// hipecore note: kept as a design reference, not part of any build (the GTK port and its
// gobject codegen are gone from this fork — see CodeGeneratorGObject.pm's removal). This class
// is WebKitGTK's non-JS EventListener bridge: the same role as our own
// WebCore::HipeCoreEventListener (Source/WebKit/qt/Api/hipecoreeventlistener.h), which
// QWebElement::requestEvent() uses to attach C++ callbacks to DOM events.
//
// The pattern worth copying: gobjectDestroyedCallback()/gobjectDestroyed() below use
// g_object_weak_ref() on m_target so that if the *owner* of the callback is destroyed before the
// DOM node is, this listener detaches itself (coreTarget->removeEventListener(...)) instead of
// leaving a dangling closure that could fire later. HipeCoreEventListener has no equivalent today:
// QWebElement::requestEvent() lambdas can capture a raw hiped-side pointer, the resulting listener
// is never returned to the caller, and QWebElement exposes no removeEventListener() — so nothing
// currently stops a callback from outliving whatever object it captured. If that ever turns out to
// be a real hazard (i.e. a hiped-side object can be destroyed before the DOM element it's attached
// to), this weak-ref-and-auto-detach shape is the fix to model, adapted to Qt/hiped object lifetime
// (e.g. a QObject::destroyed() connection instead of a GObject weak ref).

#ifndef GObjectEventListener_h
#define GObjectEventListener_h

#include "EventListener.h"
#include "EventTarget.h"
#include <wtf/RefPtr.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

typedef struct _GObject GObject;
typedef struct _GClosure GClosure;

namespace WebCore {

class GObjectEventListener : public EventListener {
public:

    static bool addEventListener(GObject* target, EventTarget* coreTarget, const char* domEventName, GClosure* handler, bool useCapture)
    {
        RefPtr<GObjectEventListener> listener(adoptRef(new GObjectEventListener(target, coreTarget, domEventName, handler, useCapture)));
        return coreTarget->addEventListener(domEventName, listener.release(), useCapture);
    }

    static bool removeEventListener(GObject* target, EventTarget* coreTarget, const char* domEventName, GClosure* handler, bool useCapture)
    {
        GObjectEventListener key(target, coreTarget, domEventName, handler, useCapture);
        return coreTarget->removeEventListener(domEventName, &key, useCapture);
    }

    static void gobjectDestroyedCallback(GObjectEventListener* listener, GObject*)
    {
        listener->gobjectDestroyed();
    }

    static const GObjectEventListener* cast(const EventListener* listener)
    {
        return listener->type() == GObjectEventListenerType
            ? static_cast<const GObjectEventListener*>(listener)
            : 0;
    }

    virtual bool operator==(const EventListener& other);

private:
    GObjectEventListener(GObject*, EventTarget*, const char* domEventName, GClosure*, bool capture);
    ~GObjectEventListener();
    void gobjectDestroyed();

    virtual void handleEvent(ScriptExecutionContext*, Event*);

    GObject* m_target;
    // We do not need to keep a reference to the m_coreTarget, because
    // we only use it when the GObject and thus the m_coreTarget object is alive.
    EventTarget* m_coreTarget;
    CString m_domEventName;
    GRefPtr<GClosure> m_handler;
    bool m_capture;
};
} // namespace WebCore

#endif
