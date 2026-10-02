/*
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2026 General Development Systems
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

#ifndef ScrollbarThemeQt_h
#define ScrollbarThemeQt_h

#include "ScrollbarThemeComposite.h"

namespace WebCore {

// hipecore: a small, self-contained flat scrollbar theme -- track + thumb, no
// stepper buttons, fixed neutral colours, zero platform-toolkit dependency. It
// replaces the removed QStyle-backed ScrollbarThemeQStyle. Note that whenever page
// CSS supplies any `::-webkit-scrollbar` rule (as hipe's own themes do), WebCore
// uses RenderScrollbar instead and this theme is not consulted at all -- so this
// mainly matters for un-themed embedders of the QWebView/QGraphicsWebView API.
class ScrollbarThemeQt final : public ScrollbarThemeComposite {
public:
    ~ScrollbarThemeQt() override;

    int scrollbarThickness(ScrollbarControlSize = RegularScrollbar) override;

    bool hasButtons(Scrollbar&) override { return false; }
    bool hasThumb(Scrollbar&) override;

    IntRect backButtonRect(Scrollbar&, ScrollbarPart, bool painting = false) override;
    IntRect forwardButtonRect(Scrollbar&, ScrollbarPart, bool painting = false) override;
    IntRect trackRect(Scrollbar&, bool painting = false) override;

    void paintTrackBackground(GraphicsContext&, Scrollbar&, const IntRect&) override;
    void paintThumb(GraphicsContext&, Scrollbar&, const IntRect&) override;
    void paintScrollCorner(ScrollView*, GraphicsContext&, const IntRect&) override;
};

}

#endif // ScrollbarThemeQt_h
