/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Gustavo Noronha Silva <gns@gnome.org>
 * Copyright (C) 2009, 2010, 2011, 2012, 2013 Igalia S.L
 * Copyright (C) 2014 Cable Television Laboratories, Inc.
 * Copyright (C) 2025-2026 General Development Systems
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "MediaPlayerPrivateGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerUtilities.h"
#include "URL.h"
#include "MIMETypeRegistry.h"
#include "MediaPlayer.h"
#include "MediaPlayerRequestInstallMissingPluginsCallback.h"
#include "NotImplemented.h"
#include "SecurityOrigin.h"
#include "MediaSampleBuffer.h"
#include "TimeRanges.h"
#include "WebKitBufferSourceGStreamer.h"
#include "WebKitWebSourceGStreamer.h"
#include <gst/gst.h>
#include <gst/pbutils/missing-plugins.h>
#include <limits>
#include <wtf/CurrentTime.h>
#include <wtf/HexNumber.h>
#include <wtf/MediaTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

#if ENABLE(VIDEO_TRACK)
#include "AudioTrackPrivateGStreamer.h"
#include "InbandMetadataTextTrackPrivateGStreamer.h"
#include "InbandTextTrackPrivateGStreamer.h"
#include "TextCombinerGStreamer.h"
#include "TextSinkGStreamer.h"
#include "VideoTrackPrivateGStreamer.h"
#endif

#if ENABLE(VIDEO_TRACK) && USE(GSTREAMER_MPEGTS)
#define GST_USE_UNSTABLE_API
#include <gst/mpegts/mpegts.h>
#undef GST_USE_UNSTABLE_API
#endif
#include <gst/audio/streamvolume.h>

#if ENABLE(MEDIA_SOURCE)
#include "MediaSource.h"
#include "WebKitMediaSourceGStreamer.h"
#endif

#if ENABLE(WEB_AUDIO)
#include "AudioSourceProviderGStreamer.h"
#endif

GST_DEBUG_CATEGORY_EXTERN(webkit_media_player_debug);
#define GST_CAT_DEFAULT webkit_media_player_debug

using namespace std;

