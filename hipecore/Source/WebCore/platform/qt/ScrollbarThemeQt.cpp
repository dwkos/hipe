/*
 * Copyright (C) 2007, 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2007 Staikos Computing Services Inc. <info@staikos.net>
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

#include "config.h"
#include "ScrollbarThemeQt.h"

#include "GraphicsContext.h"
#include "Scrollbar.h"

namespace WebCore {

static const int cScrollbarThickness = 13;
static const int cThumbInset = 2;

// The QStyle-backed ScrollbarThemeQStyle has been removed. WebCore falls back to
// the plain base ScrollbarTheme unless a native theme is supplied here; that base
// reports a thickness of 0, which makes native (non-CSS) scrollbars invisible and
// unclickable. Supply a minimal flat theme instead.
ScrollbarTheme& ScrollbarTheme::nativeTheme()
{
    static ScrollbarThemeQt theme;
    return theme;
}

ScrollbarThemeQt::~ScrollbarThemeQt()
{
}

int ScrollbarThemeQt::scrollbarThickness(ScrollbarControlSize controlSize)
{
    return controlSize == SmallScrollbar ? cScrollbarThickness - 3 : cScrollbarThickness;
}

bool ScrollbarThemeQt::hasThumb(Scrollbar& scrollbar)
{
    return thumbLength(scrollbar) > 0;
}

IntRect ScrollbarThemeQt::backButtonRect(Scrollbar&, ScrollbarPart, bool)
{
    return IntRect();
}

IntRect ScrollbarThemeQt::forwardButtonRect(Scrollbar&, ScrollbarPart, bool)
{
    return IntRect();
}

IntRect ScrollbarThemeQt::trackRect(Scrollbar& scrollbar, bool)
{
    // No stepper buttons: the track is the whole scrollbar.
    return scrollbar.frameRect();
}

void ScrollbarThemeQt::paintTrackBackground(GraphicsContext& context, Scrollbar&, const IntRect& rect)
{
    context.fillRect(FloatRect(rect), Color(0, 0, 0, 26));
}

void ScrollbarThemeQt::paintThumb(GraphicsContext& context, Scrollbar& scrollbar, const IntRect& rect)
{
    IntRect thumb(rect);
    if (scrollbar.orientation() == HorizontalScrollbar)
        thumb.inflateY(-cThumbInset);
    else
        thumb.inflateX(-cThumbInset);
    context.fillRect(FloatRect(thumb), Color(0, 0, 0, scrollbar.enabled() ? 110 : 60));
}

void ScrollbarThemeQt::paintScrollCorner(ScrollView*, GraphicsContext& context, const IntRect& rect)
{
    context.fillRect(FloatRect(rect), Color(0, 0, 0, 26));
}

}
