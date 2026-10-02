/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
#include "History.h"

#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HistoryController.h"
#include "HistoryItem.h"
#include "MainFrame.h"
#include "Page.h"
#include "SecurityOrigin.h"
#include <wtf/MainThread.h>

namespace WebCore {

History::History(Frame* frame)
    : DOMWindowProperty(frame)
{
}

unsigned History::length() const
{
    return 0;
}

void History::back()
{
    //go(-1);  //no-op
}

void History::back(ScriptExecutionContext* context)
{
    //go(context, -1);  //no-op
}

void History::forward()
{
    //go(1);  //no-op
}

void History::forward(ScriptExecutionContext* context)
{
    //go(context, 1);
}

//navigates back or forward in the browser history.
void History::go(int distance)
{
    //no-op
}

void History::go(ScriptExecutionContext* context, int distance)
{
    //no-op
}


} // namespace WebCore