namespace WebCore {

void MediaPlayerPrivateGStreamer::setAudioStreamPropertiesCallback(MediaPlayerPrivateGStreamer* player, GObject* object)
{
    player->setAudioStreamProperties(object);
}

void MediaPlayerPrivateGStreamer::setAudioStreamProperties(GObject* object)
{
    if (g_strcmp0(G_OBJECT_TYPE_NAME(object), "GstPulseSink"))
        return;

    const char* role = m_player->client().mediaPlayerIsVideo() ? "video" : "music";
    GstStructure* structure = gst_structure_new("stream-properties", "media.role", G_TYPE_STRING, role, NULL);
    g_object_set(object, "stream-properties", structure, NULL);
    gst_structure_free(structure);
    GUniquePtr<gchar> elementName(gst_element_get_name(GST_ELEMENT(object)));
    LOG_MEDIA_MESSAGE("Set media.role as %s at %s", role, elementName.get());
}

void MediaPlayerPrivateGStreamer::registerMediaEngine(MediaEngineRegistrar registrar)
{
    if (isAvailable())
        registrar([](MediaPlayer* player) { return std::make_unique<MediaPlayerPrivateGStreamer>(player); },
            getSupportedTypes, supportsType, 0, 0, 0, 0);
}

bool initializeGStreamerAndRegisterWebKitElements()
{
    if (!initializeGStreamer())
        return false;

    GRefPtr<GstElementFactory> srcFactory = adoptGRef(gst_element_factory_find("webkitwebsrc"));
    if (!srcFactory) {
        GST_DEBUG_CATEGORY_INIT(webkit_media_player_debug, "webkitmediaplayer", 0, "WebKit media player");
        gst_element_register(0, "webkitwebsrc", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_WEB_SRC);
    }

#if ENABLE(MEDIA_SOURCE)
    GRefPtr<GstElementFactory> WebKitMediaSrcFactory = adoptGRef(gst_element_factory_find("webkitmediasrc"));
    if (!WebKitMediaSrcFactory)
        gst_element_register(0, "webkitmediasrc", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_MEDIA_SRC);
#endif

    GRefPtr<GstElementFactory> WebKitBufferSrcFactory = adoptGRef(gst_element_factory_find("webkitbuffersrc"));
    if (!WebKitBufferSrcFactory)
        gst_element_register(0, "webkitbuffersrc", GST_RANK_PRIMARY + 100, WEBKIT_TYPE_BUFFER_SRC);

    return true;
}

bool MediaPlayerPrivateGStreamer::isAvailable()
{
    if (!initializeGStreamerAndRegisterWebKitElements())
        return false;

    GRefPtr<GstElementFactory> factory = adoptGRef(gst_element_factory_find("playbin"));
    return factory;
}

MediaPlayerPrivateGStreamer::MediaPlayerPrivateGStreamer(MediaPlayer* player)
    : MediaPlayerPrivateGStreamerBase(player)
    , m_weakPtrFactory(this)
    , m_source(0)
    , m_seekTime(0)
    , m_changingRate(false)
    , m_isEndReached(false)
    , m_isStreaming(false)
    , m_mediaLocations(0)
    , m_mediaLocationCurrentIndex(0)
    , m_resetPipeline(false)
    , m_paused(true)
    , m_playbackRatePause(false)
    , m_seeking(false)
    , m_seekIsPending(false)
    , m_timeOfOverlappingSeek(-1)
    , m_canFallBackToLastFinishedSeekPosition(false)
    , m_buffering(false)
    , m_playbackRate(1)
    , m_lastPlaybackRate(1)
    , m_errorOccured(false)
    , m_mediaDuration(0)
    , m_downloadFinished(false)
    , m_fillTimer(*this, &MediaPlayerPrivateGStreamer::fillTimerFired)
    , m_maxTimeLoaded(0)
    , m_bufferingPercentage(0)
    , m_preload(player->preload())
    , m_delayingLoad(false)
    , m_mediaDurationKnown(true)
    , m_maxTimeLoadedAtLastDidLoadingProgress(0)
    , m_volumeAndMuteInitialized(false)
    , m_hasVideo(false)
    , m_hasAudio(false)
    , m_readyTimerHandler(RunLoop::main(), this, &MediaPlayerPrivateGStreamer::readyTimerFired)
    , m_totalBytes(0)
    , m_preservesPitch(false)
#if ENABLE(WEB_AUDIO)
    , m_audioSourceProvider(std::make_unique<AudioSourceProviderGStreamer>())
#endif
    , m_requestedState(GST_STATE_VOID_PENDING)
{
    // FIXME: Use Qt timer priority
#if USE(GLIB) && !PLATFORM(QT)
    m_readyTimerHandler.setPriority(G_PRIORITY_DEFAULT_IDLE);
#endif
}

MediaPlayerPrivateGStreamer::~MediaPlayerPrivateGStreamer()
{
#if ENABLE(VIDEO_TRACK)
    for (size_t i = 0; i < m_audioTracks.size(); ++i)
        m_audioTracks[i]->disconnect();

    for (size_t i = 0; i < m_textTracks.size(); ++i)
        m_textTracks[i]->disconnect();

    for (size_t i = 0; i < m_videoTracks.size(); ++i)
        m_videoTracks[i]->disconnect();
#endif
    if (m_fillTimer.isActive())
        m_fillTimer.stop();

    if (m_mediaLocations) {
        gst_structure_free(m_mediaLocations);
        m_mediaLocations = 0;
    }

    if (m_autoAudioSink)
        g_signal_handlers_disconnect_by_func(G_OBJECT(m_autoAudioSink.get()),
            reinterpret_cast<gpointer>(setAudioStreamPropertiesCallback), this);

    m_readyTimerHandler.stop();
    if (m_missingPluginsCallback) {
        m_missingPluginsCallback->invalidate();
        m_missingPluginsCallback = nullptr;
    }

    if (m_pipeline) {
        GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
        ASSERT(bus);
        gst_bus_set_sync_handler(bus.get(), nullptr, nullptr, nullptr);
        g_signal_handlers_disconnect_matched(m_pipeline.get(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        gst_element_set_state(m_pipeline.get(), GST_STATE_NULL);
    }

    if (m_videoSink) {
        GRefPtr<GstPad> videoSinkPad = adoptGRef(gst_element_get_static_pad(m_videoSink.get(), "sink"));
        g_signal_handlers_disconnect_by_func(videoSinkPad.get(), reinterpret_cast<gpointer>(videoSinkCapsChangedCallback), this);
    }
}

void MediaPlayerPrivateGStreamer::load(const String& urlString)
{
    if (!initializeGStreamerAndRegisterWebKitElements())
        return;

    URL url(URL(), urlString);
    if (url.isBlankURL())
        return;

    // Clean out everything after file:// url path.
    String cleanURL(urlString);
    if (url.isLocalFile())
        cleanURL = cleanURL.substring(0, url.pathEnd());

    if (!m_pipeline)
        createGSTPlayBin();

    ASSERT(m_pipeline);

    m_url = URL(URL(), cleanURL);
    g_object_set(m_pipeline.get(), "uri", cleanURL.utf8().data(), nullptr);

    INFO_MEDIA_MESSAGE("Load %s", cleanURL.utf8().data());

    if (m_preload == MediaPlayer::None) {
        LOG_MEDIA_MESSAGE("Delaying load.");
        m_delayingLoad = true;
    }

    // Reset network and ready states. Those will be set properly once
    // the pipeline pre-rolled.
    m_networkState = MediaPlayer::Loading;
    m_player->networkStateChanged();
    m_readyState = MediaPlayer::HaveNothing;
    m_player->readyStateChanged();
    m_volumeAndMuteInitialized = false;

    // Also reset all sample-buffer pause-for-buffering state (see sampleBufferStalled()) --
    // nothing else did. A fresh load (including a progressive reload reusing the same
    // HTMLMediaElement/MediaPlayerPrivateGStreamer) previously inherited whatever m_buffering/
    // m_bufferingHoldPosition the *previous* load happened to leave behind, live-confirmed as a
    // real bug: playbackPosition() reported a stale multi-second holdover position from an
    // earlier session immediately on a brand new load, well before this load had done any
    // buffering evaluation of its own.
    m_buffering = false;
    m_bufferingPercentage = 0;
    m_bufferingHoldPosition = 0;
    m_downloadFinished = false;
    m_lastBufferingResumeTime = 0;
    // Matches the ctor's own default (m_paused(true)) -- a fresh load starts paused until a real
    // play() call says otherwise; HTMLMediaElement's own spec-mandated load-reset should call
    // pause() anyway, but reset directly here too rather than relying on that ordering.
    m_userRequestedPlay = false;

    if (!m_delayingLoad)
        commitLoad();
}

#if ENABLE(MEDIA_SOURCE)
void MediaPlayerPrivateGStreamer::load(const String& url, MediaSourcePrivateClient* mediaSource)
{
    String mediasourceUri = String::format("mediasource%s", url.utf8().data());
    m_mediaSource = mediaSource;
    load(mediasourceUri);
}
#endif

#if ENABLE(MEDIA_STREAM)
void MediaPlayerPrivateGStreamer::load(MediaStreamPrivate&)
{
    notImplemented();
}
#endif

void MediaPlayerPrivateGStreamer::load(const String& url, RefPtr<MediaSampleBuffer>&& sampleBuffer, long long expectedTotalSize)
{
    // Unlike the MediaSource path above, no scheme-prefixing trick is needed: WebKitBufferSrc
    // registers the "hipebuffer" scheme directly (see WebKitBufferSourceGStreamer.h), so the
    // dummy URL HTMLMediaElement::beginBinaryMediaData() builds is used exactly as given.
    m_sampleBuffer = WTFMove(sampleBuffer);
    m_expectedTotalSize = expectedTotalSize;
    m_lastLoopEndTime = 0;
    m_rapidLoopEnds = 0;
    load(url);
}

void MediaPlayerPrivateGStreamer::commitLoad()
{
    ASSERT(!m_delayingLoad);
    LOG_MEDIA_MESSAGE("Committing load.");

    // GStreamer needs to have the pipeline set to a paused state to
    // start providing anything useful.
    changePipelineState(GST_STATE_PAUSED);

    setDownloadBuffering();
    updateStates();
}

float MediaPlayerPrivateGStreamer::playbackPosition() const
{
    if (m_isEndReached) {
        // Position queries on a null pipeline return 0. If we're at
        // the end of the stream the pipeline is null but we want to
        // report either the seek time or the duration because this is
        // what the Media element spec expects us to do.
        if (m_seeking)
            return m_seekTime;
        if (m_mediaDuration)
            return m_mediaDuration;
        return 0;
    }

    // While internally paused for buffering a sample-buffer-backed load (see
    // sampleBufferStalled()), don't trust a live position query. Live-confirmed, with a real
    // side-by-side render-vs-reported comparison, not inferred: changePipelineState(PAUSED)
    // doesn't reliably stop this pipeline shape's clock -- the raw GST_QUERY_POSITION can keep
    // honestly advancing (it's not a caching/extrapolation artifact; there is none on this
    // backend, see currentMediaTime()/maximumDurationToCacheMediaTime()) even while nothing new
    // is actually being decoded or rendered, because the underlying clock genuinely never
    // stopped despite a real pause having been requested. Report the position captured at the
    // moment buffering started instead (m_bufferingHoldPosition, set in sampleBufferStalled()),
    // held steady until playback has genuinely resumed. This is not the reverted
    // display-only-freeze approach from project_hipe_stall_detection_fix -- that one faked a
    // pause with no real pipeline change behind it; this holds the *reported* number steady on
    // top of a real, already-in-flight GST_STATE_PAUSED request, specifically because that
    // request has now been confirmed to not reliably stop the clock on its own.
    if (m_buffering && isSampleBufferBacked())
        return m_bufferingHoldPosition;

    // Position is only available if no async state change is going on and the state is either paused or playing.
    gint64 position = GST_CLOCK_TIME_NONE;
    GstQuery* query= gst_query_new_position(GST_FORMAT_TIME);
    if (gst_element_query(m_pipeline.get(), query))
        gst_query_parse_position(query, 0, &position);

    float result = 0.0f;
    if (static_cast<GstClockTime>(position) != GST_CLOCK_TIME_NONE)
        result = static_cast<double>(position) / GST_SECOND;
    else if (m_canFallBackToLastFinishedSeekPosition)
        result = m_seekTime;

    LOG_MEDIA_MESSAGE("Position %" GST_TIME_FORMAT, GST_TIME_ARGS(position));

    gst_query_unref(query);

    return result;
}

void MediaPlayerPrivateGStreamer::readyTimerFired()
{
    changePipelineState(GST_STATE_NULL);
}

bool MediaPlayerPrivateGStreamer::changePipelineState(GstState newState)
{
    ASSERT(m_pipeline);

    GstState currentState;
    GstState pending;

    gst_element_get_state(m_pipeline.get(), &currentState, &pending, 0);
    // Only a no-op if we're STABLY already at newState (no transition in flight, pending ==
    // GST_STATE_VOID_PENDING) or already heading there (pending == newState). `currentState ==
    // newState` alone is NOT sufficient when a *different* transition is actively pending --
    // that means the pipeline is already on its way OUT of currentState toward something else, not
    // sitting there. The original version of this guard treated `currentState == newState` as
    // "already there" unconditionally, live-confirmed as a real bug via GST_DEBUG trace: a
    // buffering-pause request (PAUSED) still in flight (current=PLAYING, pending=PAUSED -- GStreamer
    // hadn't actually finished leaving PLAYING yet) caused a same-tick resume request (PLAYING) to be
    // silently rejected as "already there", permanently abandoning the resume with no retry --
    // exactly the "buffering completes but playback stays paused forever" symptom reported live.
    // Retargeting a still-in-flight async transition via a second gst_element_set_state() call is
    // standard, safe GStreamer usage (it simply updates the target state), not a race to avoid.
    if (pending == newState || (currentState == newState && pending == GST_STATE_VOID_PENDING)) {
        LOG_MEDIA_MESSAGE("Rejected state change to %s from %s with %s pending", gst_element_state_get_name(newState),
            gst_element_state_get_name(currentState), gst_element_state_get_name(pending));
        return true;
    }

    LOG_MEDIA_MESSAGE("Changing state change to %s from %s with %s pending", gst_element_state_get_name(newState),
        gst_element_state_get_name(currentState), gst_element_state_get_name(pending));

    GstStateChangeReturn setStateResult = gst_element_set_state(m_pipeline.get(), newState);
    GstState pausedOrPlaying = newState == GST_STATE_PLAYING ? GST_STATE_PAUSED : GST_STATE_PLAYING;
    if (currentState != pausedOrPlaying && setStateResult == GST_STATE_CHANGE_FAILURE) {
        return false;
    }

    // Create a timer when entering the READY state so that we can free resources
    // if we stay for too long on READY.
    // Also lets remove the timer if we request a state change for any state other than READY.
    // See also https://bugs.webkit.org/show_bug.cgi?id=117354
    if (newState == GST_STATE_READY && !m_readyTimerHandler.isActive()) {
        // Max interval in seconds to stay in the READY state on manual
        // state change requests.
        static const double readyStateTimerDelay = 60;
        m_readyTimerHandler.startOneShot(readyStateTimerDelay);
    } else if (newState != GST_STATE_READY)
        m_readyTimerHandler.stop();

    return true;
}

void MediaPlayerPrivateGStreamer::prepareToPlay()
{
    m_preload = MediaPlayer::Auto;
    if (m_delayingLoad) {
        m_delayingLoad = false;
        commitLoad();
    }
}

void MediaPlayerPrivateGStreamer::play()
{
    // See m_userRequestedPlay's own comment (MediaPlayerPrivateGStreamer.h) -- genuine user intent,
    // set here regardless of playbackRate/pipeline state, deliberately never touched by
    // updateStates()'s m_paused state-mirroring.
    m_userRequestedPlay = true;

    if (!m_playbackRate) {
        m_playbackRatePause = true;
        return;
    }

    // A real, explicit play() call deliberately bypasses shouldPlayGivenBufferedAhead()'s high
    // watermark entirely -- that hysteresis governs only this class's OWN automatic
    // stall/recovery decisions (sampleBufferStalled()/sampleBufferDataAvailable()), never a genuine
    // user action. The user asking to play is always honored immediately with whatever data is
    // currently available, even a razor-thin margin -- exactly like the abstract, always-acceptable
    // seek/play/pause input side this class's own layering principle already establishes for
    // seeking into unbuffered territory (see project_hipe_pull_to_push_redesign in memory).
    //
    // But if m_buffering was still true when this happens (e.g. the user paused mid-hold, then
    // un-paused before the internal auto-resume ever cleared it), that internal bookkeeping must be
    // reconciled here, not left stale: playbackPosition() would otherwise keep returning the old,
    // frozen m_bufferingHoldPosition even though the pipeline is now genuinely running again, and --
    // more seriously -- sampleBufferStalled()'s own `if (m_buffering) return;` re-entrancy guard
    // would silently refuse to detect the *next* real stall, believing one was already correctly
    // in progress when actually nothing is holding the pipeline back any more. Clearing it here lets
    // the ordinary low-watermark stall detection pick back up fresh and genuinely catch the moment
    // this explicit, thin-margin playback actually runs out of real data -- "stop there" instead of
    // running silently past the edge with no state tracking it.
    if (m_buffering) {
        LOG_MEDIA_MESSAGE("[Buffering] Explicit play() while still internally buffering -- reconciling state, letting normal stall detection resume fresh");
        m_buffering = false;
    }

    if (changePipelineState(GST_STATE_PLAYING)) {
        m_isEndReached = false;
        m_delayingLoad = false;
        m_preload = MediaPlayer::Auto;
        setDownloadBuffering();
        LOG_MEDIA_MESSAGE("Play");
    } else {
        loadingFailed(MediaPlayer::Empty);
    }
}

void MediaPlayerPrivateGStreamer::pause()
{
    // See m_userRequestedPlay's own comment (MediaPlayerPrivateGStreamer.h) -- set unconditionally,
    // before the early-return below, since it's DOM-level intent, not pipeline state.
    m_userRequestedPlay = false;

    m_playbackRatePause = false;
    GstState currentState, pendingState;
    gst_element_get_state(m_pipeline.get(), &currentState, &pendingState, 0);
    if (currentState < GST_STATE_PAUSED && pendingState <= GST_STATE_PAUSED)
        return;

    if (changePipelineState(GST_STATE_PAUSED))
        INFO_MEDIA_MESSAGE("Pause");
    else
        loadingFailed(MediaPlayer::Empty);
}

float MediaPlayerPrivateGStreamer::duration() const
{
    if (!m_pipeline)
        return 0.0f;

    if (m_errorOccured)
        return 0.0f;

    // Media duration query failed already, don't attempt new useless queries.
    if (!m_mediaDurationKnown)
        return numeric_limits<float>::infinity();

    if (m_mediaDuration)
        return m_mediaDuration;

    GstFormat timeFormat = GST_FORMAT_TIME;
    gint64 timeLength = 0;

    bool failure = !gst_element_query_duration(m_pipeline.get(), timeFormat, &timeLength) || static_cast<guint64>(timeLength) == GST_CLOCK_TIME_NONE;
    if (failure) {
        LOG_MEDIA_MESSAGE("Time duration query failed for %s", m_url.string().utf8().data());
        return numeric_limits<float>::infinity();
    }

    LOG_MEDIA_MESSAGE("Duration: %" GST_TIME_FORMAT, GST_TIME_ARGS(timeLength));

    m_mediaDuration = static_cast<double>(timeLength) / GST_SECOND;
    return m_mediaDuration;
    // FIXME: handle 3.14.9.5 properly
}

float MediaPlayerPrivateGStreamer::currentTime() const
{
    if (!m_pipeline)
        return 0.0f;

    if (m_errorOccured)
        return 0.0f;

    if (m_seeking)
        return m_seekTime;

    // Workaround for
    // https://bugzilla.gnome.org/show_bug.cgi?id=639941 In GStreamer
    // 0.10.35 basesink reports wrong duration in case of EOS and
    // negative playback rate. There's no upstream accepted patch for
    // this bug yet, hence this temporary workaround.
    if (m_isEndReached && m_playbackRate < 0)
        return 0.0f;

    return playbackPosition();
}

void MediaPlayerPrivateGStreamer::seek(float time)
{
    if (!m_pipeline)
        return;

    if (m_errorOccured)
        return;

    // A looping element only ever seeks to 0, so a seek anywhere else is someone acting on the
    // media by hand: whatever end-of-stream follows soon after is not a runaway loop.
    if (time)
        m_rapidLoopEnds = 0;

    INFO_MEDIA_MESSAGE("[Seek] seek attempt to %f secs", time);

    // Avoid useless seeking.
    if (time == currentTime())
        return;

    if (isLiveStream())
        return;

    GstClockTime clockTime = toGstClockTime(time);
    INFO_MEDIA_MESSAGE("[Seek] seeking to %" GST_TIME_FORMAT " (%f)", GST_TIME_ARGS(clockTime), time);

    if (m_seeking) {
        m_timeOfOverlappingSeek = time;
        if (m_seekIsPending) {
            m_seekTime = time;
            return;
        }
    }

    GstState state;
    GstStateChangeReturn getStateResult = gst_element_get_state(m_pipeline.get(), &state, nullptr, 0);
    if (getStateResult == GST_STATE_CHANGE_FAILURE || getStateResult == GST_STATE_CHANGE_NO_PREROLL) {
        LOG_MEDIA_MESSAGE("[Seek] cannot seek, current state change is %s", gst_element_state_change_return_get_name(getStateResult));
        return;
    }
    if (getStateResult == GST_STATE_CHANGE_ASYNC || state < GST_STATE_PAUSED || m_isEndReached) {
        m_seekIsPending = true;
        if (m_isEndReached) {
            LOG_MEDIA_MESSAGE("[Seek] reset pipeline");
            m_resetPipeline = true;
            if (!changePipelineState(GST_STATE_PAUSED))
                loadingFailed(MediaPlayer::Empty);
        }
    } else {
        // We can seek now.
        if (!doSeek(clockTime, m_player->rate(), static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE))) {
            LOG_MEDIA_MESSAGE("[Seek] seeking to %f failed", time);
            seekRefused(time);
            return;
        }
    }

    m_seeking = true;
    m_seekTime = time;
    m_isEndReached = false;
}

void MediaPlayerPrivateGStreamer::seekRefused(float time)
{
    // Reached when gst_element_seek() returns false. The push-mode demuxer for a fragmented mp4
    // does exactly that for every seek (qtdemux: "ignoring seek in push mode in current state"),
    // which for a *looping* element used to be catastrophic: a refused seek left the pipeline at
    // end-of-stream, the element -- whose loop restart is triggered by every end-of-stream we
    // report through didEnd() -- asked for the same seek again, play() re-armed the pipeline, it hit
    // end-of-stream immediately, and so on at ~67 iterations a second, forever, pinning hiped's main
    // thread (and, via the audio sink being toggled every iteration, the desktop's audio stack).
    // Whatever happens here, the caller must not be left waiting or retrying.

    // Seeking to the start is the one seek that can always be honoured without the demuxer's
    // cooperation: restart the pipeline. It is what looping and "restart" both ask for.
    if (!time && restartPipelineFromStart())
        return;

    m_seeking = false;
    m_seekIsPending = false;
    m_restartingFromStart = false;
    m_timeOfOverlappingSeek = -1;

    // An element that loops asks for precisely this seek at every end of stream, so a refusal that
    // cannot be worked around must end playback with an error, not be silently retried.
    if (!time && m_player->client().mediaPlayerIsLooping()) {
        ERROR_MEDIA_MESSAGE("[Seek] cannot restart looping playback from the beginning; ending it with an error instead of retrying");
        loadingFailed(MediaPlayer::DecodeError);
        return;
    }

    // Any other refused seek is simply a seek that did not go anywhere. Complete it where playback
    // actually is, so the element does not stay "seeking" forever waiting for a timeChanged() that
    // nothing would ever send. Deferred rather than called from here: we may be inside seek() or
    // updateStates(), which timeChanged() re-enters.
    WARN_MEDIA_MESSAGE("[Seek] seek to %f refused by the pipeline; leaving playback where it is", time);
    auto weakThis = createWeakPtr();
    RunLoop::main().dispatch([weakThis] {
        if (weakThis)
            weakThis->timeChanged();
    });
}

bool MediaPlayerPrivateGStreamer::restartPipelineFromStart()
{
    if (!m_pipeline || !isSampleBufferBacked())
        return false;

    INFO_MEDIA_MESSAGE("[Seek] restarting the pipeline from the beginning instead of seeking");

    // Before the state changes below: updateStates() runs for the READY they pass through, and must
    // not throw the (already known, unchanged) duration away -- for a fragmented file the demuxer
    // only learns it incrementally, so re-deriving it can come out short (see didEnd()).
    m_restartingFromStart = true;

    // Downward state changes are synchronous: once this returns, GStreamer's streaming threads --
    // including the source's -- have stopped, so the rewind below cannot race a push.
    if (!changePipelineState(GST_STATE_READY)) {
        m_restartingFromStart = false;
        return false;
    }

    // The element may or may not survive the state cycle (playbin can replace it, in which case
    // sourceChanged() attaches the buffer to the new one, already at offset 0); rewind the old one
    // in case it stays.
    if (m_source && WEBKIT_IS_BUFFER_SRC(m_source.get()))
        webKitBufferSrcRewind(WEBKIT_BUFFER_SRC(m_source.get()));

    if (!changePipelineState(GST_STATE_PAUSED)) {
        // The pipeline is now stuck in READY: nothing sensible is left to fall back to.
        m_restartingFromStart = false;
        m_seeking = false;
        loadingFailed(MediaPlayer::DecodeError);
        return true;
    }

    // Completion is the async state change we just requested (asyncStateChangeDone()), not a
    // pending gst_element_seek().
    m_seekIsPending = false;
    m_seeking = true;
    m_seekTime = 0;
    m_isEndReached = false;
    return true;
}

bool MediaPlayerPrivateGStreamer::doSeek(gint64 position, float rate, GstSeekFlags seekType)
{
    gint64 startTime, endTime;

    // TODO: Should do more than that, need to notify the media source
    // and probably flush the pipeline at least.
    if (isMediaSource())
        return true;

    if (rate > 0) {
        startTime = position;
        endTime = GST_CLOCK_TIME_NONE;
    } else {
        startTime = 0;
        // If we are at beginning of media, start from the end to
        // avoid immediate EOS.
        if (position < 0)
            endTime = static_cast<gint64>(duration() * GST_SECOND);
        else
            endTime = position;
    }

    if (!rate)
        rate = 1.0;

    return gst_element_seek(m_pipeline.get(), rate, GST_FORMAT_TIME, seekType,
        GST_SEEK_TYPE_SET, startTime, GST_SEEK_TYPE_SET, endTime);
}

void MediaPlayerPrivateGStreamer::updatePlaybackRate()
{
    if (!m_changingRate)
        return;

    float currentPosition = static_cast<float>(playbackPosition() * GST_SECOND);
    bool mute = false;

    INFO_MEDIA_MESSAGE("Set Rate to %f", m_playbackRate);

    if (m_playbackRate > 0) {
        // Mute the sound if the playback rate is too extreme and
        // audio pitch is not adjusted.
        mute = (!m_preservesPitch && (m_playbackRate < 0.8 || m_playbackRate > 2));
    } else {
        if (currentPosition == 0.0f)
            currentPosition = -1.0f;
        mute = true;
    }

    INFO_MEDIA_MESSAGE("Need to mute audio?: %d", (int) mute);
    if (doSeek(currentPosition, m_playbackRate, static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH))) {
        g_object_set(m_pipeline.get(), "mute", mute, nullptr);
        m_lastPlaybackRate = m_playbackRate;
    } else {
        m_playbackRate = m_lastPlaybackRate;
        ERROR_MEDIA_MESSAGE("Set rate to %f failed", m_playbackRate);
    }

    if (m_playbackRatePause) {
        GstState state;
        GstState pending;

        gst_element_get_state(m_pipeline.get(), &state, &pending, 0);
        if (state != GST_STATE_PLAYING && pending != GST_STATE_PLAYING)
            changePipelineState(GST_STATE_PLAYING);
        m_playbackRatePause = false;
    }

    m_changingRate = false;
    m_player->rateChanged();
}

bool MediaPlayerPrivateGStreamer::paused() const
{
    if (m_isEndReached) {
        LOG_MEDIA_MESSAGE("Ignoring pause at EOS");
        return true;
    }

    if (m_playbackRatePause)
        return false;

    // While internally paused for buffering (see sampleBufferStalled()), report "not paused" to
    // the caller -- from HTMLMediaElement's perspective playback is still conceptually running,
    // just temporarily starved of data; only this class's own updateStates() should ever resume
    // the pipeline once buffering clears (see feedback_separate_buffering_pause_from_user_pause in
    // memory), never HTMLMediaElement reacting to what it thinks is an unexpected pause. Reporting
    // the raw GST_STATE_PAUSED here instead made HTMLMediaElement::updatePlayState() see "player
    // is paused but should be playing" and call play() again on every readyState-changed
    // notification the buffering cycle produces -- undoing the pause within milliseconds, live-
    // confirmed as a rapid, continuous PAUSED<->PLAYING thrash in GST_DEBUG traces (every ~200-
    // 300ms, never settling) rather than the clean, sustained hold this was meant to produce. A
    // real user-initiated pause during buffering still works correctly despite this: pause() sets
    // GST_STATE_PAUSED regardless, updateStates()'s own switch still tracks the genuine m_paused
    // (a separate member from this method's return value) off the real pipeline state, and its
    // resume gate already checks that before ever un-pausing -- this override only affects what
    // gets reported to the outside caller, not this class's own internal bookkeeping.
    if (m_buffering)
        return false;

    GstState state;
    gst_element_get_state(m_pipeline.get(), &state, nullptr, 0);
    return state == GST_STATE_PAUSED;
}

bool MediaPlayerPrivateGStreamer::seeking() const
{
    return m_seeking;
}

void MediaPlayerPrivateGStreamer::videoChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    player->m_notifier.notify(MainThreadNotification::VideoChanged, [player] { player->notifyPlayerOfVideo(); });
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfVideo()
{
    gint numTracks = 0;
    if (m_pipeline)
        g_object_get(m_pipeline.get(), "n-video", &numTracks, nullptr);

    m_hasVideo = numTracks > 0;

#if ENABLE(VIDEO_TRACK)
    for (gint i = 0; i < numTracks; ++i) {
        GRefPtr<GstPad> pad;
        g_signal_emit_by_name(m_pipeline.get(), "get-video-pad", i, &pad.outPtr(), nullptr);
        ASSERT(pad);

        if (i < static_cast<gint>(m_videoTracks.size())) {
            RefPtr<VideoTrackPrivateGStreamer> existingTrack = m_videoTracks[i];
            existingTrack->setIndex(i);
            if (existingTrack->pad() == pad)
                continue;
        }

        RefPtr<VideoTrackPrivateGStreamer> track = VideoTrackPrivateGStreamer::create(m_pipeline, i, pad);
        m_videoTracks.append(track);
        m_player->addVideoTrack(track.release());
    }

    while (static_cast<gint>(m_videoTracks.size()) > numTracks) {
        RefPtr<VideoTrackPrivateGStreamer> track = m_videoTracks.last();
        track->disconnect();
        m_videoTracks.removeLast();
        m_player->removeVideoTrack(track.release());
    }
#endif

    m_player->client().mediaPlayerEngineUpdated(m_player);
}

void MediaPlayerPrivateGStreamer::videoSinkCapsChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    player->m_notifier.notify(MainThreadNotification::VideoCapsChanged, [player] { player->notifyPlayerOfVideoCaps(); });
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfVideoCaps()
{
    m_videoSize = IntSize();
    m_player->client().mediaPlayerEngineUpdated(m_player);
}

void MediaPlayerPrivateGStreamer::audioChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    player->m_notifier.notify(MainThreadNotification::AudioChanged, [player] { player->notifyPlayerOfAudio(); });
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfAudio()
{
    gint numTracks = 0;
    if (m_pipeline)
        g_object_get(m_pipeline.get(), "n-audio", &numTracks, nullptr);

    m_hasAudio = numTracks > 0;

#if ENABLE(VIDEO_TRACK)
    for (gint i = 0; i < numTracks; ++i) {
        GRefPtr<GstPad> pad;
        g_signal_emit_by_name(m_pipeline.get(), "get-audio-pad", i, &pad.outPtr(), nullptr);
        ASSERT(pad);

        if (i < static_cast<gint>(m_audioTracks.size())) {
            RefPtr<AudioTrackPrivateGStreamer> existingTrack = m_audioTracks[i];
            existingTrack->setIndex(i);
            if (existingTrack->pad() == pad)
                continue;
        }

        RefPtr<AudioTrackPrivateGStreamer> track = AudioTrackPrivateGStreamer::create(m_pipeline, i, pad);
        m_audioTracks.insert(i, track);
        m_player->addAudioTrack(track.release());
    }

    while (static_cast<gint>(m_audioTracks.size()) > numTracks) {
        RefPtr<AudioTrackPrivateGStreamer> track = m_audioTracks.last();
        track->disconnect();
        m_audioTracks.removeLast();
        m_player->removeAudioTrack(track.release());
    }
#endif

    m_player->client().mediaPlayerEngineUpdated(m_player);
}

#if ENABLE(VIDEO_TRACK)
void MediaPlayerPrivateGStreamer::textChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    player->m_notifier.notify(MainThreadNotification::TextChanged, [player] { player->notifyPlayerOfText(); });
}

