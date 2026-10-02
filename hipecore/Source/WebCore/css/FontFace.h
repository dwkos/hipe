/*
 * Copyright (C) 2007, 2008, 2016 Apple Inc. All rights reserved.
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

#ifndef FontFace_h
#define FontFace_h

// hipecore: JS bindings (FontFace.idl, JSFontFaceCustom.cpp) removed 2026-08-22. This class
// is kept as a real, plain-C++ interface - not JS glue over CSSFontFace, but a usable class
// in its own right - for a possible future hipe-driven dynamic font-loading API (see
// HIPE_OP_ADD_FONT). The former Promise-based "loaded" notification (a JS-only concept with
// no C++ caller to serve) was dropped rather than reimplemented; stateChanged() is a no-op
// stub for the same reason - there's currently no C++ consumer to notify. font-stretch was
// already unsupported before this change (CSSFontFace has no representation for it at all).

#include "CSSFontFace.h"
#include "CSSPropertyNames.h"
#include "ExceptionCode.h"
#include <wtf/Optional.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSFontFace;
class CSSValue;
class ScriptExecutionContext;

struct FontFaceDescriptors {
    Optional<String> style;
    Optional<String> weight;
    Optional<String> stretch;
    Optional<String> unicodeRange;
    Optional<String> variant;
    Optional<String> featureSettings;
};

class FontFace final : public RefCounted<FontFace>, public CSSFontFace::Client {
public:
    static RefPtr<FontFace> create(ScriptExecutionContext&, const String& family, const String& source, const FontFaceDescriptors&, ExceptionCode&);
    virtual ~FontFace();

    void setFamily(const String&, ExceptionCode&);
    void setStyle(const String&, ExceptionCode&);
    void setWeight(const String&, ExceptionCode&);
    void setStretch(const String&, ExceptionCode&);
    void setUnicodeRange(const String&, ExceptionCode&);
    void setVariant(const String&, ExceptionCode&);
    void setFeatureSettings(const String&, ExceptionCode&);

    String family() const;
    String style() const;
    String weight() const;
    String stretch() const;
    String unicodeRange() const;
    String variant() const;
    String featureSettings() const;
    String status() const;

    void load();

    CSSFontFace& backing() { return m_backing; }

    static RefPtr<CSSValue> parseString(const String&, CSSPropertyID);

private:
    explicit FontFace(CSSFontSelector&);

    virtual void stateChanged(CSSFontFace&, CSSFontFace::Status oldState, CSSFontFace::Status newState) override;

    Ref<CSSFontFace> m_backing;
};

}

#endif
