/*  Copyright (c) 2025 Daniel Kos
    Copyright (C) 2025-2026 General Development Systems

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of this Software library.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.
*/


#pragma once

#include "EventListener.h"
#include <wtf/Ref.h>
#include <wtf/RefPtr.h> 
#include <functional>

namespace WebCore {

class HipeCoreEventListener : public EventListener {
public:
    using Callback = std::function<void(Event*)>;
    
    static Ref<HipeCoreEventListener> create(Callback callback, bool isPreventer = false) {
        return adoptRef(*new HipeCoreEventListener(callback, isPreventer));
    }

    // A preventer only cancels default actions (QWebElement::setDefaultPrevention()); it doesn't
    // report events, so it doesn't count as a request for them.
    bool isPreventer() const { return m_isPreventer; }

    virtual void handleEvent(ScriptExecutionContext*, Event* event) override {
        if (m_callback)
            m_callback(event);
    }

    // Same-object identity only (m_callback can't be compared by value, and nothing
    // currently needs to remove one specific listener by value anyway -- QWebElement::
    // cancelEvent() detaches by enumerating an element's actual registered listeners,
    // not by constructing a comparison key like this).
    virtual bool operator==(const EventListener& other) override {
        return this == &other;
    }

private:
    HipeCoreEventListener(Callback callback, bool isPreventer)
        : EventListener(CPPEventListenerType)
        , m_callback(callback)
        , m_isPreventer(isPreventer)
    {}

    Callback m_callback;
    bool m_isPreventer;
};

} // namespace WebCore