void MediaPlayerPrivateGStreamer::notifyPlayerOfText()
{
    gint numTracks = 0;
    if (m_pipeline)
        g_object_get(m_pipeline.get(), "n-text", &numTracks, nullptr);

    for (gint i = 0; i < numTracks; ++i) {
        GRefPtr<GstPad> pad;
        g_signal_emit_by_name(m_pipeline.get(), "get-text-pad", i, &pad.outPtr(), nullptr);
        ASSERT(pad);

        if (i < static_cast<gint>(m_textTracks.size())) {
            RefPtr<InbandTextTrackPrivateGStreamer> existingTrack = m_textTracks[i];
            existingTrack->setIndex(i);
            if (existingTrack->pad() == pad)
                continue;
        }

        RefPtr<InbandTextTrackPrivateGStreamer> track = InbandTextTrackPrivateGStreamer::create(i, pad);
        m_textTracks.insert(i, track);
        m_player->addTextTrack(track.release());
    }

    while (static_cast<gint>(m_textTracks.size()) > numTracks) {
        RefPtr<InbandTextTrackPrivateGStreamer> track = m_textTracks.last();
        track->disconnect();
        m_textTracks.removeLast();
        m_player->removeTextTrack(track.release());
    }
}

GstFlowReturn MediaPlayerPrivateGStreamer::newTextSampleCallback(MediaPlayerPrivateGStreamer* player)
{
    player->newTextSample();
    return GST_FLOW_OK;
}

void MediaPlayerPrivateGStreamer::newTextSample()
{
    if (!m_textAppSink)
        return;

    GRefPtr<GstEvent> streamStartEvent = adoptGRef(
        gst_pad_get_sticky_event(m_textAppSinkPad.get(), GST_EVENT_STREAM_START, 0));

    GRefPtr<GstSample> sample;
    g_signal_emit_by_name(m_textAppSink.get(), "pull-sample", &sample.outPtr(), NULL);
    ASSERT(sample);

    if (streamStartEvent) {
        bool found = FALSE;
        const gchar* id;
        gst_event_parse_stream_start(streamStartEvent.get(), &id);
        for (size_t i = 0; i < m_textTracks.size(); ++i) {
            RefPtr<InbandTextTrackPrivateGStreamer> track = m_textTracks[i];
            if (track->streamId() == id) {
                track->handleSample(sample);
                found = true;
                break;
            }
        }
        if (!found)
            WARN_MEDIA_MESSAGE("Got sample with unknown stream ID.");
    } else
        WARN_MEDIA_MESSAGE("Unable to handle sample with no stream start event.");
}
#endif

void MediaPlayerPrivateGStreamer::setRate(float rate)
{
    // Higher rate causes crash.
    rate = clampTo(rate, -20.0, 20.0);

    // Avoid useless playback rate update.
    if (m_playbackRate == rate) {
        // and make sure that upper layers were notified if rate was set

        if (!m_changingRate && m_player->rate() != m_playbackRate)
            m_player->rateChanged();
        return;
    }

    if (isLiveStream()) {
        // notify upper layers that we cannot handle passed rate.
        m_changingRate = false;
        m_player->rateChanged();
        return;
    }

    GstState state;
    GstState pending;

    m_playbackRate = rate;
    m_changingRate = true;

    gst_element_get_state(m_pipeline.get(), &state, &pending, 0);

    if (!rate) {
        m_changingRate = false;
        m_playbackRatePause = true;
        if (state != GST_STATE_PAUSED && pending != GST_STATE_PAUSED)
            changePipelineState(GST_STATE_PAUSED);
        return;
    }

    if ((state != GST_STATE_PLAYING && state != GST_STATE_PAUSED)
        || (pending == GST_STATE_PAUSED))
        return;

    updatePlaybackRate();
}

double MediaPlayerPrivateGStreamer::rate() const
{
    return m_playbackRate;
}

void MediaPlayerPrivateGStreamer::setPreservesPitch(bool preservesPitch)
{
    m_preservesPitch = preservesPitch;
}

std::unique_ptr<PlatformTimeRanges> MediaPlayerPrivateGStreamer::buffered() const
{
    auto timeRanges = std::make_unique<PlatformTimeRanges>();
    if (m_errorOccured || isLiveStream())
        return timeRanges;

    float mediaDuration(duration());
    if (!mediaDuration || std::isinf(mediaDuration))
        return timeRanges;

    // A plain pull-mode appsrc (WebKitBufferSrc) never answers GST_QUERY_BUFFERING -- that
    // machinery is built around GStreamer's network-download elements (queue2, souphttpsrc,
    // etc.), not a source we feed byte-for-byte ourselves. Compute the buffered range directly
    // from bytes delivered so far instead (see sampleBufferLoadedTime()).
    if (isSampleBufferBacked()) {
        float loaded = sampleBufferLoadedTime();
        if (loaded > 0)
            timeRanges->add(MediaTime::zeroTime(), MediaTime::createWithDouble(loaded));
        return timeRanges;
    }

    GstQuery* query = gst_query_new_buffering(GST_FORMAT_PERCENT);

    if (!gst_element_query(m_pipeline.get(), query)) {
        gst_query_unref(query);
        return timeRanges;
    }

    guint numBufferingRanges = gst_query_get_n_buffering_ranges(query);
    for (guint index = 0; index < numBufferingRanges; index++) {
        gint64 rangeStart = 0, rangeStop = 0;
        if (gst_query_parse_nth_buffering_range(query, index, &rangeStart, &rangeStop))
            timeRanges->add(MediaTime::createWithDouble((rangeStart * mediaDuration) / GST_FORMAT_PERCENT_MAX),
                MediaTime::createWithDouble((rangeStop * mediaDuration) / GST_FORMAT_PERCENT_MAX));
    }

    // Fallback to the more general maxTimeLoaded() if no range has
    // been found.
    if (!timeRanges->length())
        if (float loaded = maxTimeLoaded())
            timeRanges->add(MediaTime::zeroTime(), MediaTime::createWithDouble(loaded));

    gst_query_unref(query);

    return timeRanges;
}

