/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if ENABLE(VIDEO)

#include "MediaElementSession.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "HTMLVideoElement.h"
#include "Logging.h"
#include "Page.h"
#include "PlatformMediaSessionManager.h"
#include "SourceBuffer.h"
#include "UserGestureIndicator.h"

namespace WebCore {

#if !LOG_DISABLED
static String restrictionName(MediaElementSession::BehaviorRestrictions restriction)
{
    StringBuilder restrictionBuilder;
#define CASE(restrictionType) \
    if (restriction & MediaElementSession::restrictionType) { \
        if (!restrictionBuilder.isEmpty()) \
            restrictionBuilder.append(", "); \
        restrictionBuilder.append(#restrictionType); \
    } \

    CASE(NoRestrictions);
    CASE(RequireUserGestureForLoad);
    CASE(RequireUserGestureForRateChange);
    CASE(RequireUserGestureForAudioRateChange);
    CASE(RequireUserGestureForFullscreen);
    CASE(RequirePageConsentToLoadMedia);
    CASE(RequirePageConsentToResumeMedia);
    CASE(RequireUserGestureForAudioRateChange);
    CASE(InvisibleAutoplayNotPermitted);

    return restrictionBuilder.toString();
}
#endif

static bool pageExplicitlyAllowsElementToAutoplayInline(const HTMLMediaElement& element)
{
    Document& document = element.document();
    Page* page = document.page();
    return document.isMediaDocument() && !document.ownerElement() && page && page->allowsMediaDocumentInlinePlayback();
}

MediaElementSession::MediaElementSession(PlatformMediaSessionClient& client)
    : PlatformMediaSession(client)
    , m_restrictions(NoRestrictions)
{
}

void MediaElementSession::registerWithDocument(Document& document)
{
    UNUSED_PARAM(document);
}

void MediaElementSession::unregisterWithDocument(Document& document)
{
    UNUSED_PARAM(document);
}

void MediaElementSession::addBehaviorRestriction(BehaviorRestrictions restriction)
{
    LOG(Media, "MediaElementSession::addBehaviorRestriction - adding %s", restrictionName(restriction).utf8().data());
    m_restrictions |= restriction;
}

void MediaElementSession::removeBehaviorRestriction(BehaviorRestrictions restriction)
{
    LOG(Media, "MediaElementSession::removeBehaviorRestriction - removing %s", restrictionName(restriction).utf8().data());
    m_restrictions &= ~restriction;
}

bool MediaElementSession::playbackPermitted(const HTMLMediaElement& element) const
{
    if (pageExplicitlyAllowsElementToAutoplayInline(element))
        return true;

    if (m_restrictions & RequireUserGestureForRateChange && !UserGestureIndicator::processingUserGestureForMedia()) {
        LOG(Media, "MediaElementSession::playbackPermitted - returning FALSE");
        return false;
    }

    if (m_restrictions & RequireUserGestureForAudioRateChange && element.hasAudio() && !UserGestureIndicator::processingUserGestureForMedia()) {
        LOG(Media, "MediaElementSession::playbackPermitted - returning FALSE");
        return false;
    }

    return true;
}

bool MediaElementSession::dataLoadingPermitted(const HTMLMediaElement&) const
{
    if (m_restrictions & RequireUserGestureForLoad && !UserGestureIndicator::processingUserGestureForMedia()) {
        LOG(Media, "MediaElementSession::dataLoadingPermitted - returning FALSE");
        return false;
    }

    return true;
}

bool MediaElementSession::fullscreenPermitted(const HTMLMediaElement&) const
{
    if (m_restrictions & RequireUserGestureForFullscreen && !UserGestureIndicator::processingUserGestureForMedia()) {
        LOG(Media, "MediaElementSession::fullscreenPermitted - returning FALSE");
        return false;
    }

    return true;
}

bool MediaElementSession::pageAllowsDataLoading(const HTMLMediaElement& element) const
{
    Page* page = element.document().page();
    if (m_restrictions & RequirePageConsentToLoadMedia && page && !page->canStartMedia()) {
        LOG(Media, "MediaElementSession::pageAllowsDataLoading - returning FALSE");
        return false;
    }

    return true;
}

bool MediaElementSession::pageAllowsPlaybackAfterResuming(const HTMLMediaElement& element) const
{
    Page* page = element.document().page();
    if (m_restrictions & RequirePageConsentToResumeMedia && page && !page->canStartMedia()) {
        LOG(Media, "MediaElementSession::pageAllowsPlaybackAfterResuming - returning FALSE");
        return false;
    }

    return true;
}

MediaPlayer::Preload MediaElementSession::effectivePreloadForElement(const HTMLMediaElement& element) const
{
    MediaPlayer::Preload preload = element.preloadValue();

    if (pageExplicitlyAllowsElementToAutoplayInline(element))
        return preload;

    if (m_restrictions & MetadataPreloadingNotPermitted)
        return MediaPlayer::None;

    if (m_restrictions & AutoPreloadingNotPermitted) {
        if (preload > MediaPlayer::MetaData)
            return MediaPlayer::MetaData;
    }

    return preload;
}

bool MediaElementSession::requiresFullscreenForVideoPlayback(const HTMLMediaElement& element) const
{
    if (pageExplicitlyAllowsElementToAutoplayInline(element))
        return false;

    Settings* settings = element.document().settings();
    if (!settings || !settings->allowsInlineMediaPlayback())
        return true;

    return settings->inlineMediaPlaybackRequiresPlaysInlineAttribute() && !element.fastHasAttribute(HTMLNames::webkit_playsinlineAttr);
}

bool MediaElementSession::allowsAutomaticMediaDataLoading(const HTMLMediaElement& element) const
{
    if (pageExplicitlyAllowsElementToAutoplayInline(element))
        return true;

    Settings* settings = element.document().settings();
    if (settings && settings->mediaDataLoadsAutomatically())
        return true;

    return false;
}

void MediaElementSession::mediaEngineUpdated(const HTMLMediaElement& element)
{
    LOG(Media, "MediaElementSession::mediaEngineUpdated");
    UNUSED_PARAM(element);
}

bool MediaElementSession::allowsPictureInPicture(const HTMLMediaElement& element) const
{
    Settings* settings = element.document().settings();
    return settings && settings->allowsPictureInPictureMediaPlayback() && !element.webkitCurrentPlaybackTargetIsWireless();
}

#if ENABLE(MEDIA_SOURCE)
const unsigned fiveMinutesOf1080PVideo = 290 * 1024 * 1024; // 290 MB is approximately 5 minutes of 8Mbps (1080p) content.
const unsigned fiveMinutesStereoAudio = 14 * 1024 * 1024; // 14 MB is approximately 5 minutes of 384kbps content.

size_t MediaElementSession::maximumMediaSourceBufferSize(const SourceBuffer& buffer) const
{
    // A good quality 1080p video uses 8,000 kbps and stereo audio uses 384 kbps, so assume 95% for video and 5% for audio.
    const float bufferBudgetPercentageForVideo = .95;
    const float bufferBudgetPercentageForAudio = .05;

    size_t maximum;
    Settings* settings = buffer.document().settings();
    if (settings)
        maximum = settings->maximumSourceBufferSize();
    else
        maximum = fiveMinutesOf1080PVideo + fiveMinutesStereoAudio;

    // Allow a SourceBuffer to buffer as though it is audio-only even if it doesn't have any active tracks (yet).
    size_t bufferSize = static_cast<size_t>(maximum * bufferBudgetPercentageForAudio);
    if (buffer.hasVideo())
        bufferSize += static_cast<size_t>(maximum * bufferBudgetPercentageForVideo);

    // FIXME: we might want to modify this algorithm to:
    // - decrease the maximum size for background tabs
    // - decrease the maximum size allowed for inactive elements when a process has more than one
    //   element, eg. so a page with many elements which are played one at a time doesn't keep
    //   everything buffered after an element has finished playing.

    return bufferSize;
}
#endif

}

#endif // ENABLE(VIDEO)
