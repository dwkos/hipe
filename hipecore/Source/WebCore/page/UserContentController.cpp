/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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
#include "UserContentController.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "ExtensionStyleSheets.h"
#include "MainFrame.h"
#include "Page.h"
#include "ResourceLoadInfo.h"
#include "UserStyleSheet.h"

#if ENABLE(CONTENT_EXTENSIONS)
#include "ContentExtensionCompiler.h"
#include "ContentExtensionsBackend.h"
#endif

namespace WebCore {

Ref<UserContentController> UserContentController::create()
{
    return adoptRef(*new UserContentController);
}

UserContentController::UserContentController()
{
}

UserContentController::~UserContentController()
{
}

void UserContentController::addPage(Page& page)
{
    ASSERT(!m_pages.contains(&page));
    m_pages.add(&page);
}

void UserContentController::removePage(Page& page)
{
    ASSERT(m_pages.contains(&page));
    m_pages.remove(&page);
}

void UserContentController::addUserStyleSheet(std::unique_ptr<UserStyleSheet> userStyleSheet, UserStyleInjectionTime injectionTime)
{
    if (!m_userStyleSheets)
        m_userStyleSheets = std::make_unique<UserStyleSheetVector>();

    m_userStyleSheets->append(WTFMove(userStyleSheet));

    if (injectionTime == InjectInExistingDocuments)
        invalidateInjectedStyleSheetCacheInAllFrames();
}

void UserContentController::removeUserStyleSheet(const URL& url)
{
    if (!m_userStyleSheets)
        return;

    bool sheetsChanged = false;
    for (int i = m_userStyleSheets->size() - 1; i >= 0; --i) {
        if (m_userStyleSheets->at(i)->url() == url) {
            m_userStyleSheets->remove(i);
            sheetsChanged = true;
        }
    }

    if (!sheetsChanged)
        return;

    if (m_userStyleSheets->isEmpty())
        m_userStyleSheets = nullptr;

    invalidateInjectedStyleSheetCacheInAllFrames();
}

void UserContentController::removeUserStyleSheets()
{
    if (!m_userStyleSheets)
        return;

    m_userStyleSheets = nullptr;
    invalidateInjectedStyleSheetCacheInAllFrames();
}

#if ENABLE(CONTENT_EXTENSIONS)
void UserContentController::addUserContentExtension(const String& name, RefPtr<ContentExtensions::CompiledContentExtension> contentExtension)
{
    if (!m_contentExtensionBackend)
        m_contentExtensionBackend = std::make_unique<ContentExtensions::ContentExtensionsBackend>();
    
    m_contentExtensionBackend->addContentExtension(name, contentExtension);
}

void UserContentController::removeUserContentExtension(const String& name)
{
    if (!m_contentExtensionBackend)
        return;

    m_contentExtensionBackend->removeContentExtension(name);
}

void UserContentController::removeAllUserContentExtensions()
{
    if (!m_contentExtensionBackend)
        return;

    m_contentExtensionBackend->removeAllContentExtensions();
}

static bool contentExtensionsEnabled(const DocumentLoader& documentLoader)
{
    if (auto frame = documentLoader.frame()) {
        if (frame->isMainFrame())
            return documentLoader.userContentExtensionsEnabled();
        if (auto mainDocumentLoader = frame->mainFrame().loader().documentLoader())
            return mainDocumentLoader->userContentExtensionsEnabled();
    }

    return true;
}
    
ContentExtensions::BlockedStatus UserContentController::processContentExtensionRulesForLoad(ResourceRequest& request, ResourceType resourceType, DocumentLoader& initiatingDocumentLoader)
{
    if (!m_contentExtensionBackend)
        return ContentExtensions::BlockedStatus::NotBlocked;

    if (!contentExtensionsEnabled(initiatingDocumentLoader))
        return ContentExtensions::BlockedStatus::NotBlocked;

    return m_contentExtensionBackend->processContentExtensionRulesForLoad(request, resourceType, initiatingDocumentLoader);
}

Vector<ContentExtensions::Action> UserContentController::actionsForResourceLoad(const ResourceLoadInfo& resourceLoadInfo, DocumentLoader& initiatingDocumentLoader)
{
    if (!m_contentExtensionBackend)
        return Vector<ContentExtensions::Action>();
    
    if (!contentExtensionsEnabled(initiatingDocumentLoader))
        return Vector<ContentExtensions::Action>();

    return m_contentExtensionBackend->actionsForResourceLoad(resourceLoadInfo);
}
#endif

void UserContentController::removeAllUserContent()
{
    if (m_userStyleSheets) {
        m_userStyleSheets = nullptr;
        invalidateInjectedStyleSheetCacheInAllFrames();
    }
}

void UserContentController::invalidateInjectedStyleSheetCacheInAllFrames()
{
    for (auto& page : m_pages) {
        for (Frame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
            frame->document()->extensionStyleSheets().invalidateInjectedStyleSheetCache();
            frame->document()->styleResolverChanged(DeferRecalcStyle);
        }
    }
}

} // namespace WebCore