void MediaPlayerPrivateGStreamer::handleMessage(GstMessage* message)
{
    GUniqueOutPtr<GError> err;
    GUniqueOutPtr<gchar> debug;
    MediaPlayer::NetworkState error;
    bool issueError = true;
    bool attemptNextLocation = false;
    const GstStructure* structure = gst_message_get_structure(message);
    GstState requestedState, currentState;

    m_canFallBackToLastFinishedSeekPosition = false;

    if (structure) {
        const gchar* messageTypeName = gst_structure_get_name(structure);

        // Redirect messages are sent from elements, like qtdemux, to
        // notify of the new location(s) of the media.
        if (!g_strcmp0(messageTypeName, "redirect")) {
            mediaLocationChanged(message);
            return;
        }
    }

    // We ignore state changes from internal elements. They are forwarded to playbin2 anyway.
    bool messageSourceIsPlaybin = GST_MESSAGE_SRC(message) == reinterpret_cast<GstObject*>(m_pipeline.get());

    LOG_MEDIA_MESSAGE("Message %s received from element %s", GST_MESSAGE_TYPE_NAME(message), GST_MESSAGE_SRC_NAME(message));
    switch (GST_MESSAGE_TYPE(message)) {
    case GST_MESSAGE_ERROR:
        if (m_resetPipeline)
            break;
        if (m_missingPluginsCallback)
            break;
        gst_message_parse_error(message, &err.outPtr(), &debug.outPtr());
        ERROR_MEDIA_MESSAGE("Error %d: %s (url=%s)", err->code, err->message, m_url.string().utf8().data());

        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, "webkit-video.error");

        error = MediaPlayer::Empty;
        if (err->code == GST_STREAM_ERROR_CODEC_NOT_FOUND
            || err->code == GST_STREAM_ERROR_WRONG_TYPE
            || err->code == GST_STREAM_ERROR_FAILED
            || err->code == GST_CORE_ERROR_MISSING_PLUGIN
            || err->code == GST_RESOURCE_ERROR_NOT_FOUND)
            error = MediaPlayer::FormatError;
        else if (err->domain == GST_STREAM_ERROR) {
            // Let the mediaPlayerClient handle the stream error, in
            // this case the HTMLMediaElement will emit a stalled
            // event.
            if (err->code == GST_STREAM_ERROR_TYPE_NOT_FOUND) {
                ERROR_MEDIA_MESSAGE("Decode error, let the Media element emit a stalled event.");
                break;
            }
            error = MediaPlayer::DecodeError;
            attemptNextLocation = true;
        } else if (err->domain == GST_RESOURCE_ERROR)
            error = MediaPlayer::NetworkError;

        if (attemptNextLocation)
            issueError = !loadNextLocation();
        if (issueError)
            loadingFailed(error);
        break;
    case GST_MESSAGE_EOS:
        didEnd();
        break;
    case GST_MESSAGE_ASYNC_DONE:
        if (!messageSourceIsPlaybin || m_delayingLoad)
            break;
        asyncStateChangeDone();
        break;
    case GST_MESSAGE_STATE_CHANGED: {
        if (!messageSourceIsPlaybin || m_delayingLoad)
            break;
        updateStates();

        // Construct a filename for the graphviz dot file output.
        GstState newState;
        gst_message_parse_state_changed(message, &currentState, &newState, 0);
        CString dotFileName = String::format("webkit-video.%s_%s", gst_element_state_get_name(currentState), gst_element_state_get_name(newState)).utf8();
        GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS(GST_BIN(m_pipeline.get()), GST_DEBUG_GRAPH_SHOW_ALL, dotFileName.data());

        break;
    }
    case GST_MESSAGE_BUFFERING:
        processBufferingStats(message);
        break;
    case GST_MESSAGE_DURATION_CHANGED:
        if (messageSourceIsPlaybin)
            durationChanged();
        break;
    case GST_MESSAGE_REQUEST_STATE:
        gst_message_parse_request_state(message, &requestedState);
        gst_element_get_state(m_pipeline.get(), &currentState, nullptr, 250 * GST_NSECOND);
        if (requestedState < currentState) {
            GUniquePtr<gchar> elementName(gst_element_get_name(GST_ELEMENT(message)));
            INFO_MEDIA_MESSAGE("Element %s requested state change to %s", elementName.get(),
                gst_element_state_get_name(requestedState));
            m_requestedState = requestedState;
            if (!changePipelineState(requestedState))
                loadingFailed(MediaPlayer::Empty);
        }
        break;
    case GST_MESSAGE_CLOCK_LOST:
        // This can only happen in PLAYING state and we should just
        // get a new clock by moving back to PAUSED and then to
        // PLAYING again.
        // This can happen if the stream that ends in a sink that
        // provides the current clock disappears, for example if
        // the audio sink provides the clock and the audio stream
        // is disabled. It also happens relatively often with
        // HTTP adaptive streams when switching between different
        // variants of a stream.
        gst_element_set_state(m_pipeline.get(), GST_STATE_PAUSED);
        gst_element_set_state(m_pipeline.get(), GST_STATE_PLAYING);
        break;
    case GST_MESSAGE_LATENCY:
        // Recalculate the latency, we don't need any special handling
        // here other than the GStreamer default.
        // This can happen if the latency of live elements changes, or
        // for one reason or another a new live element is added or
        // removed from the pipeline.
        gst_bin_recalculate_latency(GST_BIN(m_pipeline.get()));
        break;
    case GST_MESSAGE_ELEMENT:
        if (gst_is_missing_plugin_message(message)) {
            if (gst_install_plugins_supported()) {
                m_missingPluginsCallback = MediaPlayerRequestInstallMissingPluginsCallback::create([this](uint32_t result) {
                    m_missingPluginsCallback = nullptr;
                    if (result != GST_INSTALL_PLUGINS_SUCCESS)
                        return;

                    changePipelineState(GST_STATE_READY);
                    changePipelineState(GST_STATE_PAUSED);
                });
                GUniquePtr<char> detail(gst_missing_plugin_message_get_installer_detail(message));
                GUniquePtr<char> description(gst_missing_plugin_message_get_description(message));
                m_player->client().requestInstallMissingPlugins(String::fromUTF8(detail.get()), String::fromUTF8(description.get()), *m_missingPluginsCallback);
            }
        }
#if ENABLE(VIDEO_TRACK) && USE(GSTREAMER_MPEGTS)
        else {
            GstMpegtsSection* section = gst_message_parse_mpegts_section(message);
            if (section) {
                processMpegTsSection(section);
                gst_mpegts_section_unref(section);
            }
        }
#endif
        break;
#if ENABLE(VIDEO_TRACK)
    case GST_MESSAGE_TOC:
        processTableOfContents(message);
        break;
#endif
    default:
        LOG_MEDIA_MESSAGE("Unhandled GStreamer message type: %s",
                    GST_MESSAGE_TYPE_NAME(message));
        break;
    }
    return;
}

void MediaPlayerPrivateGStreamer::processBufferingStats(GstMessage* message)
{
    m_buffering = true;
    gst_message_parse_buffering(message, &m_bufferingPercentage);

    LOG_MEDIA_MESSAGE("[Buffering] Buffering: %d%%.", m_bufferingPercentage);

    updateStates();
}

#if ENABLE(VIDEO_TRACK) && USE(GSTREAMER_MPEGTS)
void MediaPlayerPrivateGStreamer::processMpegTsSection(GstMpegtsSection* section)
{
    ASSERT(section);

    if (section->section_type == GST_MPEGTS_SECTION_PMT) {
        const GstMpegtsPMT* pmt = gst_mpegts_section_get_pmt(section);
        m_metadataTracks.clear();
        for (guint i = 0; i < pmt->streams->len; ++i) {
            const GstMpegtsPMTStream* stream = static_cast<const GstMpegtsPMTStream*>(g_ptr_array_index(pmt->streams, i));
            if (stream->stream_type == 0x05 || stream->stream_type >= 0x80) {
                AtomicString pid = String::number(stream->pid);
                RefPtr<InbandMetadataTextTrackPrivateGStreamer> track = InbandMetadataTextTrackPrivateGStreamer::create(
                    InbandTextTrackPrivate::Metadata, InbandTextTrackPrivate::Data, pid);

                // 4.7.10.12.2 Sourcing in-band text tracks
                // If the new text track's kind is metadata, then set the text track in-band metadata track dispatch
                // type as follows, based on the type of the media resource:
                // Let stream type be the value of the "stream_type" field describing the text track's type in the
                // file's program map section, interpreted as an 8-bit unsigned integer. Let length be the value of
                // the "ES_info_length" field for the track in the same part of the program map section, interpreted
                // as an integer as defined by the MPEG-2 specification. Let descriptor bytes be the length bytes
                // following the "ES_info_length" field. The text track in-band metadata track dispatch type must be
                // set to the concatenation of the stream type byte and the zero or more descriptor bytes bytes,
                // expressed in hexadecimal using uppercase ASCII hex digits.
                String inbandMetadataTrackDispatchType;
                appendUnsignedAsHexFixedSize(stream->stream_type, inbandMetadataTrackDispatchType, 2);
                for (guint j = 0; j < stream->descriptors->len; ++j) {
                    const GstMpegtsDescriptor* descriptor = static_cast<const GstMpegtsDescriptor*>(g_ptr_array_index(stream->descriptors, j));
                    for (guint k = 0; k < descriptor->length; ++k)
                        appendByteAsHex(descriptor->data[k], inbandMetadataTrackDispatchType);
                }
                track->setInBandMetadataTrackDispatchType(inbandMetadataTrackDispatchType);

                m_metadataTracks.add(pid, track);
                m_player->addTextTrack(track);
            }
        }
    } else {
        AtomicString pid = String::number(section->pid);
        RefPtr<InbandMetadataTextTrackPrivateGStreamer> track = m_metadataTracks.get(pid);
        if (!track)
            return;

        GRefPtr<GBytes> data = gst_mpegts_section_get_data(section);
        gsize size;
        const void* bytes = g_bytes_get_data(data.get(), &size);

        track->addDataCue(MediaTime::createWithDouble(currentTimeDouble()), MediaTime::createWithDouble(currentTimeDouble()), bytes, size);
    }
}
#endif

#if ENABLE(VIDEO_TRACK)
void MediaPlayerPrivateGStreamer::processTableOfContents(GstMessage* message)
{
    if (m_chaptersTrack)
        m_player->removeTextTrack(m_chaptersTrack);

    m_chaptersTrack = InbandMetadataTextTrackPrivateGStreamer::create(InbandTextTrackPrivate::Chapters, InbandTextTrackPrivate::Generic);
    m_player->addTextTrack(m_chaptersTrack);

    GRefPtr<GstToc> toc;
    gboolean updated;
    gst_message_parse_toc(message, &toc.outPtr(), &updated);
    ASSERT(toc);

    for (GList* i = gst_toc_get_entries(toc.get()); i; i = i->next)
        processTableOfContentsEntry(static_cast<GstTocEntry*>(i->data), 0);
}

void MediaPlayerPrivateGStreamer::processTableOfContentsEntry(GstTocEntry* entry, GstTocEntry* parent)
{
    UNUSED_PARAM(parent);
    ASSERT(entry);

    RefPtr<GenericCueData> cue = GenericCueData::create();

    gint64 start = -1, stop = -1;
    gst_toc_entry_get_start_stop_times(entry, &start, &stop);
    if (start != -1)
        cue->setStartTime(MediaTime(start, GST_SECOND));
    if (stop != -1)
        cue->setEndTime(MediaTime(stop, GST_SECOND));

    GstTagList* tags = gst_toc_entry_get_tags(entry);
    if (tags) {
        gchar* title =  0;
        gst_tag_list_get_string(tags, GST_TAG_TITLE, &title);
        if (title) {
            cue->setContent(title);
            g_free(title);
        }
    }

    m_chaptersTrack->addGenericCue(cue.release());

    for (GList* i = gst_toc_entry_get_sub_entries(entry); i; i = i->next)
        processTableOfContentsEntry(static_cast<GstTocEntry*>(i->data), entry);
}
#endif

void MediaPlayerPrivateGStreamer::fillTimerFired()
{
    GstQuery* query = gst_query_new_buffering(GST_FORMAT_PERCENT);

    if (!gst_element_query(m_pipeline.get(), query)) {
        gst_query_unref(query);
        return;
    }

    gint64 start, stop;
    gdouble fillStatus = 100.0;

    gst_query_parse_buffering_range(query, 0, &start, &stop, 0);
    gst_query_unref(query);

    if (stop != -1)
        fillStatus = 100.0 * stop / GST_FORMAT_PERCENT_MAX;

    LOG_MEDIA_MESSAGE("[Buffering] Download buffer filled up to %f%%", fillStatus);

    if (!m_mediaDuration)
        durationChanged();

    // Update maxTimeLoaded only if the media duration is
    // available. Otherwise we can't compute it.
    if (m_mediaDuration) {
        if (fillStatus == 100.0)
            m_maxTimeLoaded = m_mediaDuration;
        else
            m_maxTimeLoaded = static_cast<float>((fillStatus * m_mediaDuration) / 100.0);
        LOG_MEDIA_MESSAGE("[Buffering] Updated maxTimeLoaded: %f", m_maxTimeLoaded);
    }

    m_downloadFinished = fillStatus == 100.0;
    if (!m_downloadFinished) {
        updateStates();
        return;
    }

    // Media is now fully loaded. It will play even if network
    // connection is cut. Buffering is done, remove the fill source
    // from the main loop.
    m_fillTimer.stop();
    updateStates();
}

float MediaPlayerPrivateGStreamer::maxTimeSeekable() const
{
    if (m_errorOccured)
        return 0.0f;

    LOG_MEDIA_MESSAGE("maxTimeSeekable");
    // infinite duration means live stream
    if (std::isinf(duration()))
        return 0.0f;

    return duration();
}

float MediaPlayerPrivateGStreamer::sampleBufferLoadedTime() const
{
    if (!isSampleBufferBacked() || m_expectedTotalSize <= 0)
        return 0.0f;

    float mediaDuration = duration();
    if (!mediaDuration || std::isinf(mediaDuration))
        return 0.0f;

    uint64_t delivered = m_sampleBuffer->sizeSoFar();
    if (delivered > static_cast<uint64_t>(m_expectedTotalSize))
        delivered = static_cast<uint64_t>(m_expectedTotalSize);

    return static_cast<float>((static_cast<double>(delivered) / m_expectedTotalSize) * mediaDuration);
}

float MediaPlayerPrivateGStreamer::maxTimeLoaded() const
{
    if (m_errorOccured)
        return 0.0f;

    if (isSampleBufferBacked())
        return sampleBufferLoadedTime();

    float loaded = m_maxTimeLoaded;
    if (m_isEndReached && m_mediaDuration)
        loaded = m_mediaDuration;
    LOG_MEDIA_MESSAGE("maxTimeLoaded: %f", loaded);
    return loaded;
}

