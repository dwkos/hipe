/*
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS
 * IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// HipeCore has no network backend: the real QNetworkAccessManager-backed
// ResourceHandle implementation (formerly ResourceHandleQt.cpp /
// QNetworkReplyHandler.{h,cpp}) has been removed outright.
//
// A handful of small, per-port hooks declared in shared WebCore headers
// (ResourceHandle.h, SynchronousLoaderClient.h, CredentialStorage.h,
// ResourceResponse.h under platform/network/qt/) still require *some*
// PLATFORM(QT) definition to exist, because other still-present code paths
// call through the generic, cross-port entry points that own them:
//
//  - WebKit/WebCoreSupport/WebResourceLoadScheduler.cpp calls
//    ResourceHandle::loadResourceSynchronously()/create() unconditionally
//    (it backs FrameLoader::loadResourceSynchronously()).
//  - ResourceLoader.cpp's now-dead network fallback also references
//    ResourceHandle::create().
//
// (XSLTProcessorLibxslt.cpp's docLoaderFunc() used to reach
// loadResourceSynchronously() too, for <xsl:import>/document() loads, but no
// longer does - it now fails XSLT_LOAD_DOCUMENT immediately instead of routing
// through this always-failing stub.)
//
// All real (non data:/about:) loads are already rejected earlier, in
// ResourceLoader::start() ("External network access disabled in HipeCore"),
// so none of the definitions below are expected to run in practice. They
// exist solely so the link succeeds, and they fail closed - consistent with
// "no network access at all" - rather than attempting any I/O.

#include "config.h"
#include "ResourceHandle.h"
#include "ResourceHandleInternal.h"

#include "AuthenticationChallenge.h"
#include "Credential.h"
#include "CredentialStorage.h"
#include "HTTPHeaderNames.h"
#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "ResourceError.h"
#include "ResourceResponse.h"
#include "SynchronousLoaderClient.h"

#include <QMimeDatabase>

namespace WebCore {

ResourceHandleInternal::~ResourceHandleInternal()
{
}

ResourceHandle::~ResourceHandle()
{
    // start() below never succeeds, so there is never an in-flight job to tear down here.
}

bool ResourceHandle::start()
{
    // Fail closed: there is no network backend to hand this request to.
    return false;
}

void ResourceHandle::cancel()
{
    // d->m_job doesn't exist (there is never an in-flight job to cancel).
}

void ResourceHandle::continueWillSendRequest(const ResourceRequest&)
{
    // Only reachable for an async job that start() never creates.
    ASSERT_NOT_REACHED();
}

void ResourceHandle::continueDidReceiveResponse()
{
    // Only reachable for an async job that start() never creates.
    ASSERT_NOT_REACHED();
}

void ResourceHandle::platformLoadResourceSynchronously(NetworkingContext*, const ResourceRequest& request, StoredCredentials, ResourceError& error, ResourceResponse&, Vector<char>&)
{
    error = ResourceError(errorDomainWebKitInternal, 0, request.url(), ASCIILiteral("Network access disabled in HipeCore"));
}

void ResourceHandle::platformSetDefersLoading(bool)
{
    // There is never an in-flight job to defer.
}

// SynchronousLoaderClient's cross-port implementation (SynchronousLoaderClient.cpp)
// calls these two platform hooks; they were already trivial notImplemented()
// stubs on the Qt port before this removal.
void SynchronousLoaderClient::didReceiveAuthenticationChallenge(ResourceHandle*, const AuthenticationChallenge&)
{
    notImplemented();
}

ResourceError SynchronousLoaderClient::platformBadResponseError()
{
    notImplemented();
    return ResourceError();
}

// CredentialStorage has no persistent backing store on the Qt port.
Credential CredentialStorage::getFromPersistentStorage(const ProtectionSpace&)
{
    return Credential();
}

// Pure string/URL utility - not part of the networking backend, just used to
// pick a sensible filename when a loaded resource is saved to disk.
String ResourceResponse::platformSuggestedFilename() const
{
    // FIXME: Move to base class
    String contentDisposition(httpHeaderField(HTTPHeaderName::ContentDisposition));
    String suggestedFilename = filenameFromHTTPContentDisposition(String::fromUTF8WithLatin1Fallback(contentDisposition.characters8(), contentDisposition.length()));

    if (!suggestedFilename.isEmpty())
        return suggestedFilename;

    Vector<String> extensions = MIMETypeRegistry::getExtensionsForMIMEType(mimeType());
    if (extensions.isEmpty())
        return url().lastPathComponent();

    // If the suffix doesn't match the MIME type, correct the suffix.
    QString filename = url().lastPathComponent();
    const String suffix = QMimeDatabase().suffixForFileName(filename);
    if (!extensions.contains(suffix)) {
        filename.chop(suffix.length());
        filename += MIMETypeRegistry::getPreferredExtensionForMIMEType(mimeType());
    }
    return filename;
}

} // namespace WebCore
