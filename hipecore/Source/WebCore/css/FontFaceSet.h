/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef FontFaceSet_h
#define FontFaceSet_h

#include "ActiveDOMObject.h"
#include "CSSFontFaceSet.h"
#include "EventTarget.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class FontFace;

class FontFaceSet final : public RefCounted<FontFaceSet>, public CSSFontFaceSetClient, public EventTargetWithInlineData, public ActiveDOMObject {
public:
    // hipecore: no JS - the DOM CSS Font Loading API is gone. FontFaceSet is kept as a
    // C++-usable font-set container (mirrors the FontFace de-JSC-ification) in case Hipe
    // grows programmatic font management; the Promise-based load()/ready() surface has been
    // replaced with fire-and-forget. A future completion callback would slot into load().
    static Ref<FontFaceSet> create(Document& document, const Vector<RefPtr<FontFace>>& initialFaces)
    {
        Ref<FontFaceSet> result = adoptRef(*new FontFaceSet(document, initialFaces));
        result->suspendIfNeeded();
        return result;
    }
    virtual ~FontFaceSet();

    bool has(FontFace*) const;
    size_t size() const;
    FontFaceSet& add(FontFace*);
    bool remove(FontFace*);
    void clear();

    void load(const String& font, ExceptionCode& ec) { load(font, String(" ", String::ConstructFromLiteral), ec); }
    void load(const String& font, const String& text, ExceptionCode&);
    bool check(const String& font, ExceptionCode& ec) { return check(font, String(" ", String::ConstructFromLiteral), ec); }
    bool check(const String& font, const String& text, ExceptionCode&);

    String status() const;

    class Iterator {
    public:
        explicit Iterator(FontFaceSet&);

        using Key = RefPtr<FontFace>;
        using Value = RefPtr<FontFace>;

        bool next(Key& nextKey, Value& nextValue);

    private:
        Ref<FontFaceSet> m_target;
        size_t m_index { 0 };
    };
    Iterator createIterator() { return Iterator(*this); }

    using RefCounted<FontFaceSet>::ref;
    using RefCounted<FontFaceSet>::deref;

private:
    FontFaceSet(Document&, const Vector<RefPtr<FontFace>>&);

    // CSSFontFaceSetClient
    virtual void startedLoading() override;
    virtual void completedLoading() override;
    virtual void faceFinished(CSSFontFace&, CSSFontFace::Status) override;

    // ActiveDOMObject
    virtual const char* activeDOMObjectName() const override { return "FontFaceSet"; }
    virtual bool canSuspendForDocumentSuspension() const override;

    // EventTarget
    virtual EventTargetInterface eventTargetInterface() const override { return GenericEventTargetInterfaceType; }
    virtual ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }
    virtual void refEventTarget() override { ref(); }
    virtual void derefEventTarget() override { deref(); }

    CSSFontFaceSet m_backing;
};

}

#endif