bool MediaPlayerPrivateGStreamer::didLoadingProgress() const
{
    // A plain pull-mode appsrc has no GStreamer-queryable byte-length until the transfer
    // finishes (see totalBytes()), so the generic guard below (which requires totalBytes() to
    // be nonzero) would always report no progress during a chunked, sample-buffer-backed load.
    // Compare sampleBufferLoadedTime() directly instead -- it doesn't depend on that query.
    if (isSampleBufferBacked()) {
        if (!m_pipeline || m_expectedTotalSize <= 0)
            return false;
        float currentLoaded = sampleBufferLoadedTime();
        bool progressed = currentLoaded != m_maxTimeLoadedAtLastDidLoadingProgress;
        m_maxTimeLoadedAtLastDidLoadingProgress = currentLoaded;
        return progressed;
    }

    if (!m_pipeline || !m_mediaDuration || (!isMediaSource() && !totalBytes()))
        return false;
    float currentMaxTimeLoaded = maxTimeLoaded();
    bool didLoadingProgress = currentMaxTimeLoaded != m_maxTimeLoadedAtLastDidLoadingProgress;
    m_maxTimeLoadedAtLastDidLoadingProgress = currentMaxTimeLoaded;
    LOG_MEDIA_MESSAGE("didLoadingProgress: %d", didLoadingProgress);
    return didLoadingProgress;
}

unsigned long long MediaPlayerPrivateGStreamer::totalBytes() const
{
    if (m_errorOccured)
        return 0;

    if (m_totalBytes)
        return m_totalBytes;

    if (!m_source)
        return 0;

    GstFormat fmt = GST_FORMAT_BYTES;
    gint64 length = 0;
    if (gst_element_query_duration(m_source.get(), fmt, &length)) {
        INFO_MEDIA_MESSAGE("totalBytes %" G_GINT64_FORMAT, length);
        m_totalBytes = static_cast<unsigned long long>(length);
        m_isStreaming = !length;
        return m_totalBytes;
    }

    // Fall back to querying the source pads manually.
    // See also https://bugzilla.gnome.org/show_bug.cgi?id=638749
    GstIterator* iter = gst_element_iterate_src_pads(m_source.get());
    bool done = false;
    while (!done) {
        GValue item = G_VALUE_INIT;
        switch (gst_iterator_next(iter, &item)) {
        case GST_ITERATOR_OK: {
            GstPad* pad = static_cast<GstPad*>(g_value_get_object(&item));
            gint64 padLength = 0;
            if (gst_pad_query_duration(pad, fmt, &padLength) && padLength > length)
                length = padLength;
            break;
        }
        case GST_ITERATOR_RESYNC:
            gst_iterator_resync(iter);
            break;
        case GST_ITERATOR_ERROR:
            FALLTHROUGH;
        case GST_ITERATOR_DONE:
            done = true;
            break;
        }

        g_value_unset(&item);
    }

    gst_iterator_free(iter);

    INFO_MEDIA_MESSAGE("totalBytes %" G_GINT64_FORMAT, length);
    m_totalBytes = static_cast<unsigned long long>(length);
    m_isStreaming = !length;
    return m_totalBytes;
}

void MediaPlayerPrivateGStreamer::sourceChangedCallback(MediaPlayerPrivateGStreamer* player)
{
    player->sourceChanged();
}

void MediaPlayerPrivateGStreamer::sourceChanged()
{
    m_source.clear();
    g_object_get(m_pipeline.get(), "source", &m_source.outPtr(), nullptr);

    if (WEBKIT_IS_WEB_SRC(m_source.get()))
        webKitWebSrcSetMediaPlayer(WEBKIT_WEB_SRC(m_source.get()), m_player);
#if ENABLE(MEDIA_SOURCE)
    if (m_mediaSource && WEBKIT_IS_MEDIA_SRC(m_source.get())) {
        MediaSourceGStreamer::open(m_mediaSource.get(), WEBKIT_MEDIA_SRC(m_source.get()));
    }
#endif
    if (m_sampleBuffer && WEBKIT_IS_BUFFER_SRC(m_source.get())) {
        webKitBufferSrcSetBuffer(WEBKIT_BUFFER_SRC(m_source.get()), m_sampleBuffer.get(), m_expectedTotalSize);
        // Re-derives the current m_source fresh on every invocation rather than capturing the
        // WebKitBufferSrc* directly -- safe regardless of exactly when this callback fires
        // relative to pipeline teardown/replacement, since m_source is a GRefPtr this class
        // already keeps correctly in sync with the live pipeline.
        m_sampleBuffer->setOnDataAvailable([this] { sampleBufferDataAvailable(); });
        // Runs on whichever thread a push attempt happens to be on (GStreamer's own streaming
        // thread, ordinarily) -- hop to the main thread before touching any player state, exactly
        // like every other GStreamer-thread-originated notification in this class already does.
        // See sampleBufferStalled()'s own comment for why this must never run inline here.
        webKitBufferSrcSetOnStalled(WEBKIT_BUFFER_SRC(m_source.get()), [this] {
            m_notifier.notify(MainThreadNotification::SampleBufferStalled, [this] { sampleBufferStalled(); });
        });
    }
}

bool MediaPlayerPrivateGStreamer::shouldPlayGivenBufferedAhead(float bufferedAheadSeconds, bool currentlyPlaying) const
{
    // The single decision point for "given how much buffered runway sits ahead of the current
    // position right now, should the pipeline be playing or waiting?" -- both sampleBufferStalled()
    // (currentlyPlaying=true, deciding whether to pause) and sampleBufferDataAvailable()
    // (currentlyPlaying=false, deciding whether to resume) consult this instead of each carrying
    // their own ad hoc threshold. A real two-watermark hysteresis, not one threshold reused in both
    // directions: resuming at the same margin that triggers a pause would immediately re-trigger
    // that same pause on the very next need-data cycle, since real-time playback consumes buffered
    // runway continuously. Live-reported symptom this fixes: position "jittering forward to stay
    // near the buffer edge" instead of holding still through one comfortable wait -- i.e. rapid,
    // closely-spaced pause/resume cycling. That cycling is not just poor UX; each cycle is another
    // chance to hit this Pi's V4L2 hardware decoder failing to survive a rapid pause-then-resume
    // retarget (see requestBufferingPipelineState()'s own comment) -- fewer, longer, more decisive
    // waits genuinely protect the pipeline, not merely look smoother.
    //
    // Consulted ONLY by this class's own internal stall/recovery machinery -- never by play(),
    // pause(), or seek(). Those stay the abstract, always-immediately-honored input side (matching
    // this project's own target-vs-engine-position layering principle): a real user asking to play
    // is never blocked waiting for a comfortable margin, even a razor-thin one is accepted
    // immediately. play() explicitly reconciles m_buffering when this happens (see its own comment)
    // specifically so the *low* watermark below -- the one unconditional, always-active guard that
    // actually detects genuine data exhaustion regardless of who or what started playback -- stays
    // free to catch the moment that thin-margin playback for real runs out, rather than being
    // silently masked by stale "already handling a stall" bookkeeping.
    // lowWatermarkSeconds was originally 0.2 -- technically enough runway to avoid a literal
    // underrun, but live GST_DEBUG tracing (with per-check logging temporarily added) showed the
    // real problem wasn't check frequency (this fires roughly every chunk, ~300ms, plenty often):
    // during genuinely slow delivery, the margin doesn't cross straight from comfortable to zero,
    // it hovers in a thin band (observed: ~0.25-0.55s, non-monotonically, for ~2.5 real seconds)
    // whenever delivery and consumption rates are nearly balanced moment-to-moment. 0.2s put the
    // pause decision *inside* that hovering band instead of before it, so playback visibly crept
    // right up against the loaded edge for seconds at a time before finally, barely, triggering --
    // exactly the "chases the edge... instead of staying put and waiting again" symptom reported
    // live. Raised to a full second: still comfortably nonzero real runway when the pause commits
    // (never a literal underrun), but decisively clear of that hovering band, so the decision to
    // stop is made while there's still an obvious, generous margin left -- not a photo finish.
    static const float lowWatermarkSeconds = 1.0;
    static const float highWatermarkSeconds = 3.0;
    if (currentlyPlaying)
        return bufferedAheadSeconds > lowWatermarkSeconds;
    return bufferedAheadSeconds >= highWatermarkSeconds;
}

void MediaPlayerPrivateGStreamer::requestBufferingPipelineState(GstState target)
{
    // The buffering-pause/resume machinery must never retarget a pipeline state transition that's
    // still in flight -- live-confirmed that this Pi's V4L2 hardware decoder (v4l2h264dec) does not
    // reliably survive a rapid PAUSED-then-PLAYING retarget: gst_element_set_state() itself accepts
    // it (ordinarily valid, safe GStreamer usage), but the decoder can end up permanently wedged --
    // gst_element_get_state() afterward returns an incoherent snapshot (current == pending inside an
    // ASYNC result) and nothing downstream ever asks for more data again. This is a hardware/driver
    // limitation on this device, not something fixable from the WebKit side, and software decode
    // isn't an acceptable default here -- so debounce at this layer instead. If a transition is
    // already pending, queue this target and return without touching the pipeline at all;
    // updateStates() applies the queued target itself, once it next observes the pipeline as
    // genuinely, stably settled (GST_STATE_CHANGE_SUCCESS with no pending transition), never
    // mid-flight. This mirrors this project's own target-vs-engine-position split for seeking into
    // unbuffered territory: the buffering state machine's *desired* target is tracked unclamped, and
    // only actually applied to the real pipeline once that's safe.
    ASSERT(isMainThread());
    GstState currentState, pending;
    gst_element_get_state(m_pipeline.get(), &currentState, &pending, 0);
    if (pending != GST_STATE_VOID_PENDING && pending != target) {
        LOG_MEDIA_MESSAGE("[Buffering] Deferring state request to %s (currently %s with %s pending)",
            gst_element_state_get_name(target), gst_element_state_get_name(currentState), gst_element_state_get_name(pending));
        m_pendingBufferingPipelineState = target;
        return;
    }
    m_pendingBufferingPipelineState = GST_STATE_VOID_PENDING;
    changePipelineState(target);
}

void MediaPlayerPrivateGStreamer::sampleBufferDataAvailable()
{
    // Tried making this skip pushing into appsrc while internally paused for buffering (the
    // hypothesis: an occasionally-observed stuck pipeline transition, see sampleBufferStalled()'s
    // comment, might be caused by appsrc still accepting pushes -- and therefore still feeding
    // downstream -- while changePipelineState(PAUSED) is trying to settle). Reverted: it caused a
    // much worse, live-confirmed regression (duration never resolved, playback never started at
    // all, for the entire run) despite m_buffering provably being false the whole time in that
    // failing run (0 "[Buffering]" log lines) -- i.e. the guard was gating on a condition that
    // should have been a no-op at that point, and evidently wasn't. Root cause not understood;
    // don't re-attempt this specific mitigation without first explaining that contradiction, not
    // just re-testing it. See project_hipe_pull_to_push_redesign in memory for the full account.
    if (m_source && WEBKIT_IS_BUFFER_SRC(m_source.get()))
        webKitBufferSrcDataAvailable(WEBKIT_BUFFER_SRC(m_source.get()));

    // New data just arrived -- if playback is currently paused-for-buffering (see
    // sampleBufferStalled()), check whether this is enough to lift it. Mirrors a real network
    // source's buffering-percent reaching 100%, computed from MediaSampleBuffer's own progress
    // instead of a GST_MESSAGE_BUFFERING bus message (a plain appsrc never generates one -- see
    // buffered()'s own isSampleBufferBacked() branch for the same reasoning already applied
    // there). Runs on the main thread already (this function is only ever reached via
    // MediaSampleBuffer::notifyDataAvailable(), itself only ever called from
    // HTMLMediaElement::appendBinaryMediaData() et al.), so updateStates() is safe to call
    // directly here, unlike from sampleBufferStalled()'s own GStreamer-thread trigger.
    if (!m_buffering || !isSampleBufferBacked())
        return;

    // See shouldPlayGivenBufferedAhead()'s own comment for the hysteresis this applies -- a
    // comfortable cushion is required before resuming, not merely clearing zero, specifically so
    // real-time consumption doesn't immediately re-exhaust a razor-thin margin and bounce straight
    // back into sampleBufferStalled() on the very next need-data cycle.
    if (!m_sampleBuffer->isFinished() && !shouldPlayGivenBufferedAhead(sampleBufferLoadedTime() - playbackPosition(), false)) {
        LOG_MEDIA_MESSAGE("[Buffering] Not enough margin yet: loaded=%f position=%f", sampleBufferLoadedTime(), playbackPosition());
        return;
    }

    LOG_MEDIA_MESSAGE("[Buffering] Resume check passed: loaded=%f position=%f finished=%d userRequestedPlay=%d rate=%f",
        sampleBufferLoadedTime(), playbackPosition(), m_sampleBuffer->isFinished(), m_userRequestedPlay, m_playbackRate);

    m_downloadFinished = m_sampleBuffer->isFinished();
    m_buffering = false;
    m_bufferingPercentage = 100;
    // Must resume the pipeline directly here, not rely on updateStates()'s own
    // `didBuffering && !m_buffering` resume check further down -- that check captures
    // `didBuffering = m_buffering` at the *top* of its own call, which by the time it runs here
    // already sees the `false` this function just set, so it can never observe the "was
    // buffering, just stopped" transition and never fires on its own. Confirmed live: without
    // this, the pipeline only ever resumed indirectly and inconsistently, via paused() briefly
    // reporting the real (still-PAUSED) GST state again once m_buffering cleared and
    // HTMLMediaElement::updatePlayState() happening to notice on its own schedule -- sometimes
    // promptly, sometimes not for a while, exactly the "sits there chasing the buffer line" shape
    // reported live. Gated on m_userRequestedPlay (NOT m_paused -- see that member's own comment
    // for why checking m_paused here was a real, live-confirmed bug: it gets clobbered true by
    // updateStates()'s own state-mirroring the instant the buffering-pause itself takes effect, so
    // the *original* version of this check could permanently refuse to ever resume, exactly
    // matching a live-reproduced "buffering completes but playback stays paused" symptom) and
    // m_playbackRate, so a real user-pause or zero rate during buffering is still respected.
    if (m_userRequestedPlay && m_playbackRate) {
        // See requestBufferingPipelineState()'s own comment -- never retargets a still-in-flight
        // transition directly; queues instead if one is pending.
        requestBufferingPipelineState(GST_STATE_PLAYING);
        LOG_MEDIA_MESSAGE("[Buffering] Resume requested");
        // See sampleBufferStalled()'s own cooldown check -- this timestamp is what it's measured
        // against.
        m_lastBufferingResumeTime = monotonicallyIncreasingTime();
    } else {
        LOG_MEDIA_MESSAGE("[Buffering] Resume check passed but not resuming: userRequestedPlay=%d m_playbackRate=%f", m_userRequestedPlay, m_playbackRate);
    }
    updateStates();
}

