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

#ifndef CacheValidation_h
#define CacheValidation_h

#include "PlatformExportMacros.h"
#include <wtf/Optional.h>

namespace WebCore {

// This header provides core functionality for HTTP caching mechanisms in WebCore.
// It implements validation logic according to RFC 7234 (HTTP Caching).


//*******ALL COMMENTS WERE ADDED BY COPILOT - TAKE THEM WITH A GRAIN OF SALT */


class HTTPHeaderMap;
class ResourceResponse;

// Tracks the caching status of redirect chains, which is crucial for 
// proper handling of cached redirects in navigation and resource loading
struct RedirectChainCacheStatus {
    enum Status {
        NoRedirection,        // No redirect occurred
        NotCachedRedirection, // Redirect occurred but was not cached
        CachedRedirection    // Redirect was cached and may be reusable
    };
    RedirectChainCacheStatus()
        : status(NoRedirection)
        , endOfValidity(std::chrono::system_clock::time_point::max())
    { }
    Status status;
    std::chrono::system_clock::time_point endOfValidity;
};

// Calculates age of a response according to RFC 7234 Section 4.2.3
WEBCORE_EXPORT std::chrono::microseconds computeCurrentAge(const ResourceResponse&, std::chrono::system_clock::time_point responseTimestamp);

// Determines how long a response can be considered fresh without revalidation
// Uses heuristics from RFC 7234 Section 4.2.2
WEBCORE_EXPORT std::chrono::microseconds computeFreshnessLifetimeForHTTPFamily(const ResourceResponse&, std::chrono::system_clock::time_point responseTimestamp);

// Updates response headers after successful revalidation with the server
// Implements behavior specified in RFC 7234 Section 4.3.4
WEBCORE_EXPORT void updateResponseHeadersAfterRevalidation(ResourceResponse&, const ResourceResponse& validatingResponse);

// Maintains the state of cached redirects in a redirect chain
WEBCORE_EXPORT void updateRedirectChainStatus(RedirectChainCacheStatus&, const ResourceResponse&);

enum ReuseExpiredRedirectionOrNot { DoNotReuseExpiredRedirection, ReuseExpiredRedirection };
// Determines if a cached redirect chain can be reused based on its current status
WEBCORE_EXPORT bool redirectChainAllowsReuse(RedirectChainCacheStatus, ReuseExpiredRedirectionOrNot);

// Represents parsed Cache-Control header directives as defined in RFC 7234 Section 5.2
struct CacheControlDirectives {
    Optional<std::chrono::microseconds> maxAge;      // max-age directive value
    Optional<std::chrono::microseconds> maxStale;    // max-stale directive value
    bool noCache { false };       // no-cache directive present
    bool noStore { false };       // no-store directive present
    bool mustRevalidate { false }; // must-revalidate directive present
};
// Parses Cache-Control headers into structured directives for easy handling
WEBCORE_EXPORT CacheControlDirectives parseCacheControlDirectives(const HTTPHeaderMap&);

}

#endif
