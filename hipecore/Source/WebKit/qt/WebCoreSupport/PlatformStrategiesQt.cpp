/*
 * Copyright (C) 2007 Staikos Computing Services Inc. <info@staikos.net>
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 INdT - Instituto Nokia de Tecnologia
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformStrategiesQt.h"

#include "WebResourceLoadScheduler.h"

#include <BlobRegistryImpl.h>
#include <IntSize.h>
#include <NotImplemented.h>
#include <Page.h>
#include <PageGroup.h>
#include <QCoreApplication>
#include <QLocale>
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>

using namespace WebCore;

void PlatformStrategiesQt::initialize()
{
    static NeverDestroyed<PlatformStrategiesQt> platformStrategies;
    Q_UNUSED(platformStrategies);
}

PlatformStrategiesQt::PlatformStrategiesQt()
{
    setPlatformStrategies(this);
}


LoaderStrategy* PlatformStrategiesQt::createLoaderStrategy()
{
    return new WebResourceLoadScheduler;
}

PasteboardStrategy* PlatformStrategiesQt::createPasteboardStrategy()
{
    return 0;
}

PluginStrategy* PlatformStrategiesQt::createPluginStrategy()
{
    return this;
}

void PlatformStrategiesQt::refreshPlugins()
{
    // hipecore has no plugins.
}

void PlatformStrategiesQt::getPluginInfo(const WebCore::Page*, Vector<WebCore::PluginInfo>&)
{
    // hipecore has no plugins: the list stays empty.
}

void PlatformStrategiesQt::getWebVisiblePluginInfo(const Page* page, Vector<PluginInfo>& outPlugins)
{
    getPluginInfo(page, outPlugins);
}

BlobRegistry* PlatformStrategiesQt::createBlobRegistry()
{
    return new BlobRegistryImpl;
}