void MediaPlayerPrivateGStreamer::sampleBufferStalled()
{
    // The push-mode source just found nothing available at the read/playback position, with more
    // still to come (not real end-of-stream -- see webKitBufferSrcSetOnStalled()'s own comment).
    // This is the sample-buffer equivalent of a real network source's queue2 posting
    // GST_MESSAGE_BUFFERING below 100%, so it routes through the exact same, already
    // battle-tested pause-for-buffering machinery updateStates() already drives for a real
    // download (m_buffering/m_bufferingPercentage -- see processBufferingStats() and
    // updateStates()'s own GST_STATE_PLAYING branch, which genuinely pauses the pipeline, not
    // just the displayed position, so the actual rendered frame holds still along with the
    // number -- unlike the reverted display-only freeze this project tried once before, see
    // project_hipe_stall_detection_fix in memory). Never touches m_paused (the user's own
    // play/pause intent), and updateStates()'s own resume logic already checks !m_paused before
    // ever restarting playback on its own -- see feedback_separate_buffering_pause_from_user_pause
    // in memory.
    //
    // Must only ever be reached via the MainThreadNotifier hop wired up in sourceChanged() --
    // never call this (or updateStates()/changePipelineState()) directly from the GStreamer
    // streaming thread that webKitBufferSrcSetOnStalled()'s callback actually runs on. Doing so
    // risks exactly the deadlock class the whole push-mode redesign exists to eliminate: this
    // project's entire earlier pull-mode design was abandoned specifically because
    // changePipelineState() is dangerous when called from a moment the streaming thread itself is
    // still inside GStreamer's own machinery -- see project_hipe_chunked_media_underrun_deadlock
    // in memory for the original, gdb-confirmed incident.
    ASSERT(isMainThread());
    if (m_buffering || !m_sampleBuffer)
        return;

    // This notification can be stale. It travels from the streaming thread to this one through the
    // main-thread notifier, and this thread can be busy in the meantime -- on the very first load in
    // a process it is building the whole GStreamer pipeline -- applying the data that was missing
    // when the notification was raised. A complete buffer cannot stall (once the source has read to
    // the end of a finished buffer it reports end-of-stream, not a stall), and acting on the
    // notification would be worse than useless: the hold below is only ever lifted by
    // sampleBufferDataAvailable(), which runs when new data arrives, and none ever will. The
    // duration is typically still unknown at that point, so the check below would read
    // sampleBufferLoadedTime()'s "cannot tell" 0 as "nothing loaded" and always enter the hold. The
    // element was then left with readyState stuck at HAVE_CURRENT_DATA, so it never fired "canplay"
    // and never started playing: the first video played after hiped started never advanced.
    //
    // (With an unfinished buffer a hold entered on such a value is harmless: the next data to arrive
    // re-evaluates it.)
    LOG_MEDIA_MESSAGE("[Buffering] Stall notification: bufferFinished=%d duration=%f", m_sampleBuffer->isFinished(), duration());
    if (m_sampleBuffer->isFinished()) {
        LOG_MEDIA_MESSAGE("[Buffering] Ignoring stale stall notification: the buffer is already complete");
        return;
    }

    // The trigger for this call (webKitBufferSrcTryPushData() finding nothing left to push) only
    // means WebKitBufferSrc has handed everything currently loaded to appsrc's own internal queue
    // -- NOT that playback has actually caught up to it. appsrc (and any downstream queue) can
    // easily still be holding a couple of seconds of already-pushed, not-yet-rendered content at
    // the exact moment the push cursor itself runs dry, since pushing happens eagerly ahead of
    // real-time consumption. Acting on the raw push-exhaustion signal directly (the first version
    // of this fix) paused far too eagerly and repeatedly -- live-confirmed: it produced both a
    // much longer initial hold than actually necessary, and continuous jitter afterward, because
    // the push cursor re-exhausts and re-signals almost immediately every time appsrc's queue
    // drains and asks for more, regardless of how much runway the RENDER position actually still
    // has. Only treat this as a genuine stall if the render position itself is actually close to
    // the loaded edge -- this is what a real network source's queue2 measures too (buffered-ahead
    // of the current playback position within its own ring buffer, not "any bytes left upstream").
    // See shouldPlayGivenBufferedAhead()'s own comment for the hysteresis this applies.
    // Captured once and reused below (as the buffering-hold position) rather than queried twice
    // -- see playbackPosition()'s own comment for why this specific read is the last one trusted
    // until we've genuinely resumed.
    float currentPosition = playbackPosition();
    if (shouldPlayGivenBufferedAhead(sampleBufferLoadedTime() - currentPosition, true)) {
        LOG_MEDIA_MESSAGE("[Buffering] Stall check: still fine, loaded=%f position=%f margin=%f", sampleBufferLoadedTime(), currentPosition, sampleBufferLoadedTime() - currentPosition);
        return; // Still plenty queued/renderable ahead -- not a real stall yet. We'll be called
                 // again once appsrc's queue actually drains far enough to ask for more.
    }

    LOG_MEDIA_MESSAGE("[Buffering] Stall check passed: loaded=%f position=%f", sampleBufferLoadedTime(), currentPosition);

    // Refuse to re-pause too soon after a resume, even if the margin check above says to. Live-
    // confirmed, root-caused via GST_DEBUG: pausing again ~300ms after a resume -- before
    // GStreamer's own async PLAYING transition from that resume had actually settled -- left the
    // pipeline permanently stuck straddling states (gst_element_get_state() reporting
    // current=PLAYING with pending=PAUSED indefinitely afterward), because the new PAUSED request
    // raced the still-in-flight PLAYING one instead of following it. This is exactly the
    // "sits there chasing the buffer line, no meaningful playback" symptom reported live -- not a
    // display artifact, a genuinely wedged pipeline. Giving the just-resumed pipeline a minimum
    // real run before it's allowed to pause again avoids racing GStreamer's own state machine.
    static const double resumeCooldownSeconds = 1.0;
    if (m_lastBufferingResumeTime && monotonicallyIncreasingTime() - m_lastBufferingResumeTime < resumeCooldownSeconds) {
        LOG_MEDIA_MESSAGE("[Buffering] Stall detected but in resume cooldown (%f s since last resume)",
            monotonicallyIncreasingTime() - m_lastBufferingResumeTime);
        return;
    }

    LOG_MEDIA_MESSAGE("[Buffering] Pausing for real: holdPosition=%f", currentPosition);
    m_bufferingHoldPosition = currentPosition;
    m_buffering = true;
    m_bufferingPercentage = duration() > 0 ? static_cast<int>((sampleBufferLoadedTime() / duration()) * 100) : 0;
    if (m_bufferingPercentage >= 100)
        m_bufferingPercentage = 99; // Genuinely stalled right now -- never claim "complete" from this path.
    updateStates();
}

void MediaPlayerPrivateGStreamer::cancelLoad()
{
    if (m_networkState < MediaPlayer::Loading || m_networkState == MediaPlayer::Loaded)
        return;

    if (m_pipeline)
        changePipelineState(GST_STATE_READY);
}

void MediaPlayerPrivateGStreamer::asyncStateChangeDone()
{
    if (!m_pipeline || m_errorOccured)
        return;

    if (m_seeking) {
        if (m_seekIsPending)
            updateStates();
        else {
            LOG_MEDIA_MESSAGE("[Seek] seeked to %f", m_seekTime);
            m_seeking = false;
            m_restartingFromStart = false;
            if (m_timeOfOverlappingSeek != m_seekTime && m_timeOfOverlappingSeek != -1) {
                seek(m_timeOfOverlappingSeek);
                m_timeOfOverlappingSeek = -1;
                return;
            }
            m_timeOfOverlappingSeek = -1;

            // The pipeline can still have a pending state. In this case a position query will fail.
            // Right now we can use m_seekTime as a fallback.
            m_canFallBackToLastFinishedSeekPosition = true;
            timeChanged();
        }
    } else
        updateStates();
}

void MediaPlayerPrivateGStreamer::updateStates()
{
    if (!m_pipeline)
        return;

    if (m_errorOccured)
        return;

    MediaPlayer::NetworkState oldNetworkState = m_networkState;
    MediaPlayer::ReadyState oldReadyState = m_readyState;
    GstState state;
    GstState pending;

    // GST_NSECOND (not GST_MSECOND) here meant this timeout was really 250 *nanoseconds* --
    // effectively non-blocking, not the real ~250ms grace period the value was clearly meant to
    // give an in-flight async state change to settle. Live-confirmed as a real contributor to a
    // genuine stuck-playback bug via GST_DEBUG tracing: right after retargeting an in-flight
    // PAUSED->PLAYING transition (see changePipelineState()'s own comment on retargeting), this call
    // read back an incoherent snapshot (`Async: State: PLAYING, pending: PLAYING` -- current and
    // pending reported identically inside an ASYNC result, which should never happen for a
    // genuinely settled read), and playbackPosition()'s live GST_QUERY_POSITION then stayed frozen
    // indefinitely even though downstream elements kept emitting QoS messages. Letting this block
    // for a real ~250ms gives GStreamer's own state-change worker thread an actual chance to finish
    // the transition before this reads its result, instead of reading whatever transient bookkeeping
    // happens to exist at the exact instant a request was just issued.
    GstStateChangeReturn getStateResult = gst_element_get_state(m_pipeline.get(), &state, &pending, 250 * GST_MSECOND);

    bool shouldUpdatePlaybackState = false;
    switch (getStateResult) {
    case GST_STATE_CHANGE_SUCCESS: {
        LOG_MEDIA_MESSAGE("State: %s, pending: %s", gst_element_state_get_name(state), gst_element_state_get_name(pending));

        // Apply any buffering-driven state request that had to be deferred (via
        // requestBufferingPipelineState()) because a transition was still in flight when it was
        // originally requested. Only safe here: GST_STATE_CHANGE_SUCCESS (as opposed to ASYNC)
        // confirms the pipeline just reached a genuinely, stably settled state, not mid-transition
        // -- this is the one place this class can trust that. Routed back through
        // requestBufferingPipelineState() itself (not a bare changePipelineState() call) so it stays
        // idempotent against the "Sync states where needed" block further down also wanting to issue
        // a request this same call -- whichever runs first settles it, the other becomes a no-op or
        // a fresh, correctly-ordered deferral instead of a second uncoordinated request.
        if (pending == GST_STATE_VOID_PENDING && m_pendingBufferingPipelineState != GST_STATE_VOID_PENDING) {
            GstState target = m_pendingBufferingPipelineState;
            if (target != state) {
                LOG_MEDIA_MESSAGE("[Buffering] Applying deferred state request to %s now that pipeline settled at %s",
                    gst_element_state_get_name(target), gst_element_state_get_name(state));
                requestBufferingPipelineState(target);
            } else
                m_pendingBufferingPipelineState = GST_STATE_VOID_PENDING;
        }

        // Do nothing if on EOS and state changed to READY to avoid recreating the player
        // on HTMLMediaElement and properly generate the video 'ended' event.
        if (m_isEndReached && state == GST_STATE_READY)
            break;

        if (state <= GST_STATE_READY) {
            m_resetPipeline = true;
            // Kept across a restartPipelineFromStart(): the duration is known and unchanged, and
            // re-deriving it after the restart is unreliable for a fragmented file.
            if (!m_restartingFromStart)
                m_mediaDuration = 0;
        } else {
            m_resetPipeline = false;
            // Not a bare cacheDuration() call: this can be the point where duration first
            // becomes known for this pipeline (a chunked/appsrc-backed load can reach an
            // aggregate PAUSED/PLAYING state before qtdemux has parsed enough of a
            // progressively-arriving moov to know real duration, unlike GST_MESSAGE_DURATION_CHANGED
            // bus messages or fillTimerFired(), which don't fire for this source at all -- see
            // setDownloadBuffering()). durationChanged() both caches it and notifies the client;
            // a bare cacheDuration() here silently updated m_mediaDuration with no notification,
            // which meant nothing ever told HTMLMediaElement duration became known after an
            // earlier readyState-crossing-HAVE_METADATA call had already run reset() against a
            // still-unknown duration (permanently hiding the media-controls-timeline-container,
            // since only that first crossing is otherwise wired to show it again).
            durationChanged();
        }

        bool didBuffering = m_buffering;

        // Update ready and network states.
        switch (state) {
        case GST_STATE_NULL:
            m_readyState = MediaPlayer::HaveNothing;
            m_networkState = MediaPlayer::Empty;
            break;
        case GST_STATE_READY:
            m_readyState = MediaPlayer::HaveMetadata;
            m_networkState = MediaPlayer::Empty;
            break;
        case GST_STATE_PAUSED:
        case GST_STATE_PLAYING:
            if (m_buffering) {
                if (m_bufferingPercentage == 100) {
                    LOG_MEDIA_MESSAGE("[Buffering] Complete.");
                    m_buffering = false;
                    m_readyState = MediaPlayer::HaveEnoughData;
                    m_networkState = m_downloadFinished ? MediaPlayer::Idle : MediaPlayer::Loading;
                } else {
                    m_readyState = MediaPlayer::HaveCurrentData;
                    m_networkState = MediaPlayer::Loading;
                }
            } else if (m_downloadFinished) {
                m_readyState = MediaPlayer::HaveEnoughData;
                m_networkState = MediaPlayer::Loaded;
            } else {
                m_readyState = MediaPlayer::HaveFutureData;
                m_networkState = MediaPlayer::Loading;
            }

            break;
        default:
            ASSERT_NOT_REACHED();
            break;
        }

        // Sync states where needed.
        if (state == GST_STATE_PAUSED) {
            if (!m_volumeAndMuteInitialized) {
                notifyPlayerOfVolumeChange();
                notifyPlayerOfMute();
                m_volumeAndMuteInitialized = true;
            }

            if (didBuffering && !m_buffering && m_userRequestedPlay && m_playbackRate) {
                LOG_MEDIA_MESSAGE("[Buffering] Restarting playback.");
                requestBufferingPipelineState(GST_STATE_PLAYING);
            }
        } else if (state == GST_STATE_PLAYING) {
            m_paused = false;

            if ((m_buffering && !isLiveStream()) || !m_playbackRate) {
                LOG_MEDIA_MESSAGE("[Buffering] Pausing stream for buffering.");
                requestBufferingPipelineState(GST_STATE_PAUSED);
            }
        } else
            m_paused = true;

        if (m_requestedState == GST_STATE_PAUSED && state == GST_STATE_PAUSED) {
            shouldUpdatePlaybackState = true;
            LOG_MEDIA_MESSAGE("Requested state change to %s was completed", gst_element_state_get_name(state));
        }

        break;
    }
    case GST_STATE_CHANGE_ASYNC:
        LOG_MEDIA_MESSAGE("Async: State: %s, pending: %s", gst_element_state_get_name(state), gst_element_state_get_name(pending));
        // Change in progress.
        break;
    case GST_STATE_CHANGE_FAILURE:
        LOG_MEDIA_MESSAGE("Failure: State: %s, pending: %s", gst_element_state_get_name(state), gst_element_state_get_name(pending));
        // Change failed
        return;
    case GST_STATE_CHANGE_NO_PREROLL:
        LOG_MEDIA_MESSAGE("No preroll: State: %s, pending: %s", gst_element_state_get_name(state), gst_element_state_get_name(pending));

        // Live pipelines go in PAUSED without prerolling.
        m_isStreaming = true;
        setDownloadBuffering();

        if (state == GST_STATE_READY)
            m_readyState = MediaPlayer::HaveNothing;
        else if (state == GST_STATE_PAUSED) {
            m_readyState = MediaPlayer::HaveEnoughData;
            m_paused = true;
        } else if (state == GST_STATE_PLAYING)
            m_paused = false;

        if (!m_paused && m_playbackRate)
            changePipelineState(GST_STATE_PLAYING);

        m_networkState = MediaPlayer::Loading;
        break;
    default:
        LOG_MEDIA_MESSAGE("Else : %d", getStateResult);
        break;
    }

    m_requestedState = GST_STATE_VOID_PENDING;

    if (shouldUpdatePlaybackState)
        m_player->playbackStateChanged();

    if (m_networkState != oldNetworkState) {
        LOG_MEDIA_MESSAGE("Network State Changed from %u to %u", oldNetworkState, m_networkState);
        m_player->networkStateChanged();
    }
    if (m_readyState != oldReadyState) {
        LOG_MEDIA_MESSAGE("Ready State Changed from %u to %u", oldReadyState, m_readyState);
        m_player->readyStateChanged();
    }

    if (getStateResult == GST_STATE_CHANGE_SUCCESS && state >= GST_STATE_PAUSED) {
        updatePlaybackRate();
        if (m_seekIsPending) {
            LOG_MEDIA_MESSAGE("[Seek] committing pending seek to %f", m_seekTime);
            m_seekIsPending = false;
            // A restart still marked in flight has, by definition, just completed (we are looking at
            // the settled PAUSED state it was heading for); a seek queued behind it starts fresh.
            m_restartingFromStart = false;
            m_seeking = doSeek(toGstClockTime(m_seekTime), m_player->rate(), static_cast<GstSeekFlags>(GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE));
            if (!m_seeking) {
                LOG_MEDIA_MESSAGE("[Seek] seeking to %f failed", m_seekTime);
                seekRefused(m_seekTime);
            }
        }
    }
}

