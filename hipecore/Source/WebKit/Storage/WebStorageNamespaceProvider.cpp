/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "WebStorageNamespaceProvider.h"

#include <WebCore/SecurityOrigin.h>
#include <WebCore/StorageArea.h>
#include <WebCore/StorageNamespace.h>

using namespace WebCore;

namespace {

class NullStorageArea final : public StorageArea {
    unsigned length() override { return 0; }
    String key(unsigned) override { return String(); }
    String item(const String&) override { return String(); }
    void setItem(Frame*, const String&, const String&, bool&) override { }
    void removeItem(Frame*, const String&) override { }
    void clear(Frame*) override { }
    bool contains(const String&) override { return false; }
    bool canAccessStorage(Frame*) override { return false; }
    StorageType storageType() const override { return LocalStorage; }
    size_t memoryBytesUsedByCache() override { return 0; }
    SecurityOrigin& securityOrigin() override { return SecurityOrigin::createUnique(); }
};

class NullStorageNamespace final : public StorageNamespace {
    RefPtr<StorageArea> storageArea(RefPtr<SecurityOrigin>&&) override { return adoptRef(new NullStorageArea); }
    RefPtr<StorageNamespace> copy(Page*) override { return adoptRef(new NullStorageNamespace); }
};

} // namespace

RefPtr<WebStorageNamespaceProvider> WebStorageNamespaceProvider::create()
{
    return adoptRef(new WebStorageNamespaceProvider);
}

WebStorageNamespaceProvider::WebStorageNamespaceProvider()
{
}

WebStorageNamespaceProvider::~WebStorageNamespaceProvider()
{
}

RefPtr<StorageNamespace> WebStorageNamespaceProvider::createSessionStorageNamespace(Page&, unsigned)
{
    return adoptRef(new NullStorageNamespace);
}

RefPtr<StorageNamespace> WebStorageNamespaceProvider::createLocalStorageNamespace(unsigned)
{
    return adoptRef(new NullStorageNamespace);
}

RefPtr<StorageNamespace> WebStorageNamespaceProvider::createTransientLocalStorageNamespace(SecurityOrigin&, unsigned)
{
    return adoptRef(new NullStorageNamespace);
}
