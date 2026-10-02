/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile, Inc.
 * Copyright (C) 2025-2026 General Development Systems
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PageConsoleClient_h
#define PageConsoleClient_h

#include "ConsoleTypes.h"
#include <wtf/Forward.h>

namespace WebCore {

class Document;
class Page;

// hipecore: was a JSC::ConsoleClient subclass wrapping Inspector::ConsoleMessage /
// ScriptCallStack. With no JS, all of that (console.log/count/profile/time/... and the
// JS call-stack capture) is gone. What remains is a thin router: engine diagnostics
// (CSS/CSP/MIME warnings) forwarded to ChromeClient::addMessageToConsole. In hipe's
// default config that terminates at a Qt signal with no connections - see the
// bucket5 memory - so this currently produces nothing observable, but the seam is
// kept so hiped could wire it up.
class WEBCORE_EXPORT PageConsoleClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit PageConsoleClient(Page&);
    ~PageConsoleClient();

    static bool shouldPrintExceptions();
    static void setShouldPrintExceptions(bool);

    static void mute();
    static void unmute();

    void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, unsigned columnNumber, unsigned long requestIdentifier = 0);
    void addMessage(MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier = 0, Document* = nullptr);

private:
    Page& m_page;
};

} // namespace WebCore

#endif // PageConsoleClient_h