void MediaPlayerPrivateGStreamer::mediaLocationChanged(GstMessage* message)
{
    if (m_mediaLocations)
        gst_structure_free(m_mediaLocations);

    const GstStructure* structure = gst_message_get_structure(message);
    if (structure) {
        // This structure can contain:
        // - both a new-location string and embedded locations structure
        // - or only a new-location string.
        m_mediaLocations = gst_structure_copy(structure);
        const GValue* locations = gst_structure_get_value(m_mediaLocations, "locations");

        if (locations)
            m_mediaLocationCurrentIndex = static_cast<int>(gst_value_list_get_size(locations)) -1;

        loadNextLocation();
    }
}

bool MediaPlayerPrivateGStreamer::loadNextLocation()
{
    if (!m_mediaLocations)
        return false;

    const GValue* locations = gst_structure_get_value(m_mediaLocations, "locations");
    const gchar* newLocation = 0;

    if (!locations) {
        // Fallback on new-location string.
        newLocation = gst_structure_get_string(m_mediaLocations, "new-location");
        if (!newLocation)
            return false;
    }

    if (!newLocation) {
        if (m_mediaLocationCurrentIndex < 0) {
            m_mediaLocations = 0;
            return false;
        }

        const GValue* location = gst_value_list_get_value(locations,
                                                          m_mediaLocationCurrentIndex);
        const GstStructure* structure = gst_value_get_structure(location);

        if (!structure) {
            m_mediaLocationCurrentIndex--;
            return false;
        }

        newLocation = gst_structure_get_string(structure, "new-location");
    }

    if (newLocation) {
        // Found a candidate. new-location is not always an absolute url
        // though. We need to take the base of the current url and
        // append the value of new-location to it.
        URL baseUrl = gst_uri_is_valid(newLocation) ? URL() : m_url;
        URL newUrl = URL(baseUrl, newLocation);

        RefPtr<SecurityOrigin> securityOrigin = SecurityOrigin::create(m_url);
        if (securityOrigin->canRequest(newUrl)) {
            INFO_MEDIA_MESSAGE("New media url: %s", newUrl.string().utf8().data());

            // Reset player states.
            m_networkState = MediaPlayer::Loading;
            m_player->networkStateChanged();
            m_readyState = MediaPlayer::HaveNothing;
            m_player->readyStateChanged();

            // Reset pipeline state.
            m_resetPipeline = true;
            changePipelineState(GST_STATE_READY);

            GstState state;
            gst_element_get_state(m_pipeline.get(), &state, nullptr, 0);
            if (state <= GST_STATE_READY) {
                // Set the new uri and start playing.
                g_object_set(m_pipeline.get(), "uri", newUrl.string().utf8().data(), nullptr);
                m_url = newUrl;
                changePipelineState(GST_STATE_PLAYING);
                return true;
            }
        } else
            INFO_MEDIA_MESSAGE("Not allowed to load new media location: %s", newUrl.string().utf8().data());
    }
    m_mediaLocationCurrentIndex--;
    return false;
}

void MediaPlayerPrivateGStreamer::loadStateChanged()
{
    updateStates();
}

void MediaPlayerPrivateGStreamer::timeChanged()
{
    updateStates();
    m_player->timeChanged();
}

bool MediaPlayerPrivateGStreamer::loopIsRunningAway()
{
    // A looping element restarts itself at every end-of-stream we report, so anything that makes a
    // restart end immediately again -- a seek that keeps being refused, a decoder that fails
    // straight after starting, whatever comes next -- turns into an endless loop, and one that runs
    // on hiped's main thread. That is exactly how a refused seek-to-start once pinned the whole
    // display server (see seekRefused()). This does not depend on any particular cause, and in
    // particular not on how long a restart happens to take today: a healthy cycle plays the media
    // through, so it takes about duration/rate of wall time; several cycles in a row that each took
    // less than half of that did not play anything, and we stop with an error instead.
    static const double minPlayedFraction = 0.5;
    static const unsigned maxRapidLoopEnds = 5;

    double now = monotonicallyIncreasingTime();
    double previousEnd = m_lastLoopEndTime;
    m_lastLoopEndTime = now;

    // Nothing to judge against for the first end, or without a known duration.
    float length = m_mediaDuration;
    if (!previousEnd || !length || std::isinf(length)) {
        m_rapidLoopEnds = 0;
        return false;
    }

    float rate = m_playbackRate > 0 ? m_playbackRate : 1;
    if (now - previousEnd >= minPlayedFraction * length / rate) {
        m_rapidLoopEnds = 0;
        return false;
    }

    if (++m_rapidLoopEnds < maxRapidLoopEnds)
        return false;

    ERROR_MEDIA_MESSAGE("[Loop] %u consecutive end-of-stream events, each less than %.0f%% of the media's length apart: ending looped playback with an error instead of looping forever", m_rapidLoopEnds, minPlayedFraction * 100);
    m_rapidLoopEnds = 0;
    m_lastLoopEndTime = 0;
    // Same as a non-looping end (below): stop the pipeline rather than leaving it running at EOS.
    m_paused = true;
    changePipelineState(GST_STATE_READY);
    loadingFailed(MediaPlayer::DecodeError);
    return true;
}

void MediaPlayerPrivateGStreamer::didEnd()
{
    if (m_player->client().mediaPlayerIsLooping() && loopIsRunningAway())
        return;

    // Synchronize position and duration values to not confuse the
    // HTMLMediaElement. In some cases like reverse playback the
    // position is not always reported as 0 for instance.
    //
    // Never applies to a sample-buffer-backed load: duration for that pipeline shape is always
    // known exactly upfront (either from the client's HIPE_OP_SET_SRC size hint or from the
    // moov/mvhd box itself, both authoritative), so an EOS arriving at a position short of it is
    // never legitimate evidence the real duration was actually shorter -- unlike the
    // reverse-playback/estimated-duration sources this heuristic was written for, where the
    // duration itself may only ever have been an estimate. Silently trusting a short EOS position
    // here would instead mask a genuine premature-truncation bug in the delivery/decode pipeline,
    // and once corrupted it self-perpetuates: cacheDuration()'s own guard prevents ever
    // recomputing m_mediaDuration once it's nonzero, so every subsequent loop replays at the
    // wrong, truncated length too. See project_hipe_pull_to_push_redesign in memory.
    float now = currentTime();
    if (!isSampleBufferBacked() && now > 0 && now <= duration() && m_mediaDuration != now) {
        m_mediaDurationKnown = true;
        m_mediaDuration = now;
        m_player->durationChanged();
    }

    m_isEndReached = true;
    timeChanged();

    if (!m_player->client().mediaPlayerIsLooping()) {
        m_paused = true;
        changePipelineState(GST_STATE_READY);
        m_downloadFinished = false;
    }
}

void MediaPlayerPrivateGStreamer::cacheDuration()
{
    if (m_mediaDuration || !m_mediaDurationKnown)
        return;

    float newDuration = duration();
    if (std::isinf(newDuration)) {
        // Only pretend that duration is not available if the the query failed in a stable pipeline state.
        GstState state;
        if (gst_element_get_state(m_pipeline.get(), &state, nullptr, 0) == GST_STATE_CHANGE_SUCCESS && state > GST_STATE_READY)
            m_mediaDurationKnown = false;
        return;
    }

    m_mediaDuration = newDuration;
}

void MediaPlayerPrivateGStreamer::durationChanged()
{
    float previousDuration = m_mediaDuration;

    cacheDuration();
    // Notify on the very first resolution (previousDuration == 0) too, not just subsequent
    // changes -- that first-resolution case is only "already handled by HTMLMediaElement" (the
    // reasoning the old previousDuration-guarded version relied on) when readyState's own
    // HAVE_METADATA crossing happens at the same time duration becomes known. For a chunked/
    // appsrc-backed load those two events aren't reliably ordered together (see this function's
    // caller in updateStates()), so skipping the notification here left nothing to tell
    // HTMLMediaElement duration became known after an earlier, still-unknown-duration
    // readyState crossing had already run.
    if (m_mediaDuration != previousDuration)
        m_player->durationChanged();
}

void MediaPlayerPrivateGStreamer::loadingFailed(MediaPlayer::NetworkState error)
{
    m_errorOccured = true;
    if (m_networkState != error) {
        m_networkState = error;
        m_player->networkStateChanged();
    }
    if (m_readyState != MediaPlayer::HaveNothing) {
        m_readyState = MediaPlayer::HaveNothing;
        m_player->readyStateChanged();
    }

    // Loading failed, remove ready timer.
    m_readyTimerHandler.stop();
}

