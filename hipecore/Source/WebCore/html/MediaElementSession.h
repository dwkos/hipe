/*
 * Copyright (C) 2014-2015 Apple Inc. All rights reserved.
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

#ifndef MediaElementSession_h
#define MediaElementSession_h

#if ENABLE(VIDEO)

#include "MediaPlayer.h"
#include "PlatformMediaSession.h"
#include "Timer.h"

namespace WebCore {

class Document;
class HTMLMediaElement;
class SourceBuffer;

class MediaElementSession final : public PlatformMediaSession {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit MediaElementSession(PlatformMediaSessionClient&);
    virtual ~MediaElementSession() { }

    void registerWithDocument(Document&);
    void unregisterWithDocument(Document&);

    bool playbackPermitted(const HTMLMediaElement&) const;
    bool dataLoadingPermitted(const HTMLMediaElement&) const;
    bool fullscreenPermitted(const HTMLMediaElement&) const;
    bool pageAllowsDataLoading(const HTMLMediaElement&) const;
    bool pageAllowsPlaybackAfterResuming(const HTMLMediaElement&) const;

    bool requiresFullscreenForVideoPlayback(const HTMLMediaElement&) const;
    WEBCORE_EXPORT bool allowsPictureInPicture(const HTMLMediaElement&) const;
    MediaPlayer::Preload effectivePreloadForElement(const HTMLMediaElement&) const;
    bool allowsAutomaticMediaDataLoading(const HTMLMediaElement&) const;

    void mediaEngineUpdated(const HTMLMediaElement&);

    // Restrictions to modify default behaviors.
    enum BehaviorRestrictionFlags {
        NoRestrictions = 0,
        RequireUserGestureForLoad = 1 << 0,
        RequireUserGestureForRateChange = 1 << 1,
        RequireUserGestureForFullscreen = 1 << 2,
        RequirePageConsentToLoadMedia = 1 << 3,
        RequirePageConsentToResumeMedia = 1 << 4,
        RequireUserGestureForAudioRateChange = 1 << 5,
        MetadataPreloadingNotPermitted = 1 << 9,
        AutoPreloadingNotPermitted = 1 << 10,
        InvisibleAutoplayNotPermitted = 1 << 11,
    };
    typedef unsigned BehaviorRestrictions;

    WEBCORE_EXPORT BehaviorRestrictions behaviorRestrictions() const { return m_restrictions; }
    WEBCORE_EXPORT void addBehaviorRestriction(BehaviorRestrictions);
    WEBCORE_EXPORT void removeBehaviorRestriction(BehaviorRestrictions);
    bool hasBehaviorRestriction(BehaviorRestrictions restriction) const { return restriction & m_restrictions; }

#if ENABLE(MEDIA_SOURCE)
    size_t maximumMediaSourceBufferSize(const SourceBuffer&) const;
#endif

private:
    BehaviorRestrictions m_restrictions;
};

}

#endif // MediaElementSession_h

#endif // ENABLE(VIDEO)
