/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2025-2026 General Development Systems
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScriptSourceCode_h
#define ScriptSourceCode_h

#include "CachedResourceHandle.h"
#include "CachedScript.h"
#include "URL.h"
#include <wtf/text/TextPosition.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

// hipecore: script never executes, so this no longer wraps a JSC::SourceCode /
// JSC::SourceProvider. It just carries the source text (for the CSP inline-script
// check in ScriptElement::executeScript) and, for external scripts, a handle to the
// CachedScript so the load/error events still fire. The URL and TextPosition ctor
// arguments are accepted for call-site compatibility but not stored.
class ScriptSourceCode {
public:
    ScriptSourceCode(const String& source, const URL& = URL(), const TextPosition& = TextPosition::minimumPosition())
        : m_source(source)
    {
    }

    explicit ScriptSourceCode(CachedScript* cachedScript)
        : m_source(cachedScript->script().toString())
        , m_cachedScript(cachedScript)
    {
    }

    bool isEmpty() const { return m_source.isEmpty(); }

    StringView source() const { return m_source; }

    CachedScript* cachedScript() const { return m_cachedScript.get(); }

private:
    String m_source;
    CachedResourceHandle<CachedScript> m_cachedScript;
};

} // namespace WebCore

#endif // ScriptSourceCode_h
