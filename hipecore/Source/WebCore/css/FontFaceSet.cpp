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

#include "config.h"
#include "FontFaceSet.h"

#include "Document.h"
#include "FontFace.h"

namespace WebCore {

// hipecore: the DOM CSS Font Loading API is gone (no JS). FontFaceSet.idl /
// JSFontFaceSetCustom.cpp were removed 2026-08-22; the JS-Promise-based load()/ready()
// machinery (the DeferredWrapper, the global-object plumbing, m_pendingPromises) was
// stripped 2026-08-28. FontFaceSet is kept as a plain C++ font-set container backed by
// CSSFontFaceSet - mirrors the FontFace de-JSC-ification, in case Hipe grows programmatic
// font management. load() is now fire-and-forget; a completion callback would slot in here.

FontFaceSet::FontFaceSet(Document& document, const Vector<RefPtr<FontFace>>& initialFaces)
    : ActiveDOMObject(&document)
    , m_backing(*this)
{
    for (auto& face : initialFaces)
        add(face.get());
}

FontFaceSet::~FontFaceSet()
{
}

FontFaceSet::Iterator::Iterator(FontFaceSet& set)
    : m_target(set)
{
}

bool FontFaceSet::Iterator::next(RefPtr<FontFace>& key, RefPtr<FontFace>& value)
{
    if (m_index == m_target->size())
        return true;
    key = m_target->m_backing[m_index++].wrapper();
    value = key;
    return false;
}

bool FontFaceSet::has(FontFace* face) const
{
    if (!face)
        return false;
    return m_backing.hasFace(face->backing());
}

size_t FontFaceSet::size() const
{
    return m_backing.faceCount();
}

FontFaceSet& FontFaceSet::add(FontFace* face)
{
    if (face && !m_backing.hasFace(face->backing()))
        m_backing.add(face->backing());
    return *this;
}

bool FontFaceSet::remove(FontFace* face)
{
    if (!face)
        return false;

    bool result = m_backing.hasFace(face->backing());
    if (result)
        m_backing.remove(face->backing());
    return result;
}

void FontFaceSet::clear()
{
    while (m_backing.faceCount())
        m_backing.remove(m_backing[0]);
}

void FontFaceSet::load(const String& font, const String& text, ExceptionCode& ec)
{
    auto matchingFaces = m_backing.matchingFaces(font, text, ec);
    if (ec)
        return;

    // hipecore: fire-and-forget - kick off loading of every matching face. There is no
    // caller waiting on completion (the Promise surface is gone).
    for (auto& face : matchingFaces)
        face.get().load();
}

bool FontFaceSet::check(const String& family, const String& text, ExceptionCode& ec)
{
    return m_backing.check(family, text, ec);
}

String FontFaceSet::status() const
{
    switch (m_backing.status()) {
    case CSSFontFaceSet::Status::Loading:
        return String("loading", String::ConstructFromLiteral);
    case CSSFontFaceSet::Status::Loaded:
        return String("loaded", String::ConstructFromLiteral);
    }
    ASSERT_NOT_REACHED();
    return String("loaded", String::ConstructFromLiteral);
}

bool FontFaceSet::canSuspendForDocumentSuspension() const
{
    return m_backing.status() == CSSFontFaceSet::Status::Loaded;
}

void FontFaceSet::startedLoading()
{
    // FIXME: Fire a "loading" event asynchronously.
}

void FontFaceSet::completedLoading()
{
    // FIXME: Fire a "loadingdone" and possibly a "loadingerror" event asynchronously.
}

void FontFaceSet::faceFinished(CSSFontFace&, CSSFontFace::Status newStatus)
{
    // hipecore: nothing to notify - the per-face pending-promise routing is gone.
    ASSERT_UNUSED(newStatus, newStatus == CSSFontFace::Status::Success || newStatus == CSSFontFace::Status::Failure);
}

}