static HashSet<String, ASCIICaseInsensitiveHash>& mimeTypeSet()
{
    static NeverDestroyed<HashSet<String, ASCIICaseInsensitiveHash>> mimeTypes = []()
    {
        initializeGStreamerAndRegisterWebKitElements();
        HashSet<String, ASCIICaseInsensitiveHash> set;

        GList* audioDecoderFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_AUDIO, GST_RANK_MARGINAL);
        GList* videoDecoderFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DECODER | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO, GST_RANK_MARGINAL);
        GList* demuxerFactories = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_DEMUXER, GST_RANK_MARGINAL);

        enum ElementType {
            AudioDecoder = 0,
            VideoDecoder,
            Demuxer
        };
        struct GstCapsWebKitMapping {
            ElementType elementType;
            const char* capsString;
            Vector<AtomicString> webkitMimeTypes;
        };

        Vector<GstCapsWebKitMapping> mapping = {
            {AudioDecoder, "audio/midi", {"audio/midi", "audio/riff-midi"}},
            {AudioDecoder, "audio/x-sbc", { }},
            {AudioDecoder, "audio/x-sid", { }},
            {AudioDecoder, "audio/x-flac", {"audio/x-flac", "audio/flac"}},
            {AudioDecoder, "audio/x-wav", {"audio/x-wav", "audio/wav"}},
            {AudioDecoder, "audio/x-wavpack", {"audio/x-wavpack"}},
            {AudioDecoder, "audio/x-speex", {"audio/speex", "audio/x-speex"}},
            {AudioDecoder, "audio/x-ac3", { }},
            {AudioDecoder, "audio/x-eac3", {"audio/x-ac3"}},
            {AudioDecoder, "audio/x-dts", { }},
            {VideoDecoder, "video/x-h264, profile=(string)high", {"video/mp4", "video/x-m4v"}},
            {VideoDecoder, "video/x-msvideocodec", {"video/x-msvideo"}},
            {VideoDecoder, "video/x-h263", { }},
            {VideoDecoder, "video/mpegts", { }},
            {VideoDecoder, "video/mpeg, mpegversion=(int){1,2}, systemstream=(boolean)false", {"video/mpeg"}},
            {VideoDecoder, "video/x-dirac", { }},
            {VideoDecoder, "video/x-flash-video", {"video/flv", "video/x-flv"}},
            {Demuxer, "video/quicktime", { }},
            {Demuxer, "video/quicktime, variant=(string)3gpp", {"video/3gpp"}},
            {Demuxer, "application/x-3gp", { }},
            {Demuxer, "video/x-ms-asf", { }},
            {Demuxer, "audio/x-aiff", { }},
            {Demuxer, "application/x-pn-realaudio", { }},
            {Demuxer, "application/vnd.rn-realmedia", { }},
            {Demuxer, "audio/x-wav", {"audio/x-wav", "audio/wav"}},
            {Demuxer, "application/x-hls", {"application/vnd.apple.mpegurl", "application/x-mpegurl"}}
        };

        for (auto& current : mapping) {
            GList* factories = demuxerFactories;
            if (current.elementType == AudioDecoder)
                factories = audioDecoderFactories;
            else if (current.elementType == VideoDecoder)
                factories = videoDecoderFactories;

            if (gstRegistryHasElementForMediaType(factories, current.capsString)) {
                if (!current.webkitMimeTypes.isEmpty()) {
                    for (const auto& mimeType : current.webkitMimeTypes)
                        set.add(mimeType);
                } else
                    set.add(AtomicString(current.capsString));
            }
        }

        bool opusSupported = false;
        if (gstRegistryHasElementForMediaType(audioDecoderFactories, "audio/x-opus")) {
            opusSupported = true;
            set.add(AtomicString("audio/opus"));
        }

        bool vorbisSupported = false;
        if (gstRegistryHasElementForMediaType(demuxerFactories, "application/ogg")) {
            set.add(AtomicString("application/ogg"));

            vorbisSupported = gstRegistryHasElementForMediaType(audioDecoderFactories, "audio/x-vorbis");
            if (vorbisSupported) {
                set.add(AtomicString("audio/ogg"));
                set.add(AtomicString("audio/x-vorbis+ogg"));
            }

            if (gstRegistryHasElementForMediaType(videoDecoderFactories, "video/x-theora"))
                set.add(AtomicString("video/ogg"));
        }

        bool audioMpegSupported = false;
        if (gstRegistryHasElementForMediaType(audioDecoderFactories, "audio/mpeg, mpegversion=(int)1, layer=(int)[1, 3]")) {
            audioMpegSupported = true;
            set.add(AtomicString("audio/mp1"));
            set.add(AtomicString("audio/mp3"));
            set.add(AtomicString("audio/x-mp3"));
        }

        if (gstRegistryHasElementForMediaType(audioDecoderFactories, "audio/mpeg, mpegversion=(int){2, 4}")) {
            audioMpegSupported = true;
            set.add(AtomicString("audio/aac"));
            set.add(AtomicString("audio/mp2"));
            set.add(AtomicString("audio/mp4"));
            set.add(AtomicString("audio/x-m4a"));
        }

        if (audioMpegSupported) {
            set.add(AtomicString("audio/mpeg"));
            set.add(AtomicString("audio/x-mpeg"));
        }

        if (gstRegistryHasElementForMediaType(demuxerFactories, "video/x-matroska")) {
            set.add(AtomicString("video/x-matroska"));

            if (gstRegistryHasElementForMediaType(videoDecoderFactories, "video/x-vp8")
                || gstRegistryHasElementForMediaType(videoDecoderFactories, "video/x-vp9")
                || gstRegistryHasElementForMediaType(videoDecoderFactories, "video/x-vp10"))
                set.add(AtomicString("video/webm"));

            if (vorbisSupported || opusSupported)
                set.add(AtomicString("audio/webm"));
        }

        gst_plugin_feature_list_free(audioDecoderFactories);
        gst_plugin_feature_list_free(videoDecoderFactories);
        gst_plugin_feature_list_free(demuxerFactories);
        return set;
    }();
    return mimeTypes;
}

void MediaPlayerPrivateGStreamer::getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>& types)
{
    types = mimeTypeSet();
}

MediaPlayer::SupportsType MediaPlayerPrivateGStreamer::supportsType(const MediaEngineSupportParameters& parameters)
{
    if (parameters.type.isNull() || parameters.type.isEmpty())
        return MediaPlayer::IsNotSupported;

    // spec says we should not return "probably" if the codecs string is empty
    if (mimeTypeSet().contains(parameters.type))
        return parameters.codecs.isEmpty() ? MediaPlayer::MayBeSupported : MediaPlayer::IsSupported;
    return MediaPlayer::IsNotSupported;
}

void MediaPlayerPrivateGStreamer::setDownloadBuffering()
{
    if (!m_pipeline)
        return;

    unsigned flags;
    g_object_get(m_pipeline.get(), "flags", &flags, nullptr);

    unsigned flagDownload = getGstPlayFlag("download");

    // We don't want to stop downloading if we already started it.
    if (flags & flagDownload && m_readyState > MediaPlayer::HaveNothing && !m_resetPipeline)
        return;

    // Also excludes our own sample-buffer-backed source (see MediaSampleBuffer.h): playbin's
    // on-disk download buffering exists so a real network stream can be re-seeked without
    // re-fetching, which our MediaSampleBuffer already gives us for free by retaining everything
    // in memory -- the wrong mechanism for this source, not just an unnecessary one. It's actively
    // harmful here: fillTimerFired()'s buffering-percent query has no way to express "unknown"
    // for a stream whose total size isn't known yet (appsrc reports size=-1 until
    // finishBinaryMediaData() completes), so it defaults to reporting 100% instantly, falsely
    // telling the rest of this class the whole transfer already finished within a fraction of a
    // second of it actually starting.
    bool shouldDownload = !isLiveStream() && !isSampleBufferBacked() && m_preload == MediaPlayer::Auto;
    if (shouldDownload) {
        LOG_MEDIA_MESSAGE("Enabling on-disk buffering");
        g_object_set(m_pipeline.get(), "flags", flags | flagDownload, nullptr);
        m_fillTimer.startRepeating(0.2);
    } else {
        LOG_MEDIA_MESSAGE("Disabling on-disk buffering");
        g_object_set(m_pipeline.get(), "flags", flags & ~flagDownload, nullptr);
        m_fillTimer.stop();
    }
}

void MediaPlayerPrivateGStreamer::setPreload(MediaPlayer::Preload preload)
{
    if (preload == MediaPlayer::Auto && isLiveStream())
        return;

    m_preload = preload;
    setDownloadBuffering();

    if (m_delayingLoad && m_preload != MediaPlayer::None) {
        m_delayingLoad = false;
        commitLoad();
    }
}

GstElement* MediaPlayerPrivateGStreamer::createAudioSink()
{
    m_autoAudioSink = gst_element_factory_make("autoaudiosink", 0);
    if (!m_autoAudioSink) {
        WARN_MEDIA_MESSAGE("GStreamer's autoaudiosink not found. Please check your gst-plugins-good installation");
        return nullptr;
    }

    g_signal_connect_swapped(m_autoAudioSink.get(), "child-added", G_CALLBACK(setAudioStreamPropertiesCallback), this);

    GstElement* audioSinkBin;

    if (webkitGstCheckVersion(1, 4, 2)) {
#if ENABLE(WEB_AUDIO)
        audioSinkBin = gst_bin_new("audio-sink");
        m_audioSourceProvider->configureAudioBin(audioSinkBin, nullptr);
        return audioSinkBin;
#else
        return m_autoAudioSink.get();
#endif
    }

    // Construct audio sink only if pitch preserving is enabled.
    // If GStreamer 1.4.2 is used the audio-filter playbin property is used instead.
    if (m_preservesPitch) {
        GstElement* scale = gst_element_factory_make("scaletempo", nullptr);
        if (!scale) {
            WARN_MEDIA_MESSAGE("Failed to create scaletempo");
            return m_autoAudioSink.get();
        }

        audioSinkBin = gst_bin_new("audio-sink");
        gst_bin_add(GST_BIN(audioSinkBin), scale);
        GRefPtr<GstPad> pad = adoptGRef(gst_element_get_static_pad(scale, "sink"));
        gst_element_add_pad(audioSinkBin, gst_ghost_pad_new("sink", pad.get()));

#if ENABLE(WEB_AUDIO)
        m_audioSourceProvider->configureAudioBin(audioSinkBin, scale);
#else
        GstElement* convert = gst_element_factory_make("audioconvert", nullptr);
        GstElement* resample = gst_element_factory_make("audioresample", nullptr);

        gst_bin_add_many(GST_BIN(audioSinkBin), convert, resample, m_autoAudioSink.get(), nullptr);

        if (!gst_element_link_many(scale, convert, resample, m_autoAudioSink.get(), nullptr)) {
            WARN_MEDIA_MESSAGE("Failed to link audio sink elements");
            gst_object_unref(audioSinkBin);
            return m_autoAudioSink.get();
        }
#endif
        return audioSinkBin;
    }

#if ENABLE(WEB_AUDIO)
    audioSinkBin = gst_bin_new("audio-sink");
    m_audioSourceProvider->configureAudioBin(audioSinkBin, nullptr);
    return audioSinkBin;
#endif
    ASSERT_NOT_REACHED();
    return nullptr;
}

GstElement* MediaPlayerPrivateGStreamer::audioSink() const
{
    GstElement* sink;
    g_object_get(m_pipeline.get(), "audio-sink", &sink, nullptr);
    return sink;
}

void MediaPlayerPrivateGStreamer::createGSTPlayBin()
{
    ASSERT(!m_pipeline);

    // gst_element_factory_make() returns a floating reference so
    // we should not adopt.
    setPipeline(gst_element_factory_make("playbin", "play"));
    setStreamVolumeElement(GST_STREAM_VOLUME(m_pipeline.get()));

    GRefPtr<GstBus> bus = adoptGRef(gst_pipeline_get_bus(GST_PIPELINE(m_pipeline.get())));
    gst_bus_set_sync_handler(bus.get(), [](GstBus*, GstMessage* message, gpointer userData) {
        auto& player = *static_cast<MediaPlayerPrivateGStreamer*>(userData);

        if (!player.handleSyncMessage(message)) {
            GRefPtr<GstMessage> protectedMessage(message);
            auto weakThis = player.createWeakPtr();
            RunLoop::main().dispatch([weakThis, protectedMessage] {
                if (weakThis)
                    weakThis->handleMessage(protectedMessage.get());
            });
        }
        gst_message_unref(message);
        return GST_BUS_DROP;
    }, this, nullptr);

    g_object_set(m_pipeline.get(), "mute", m_player->muted(), nullptr);

    g_signal_connect_swapped(m_pipeline.get(), "notify::source", G_CALLBACK(sourceChangedCallback), this);
    g_signal_connect_swapped(m_pipeline.get(), "video-changed", G_CALLBACK(videoChangedCallback), this);
    g_signal_connect_swapped(m_pipeline.get(), "audio-changed", G_CALLBACK(audioChangedCallback), this);
#if ENABLE(VIDEO_TRACK)
    if (webkitGstCheckVersion(1, 1, 2)) {
        g_signal_connect_swapped(m_pipeline.get(), "text-changed", G_CALLBACK(textChangedCallback), this);

        GstElement* textCombiner = webkitTextCombinerNew();
        ASSERT(textCombiner);
        g_object_set(m_pipeline.get(), "text-stream-combiner", textCombiner, nullptr);

        m_textAppSink = webkitTextSinkNew();
        ASSERT(m_textAppSink);

        m_textAppSinkPad = adoptGRef(gst_element_get_static_pad(m_textAppSink.get(), "sink"));
        ASSERT(m_textAppSinkPad);

        g_object_set(m_textAppSink.get(), "emit-signals", true, "enable-last-sample", false, "caps", gst_caps_new_empty_simple("text/vtt"), NULL);
        g_signal_connect_swapped(m_textAppSink.get(), "new-sample", G_CALLBACK(newTextSampleCallback), this);

        g_object_set(m_pipeline.get(), "text-sink", m_textAppSink.get(), NULL);
    }
#endif

    g_object_set(m_pipeline.get(), "video-sink", createVideoSink(), "audio-sink", createAudioSink(), nullptr);

    // On 1.4.2 and newer we use the audio-filter property instead.
    // See https://bugzilla.gnome.org/show_bug.cgi?id=735748 for
    // the reason for using >= 1.4.2 instead of >= 1.4.0.
    if (m_preservesPitch && webkitGstCheckVersion(1, 4, 2)) {
        GstElement* scale = gst_element_factory_make("scaletempo", 0);

        if (!scale)
            WARN_MEDIA_MESSAGE("Failed to create scaletempo");
        else
            g_object_set(m_pipeline.get(), "audio-filter", scale, nullptr);
    }

    GRefPtr<GstPad> videoSinkPad = adoptGRef(gst_element_get_static_pad(m_videoSink.get(), "sink"));
    if (videoSinkPad)
        g_signal_connect_swapped(videoSinkPad.get(), "notify::caps", G_CALLBACK(videoSinkCapsChangedCallback), this);
}

void MediaPlayerPrivateGStreamer::simulateAudioInterruption()
{
    GstMessage* message = gst_message_new_request_state(GST_OBJECT(m_pipeline.get()), GST_STATE_PAUSED);
    gst_element_post_message(m_pipeline.get(), message);
}

bool MediaPlayerPrivateGStreamer::didPassCORSAccessCheck() const
{
    if (WEBKIT_IS_WEB_SRC(m_source.get()))
        return webKitSrcPassedCORSAccessCheck(WEBKIT_WEB_SRC(m_source.get()));
    return false;
}

bool MediaPlayerPrivateGStreamer::canSaveMediaData() const
{
    if (isLiveStream())
        return false;

    if (m_url.isLocalFile())
        return true;

    if (m_url.protocolIsInHTTPFamily())
        return true;

    return false;
}

}

#endif // USE(GSTREAMER)
