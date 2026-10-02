/*
 * Copyright (C) 2007, 2009 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Collabora Ltd. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009, 2010 Igalia S.L
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

#ifndef MediaPlayerPrivateGStreamer_h
#define MediaPlayerPrivateGStreamer_h
#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GRefPtrGStreamer.h"
#include "MediaPlayerPrivateGStreamerBase.h"
#include "Timer.h"

#include <glib.h>
#include <gst/gst.h>
#include <gst/pbutils/install-plugins.h>
#include <wtf/Forward.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>

#if ENABLE(VIDEO_TRACK) && USE(GSTREAMER_MPEGTS)
#include <wtf/text/AtomicStringHash.h>
#endif

#if ENABLE(MEDIA_SOURCE)
#include "MediaSourceGStreamer.h"
#endif

typedef struct _GstBuffer GstBuffer;
typedef struct _GstMessage GstMessage;
typedef struct _GstElement GstElement;
typedef struct _GstMpegtsSection GstMpegtsSection;

namespace WebCore {

#if ENABLE(WEB_AUDIO)
class AudioSourceProvider;
class AudioSourceProviderGStreamer;
#endif

class AudioTrackPrivateGStreamer;
class InbandMetadataTextTrackPrivateGStreamer;
class InbandTextTrackPrivateGStreamer;
class MediaPlayerRequestInstallMissingPluginsCallback;
class MediaSampleBuffer;
class VideoTrackPrivateGStreamer;

class MediaPlayerPrivateGStreamer : public MediaPlayerPrivateGStreamerBase {
public:
    explicit MediaPlayerPrivateGStreamer(MediaPlayer*);
    ~MediaPlayerPrivateGStreamer();

    static void registerMediaEngine(MediaEngineRegistrar);
    void handleMessage(GstMessage*);
    void handlePluginInstallerResult(GstInstallPluginsReturn);

    bool hasVideo() const override { return m_hasVideo; }
    bool hasAudio() const override { return m_hasAudio; }

    void load(const String &url) override;
#if ENABLE(MEDIA_SOURCE)
    void load(const String& url, MediaSourcePrivateClient*) override;
#endif
#if ENABLE(MEDIA_STREAM)
    void load(MediaStreamPrivate&) override;
#endif
    void load(const String& url, RefPtr<MediaSampleBuffer>&&, long long expectedTotalSize) override;
    void commitLoad();
    void cancelLoad() override;

    void prepareToPlay() override;
    void play() override;
    void pause() override;

    bool paused() const override;
    bool seeking() const override;

    float duration() const override;
    float currentTime() const override;
    void seek(float) override;

    void setRate(float) override;
    double rate() const override;
    void setPreservesPitch(bool) override;

    void setPreload(MediaPlayer::Preload) override;
    void fillTimerFired();

    std::unique_ptr<PlatformTimeRanges> buffered() const override;
    float maxTimeSeekable() const override;
    bool didLoadingProgress() const override;
    unsigned long long totalBytes() const override;
    float maxTimeLoaded() const override;

    void loadStateChanged();
    void timeChanged();
    void didEnd();
    void durationChanged();
    void loadingFailed(MediaPlayer::NetworkState);

    // The pipeline could not perform a seek to `time` (qtdemux, for one, refuses every seek of a
    // fragmented mp4 fed in push mode). Never leaves the caller waiting on it: see the definition.
    void seekRefused(float time);
    // Seeks to the beginning without gst_element_seek(), by taking the pipeline through READY and
    // back to PAUSED with the source rewound. Returns false if it could not be attempted.
    bool restartPipelineFromStart();
    // Safeguard for looped playback, independent of why a loop might spin: called for every
    // end-of-stream of a looping element; returns true (after ending playback with an error) once
    // end-of-stream keeps arriving without the media ever being played through.
    bool loopIsRunningAway();

    void sourceChanged();
    void sampleBufferDataAvailable();
    void sampleBufferStalled();
    GstElement* audioSink() const override;

    void simulateAudioInterruption() override;

    bool changePipelineState(GstState);

#if ENABLE(WEB_AUDIO)
    AudioSourceProvider* audioSourceProvider() override { return reinterpret_cast<AudioSourceProvider*>(m_audioSourceProvider.get()); }
#endif

private:
    static void getSupportedTypes(HashSet<String, ASCIICaseInsensitiveHash>&);
    static MediaPlayer::SupportsType supportsType(const MediaEngineSupportParameters&);

    static bool isAvailable();

    WeakPtr<MediaPlayerPrivateGStreamer> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(); }

    GstElement* createAudioSink() override;

    float playbackPosition() const;

    void cacheDuration();
    void updateStates();
    void asyncStateChangeDone();
    void requestBufferingPipelineState(GstState);
    bool shouldPlayGivenBufferedAhead(float bufferedAheadSeconds, bool currentlyPlaying) const;

    void createGSTPlayBin();

    bool loadNextLocation();
    void mediaLocationChanged(GstMessage*);

    void setDownloadBuffering();
    void processBufferingStats(GstMessage*);
#if ENABLE(VIDEO_TRACK) && USE(GSTREAMER_MPEGTS)
    void processMpegTsSection(GstMpegtsSection*);
#endif
#if ENABLE(VIDEO_TRACK)
    void processTableOfContents(GstMessage*);
    void processTableOfContentsEntry(GstTocEntry*, GstTocEntry* parent);
#endif
    bool doSeek(gint64 position, float rate, GstSeekFlags seekType);
    void updatePlaybackRate();

    String engineDescription() const override { return "GStreamer"; }
    bool isLiveStream() const override { return m_isStreaming; }
    bool didPassCORSAccessCheck() const override;
    bool canSaveMediaData() const override;

#if ENABLE(MEDIA_SOURCE)
    // TODO: Implement
    unsigned long totalVideoFrames() override { return 0; }
    unsigned long droppedVideoFrames() override { return 0; }
    unsigned long corruptedVideoFrames() override { return 0; }
    MediaTime totalFrameDelay() override { return MediaTime::zeroTime(); }
#endif

    void readyTimerFired();

    void notifyPlayerOfVideo();
    void notifyPlayerOfVideoCaps();
    void notifyPlayerOfAudio();

#if ENABLE(VIDEO_TRACK)
    void notifyPlayerOfText();
    void newTextSample();
#endif

    void setAudioStreamProperties(GObject*);

    static void setAudioStreamPropertiesCallback(MediaPlayerPrivateGStreamer*, GObject*);

    static void sourceChangedCallback(MediaPlayerPrivateGStreamer*);
    static void videoChangedCallback(MediaPlayerPrivateGStreamer*);
    static void videoSinkCapsChangedCallback(MediaPlayerPrivateGStreamer*);
    static void audioChangedCallback(MediaPlayerPrivateGStreamer*);
#if ENABLE(VIDEO_TRACK)
    static void textChangedCallback(MediaPlayerPrivateGStreamer*);
    static GstFlowReturn newTextSampleCallback(MediaPlayerPrivateGStreamer*);
#endif

    WeakPtrFactory<MediaPlayerPrivateGStreamer> m_weakPtrFactory;

    GRefPtr<GstElement> m_source;
#if ENABLE(VIDEO_TRACK)
    GRefPtr<GstElement> m_textAppSink;
    GRefPtr<GstPad> m_textAppSinkPad;
#endif
    float m_seekTime;
    bool m_changingRate;
    bool m_isEndReached;
    mutable bool m_isStreaming;
    GstStructure* m_mediaLocations;
    int m_mediaLocationCurrentIndex;
    bool m_resetPipeline;
    bool m_paused;
    bool m_playbackRatePause;
    bool m_seeking;
    bool m_seekIsPending;
    // True from restartPipelineFromStart() until that restart's async state change completes. The
    // restart itself is the seek (a freshly started pipeline is already at 0), so completion goes
    // through the ordinary "seek finished" path in asyncStateChangeDone(), with no
    // gst_element_seek() -- which would be refused all over again. Also keeps updateStates() from
    // discarding the known duration while passing through READY.
    bool m_restartingFromStart { false };
    // loopIsRunningAway()'s bookkeeping: when the previous end-of-stream of a looping element
    // arrived (monotonic seconds; 0 = none yet), and how many consecutive ones followed it too soon.
    double m_lastLoopEndTime { 0 };
    unsigned m_rapidLoopEnds { 0 };
    float m_timeOfOverlappingSeek;
    bool m_canFallBackToLastFinishedSeekPosition;
    bool m_buffering;
    float m_playbackRate;
    float m_lastPlaybackRate;
    bool m_errorOccured;
    mutable gfloat m_mediaDuration;
    bool m_downloadFinished;
    Timer m_fillTimer;
    float m_maxTimeLoaded;
    int m_bufferingPercentage;
    MediaPlayer::Preload m_preload;
    bool m_delayingLoad;
    bool m_mediaDurationKnown;
    mutable float m_maxTimeLoadedAtLastDidLoadingProgress;
    bool m_volumeAndMuteInitialized;
    bool m_hasVideo;
    bool m_hasAudio;
    RunLoop::Timer<MediaPlayerPrivateGStreamer> m_readyTimerHandler;
    mutable unsigned long long m_totalBytes;
    URL m_url;
    bool m_preservesPitch;
#if ENABLE(WEB_AUDIO)
    std::unique_ptr<AudioSourceProviderGStreamer> m_audioSourceProvider;
#endif
    GstState m_requestedState;
    GRefPtr<GstElement> m_autoAudioSink;
    RefPtr<MediaPlayerRequestInstallMissingPluginsCallback> m_missingPluginsCallback;
#if ENABLE(VIDEO_TRACK)
    Vector<RefPtr<AudioTrackPrivateGStreamer>> m_audioTracks;
    Vector<RefPtr<InbandTextTrackPrivateGStreamer>> m_textTracks;
    Vector<RefPtr<VideoTrackPrivateGStreamer>> m_videoTracks;
    RefPtr<InbandMetadataTextTrackPrivateGStreamer> m_chaptersTrack;
#endif
#if ENABLE(VIDEO_TRACK) && USE(GSTREAMER_MPEGTS)
    HashMap<AtomicString, RefPtr<InbandMetadataTextTrackPrivateGStreamer>> m_metadataTracks;
#endif
#if ENABLE(MEDIA_SOURCE)
    RefPtr<MediaSourcePrivateClient> m_mediaSource;
    bool isMediaSource() const { return m_mediaSource; }
#else
    bool isMediaSource() const { return false; }
#endif

    // See MediaSampleBuffer.h and WebKitBufferSourceGStreamer.h -- chunked binary media loading's
    // replacement for the MediaSource path above. Kept entirely separate from m_mediaSource: this
    // is not MediaSource-backed, so isMediaSource() (and everything gated on it, such as the
    // MSE-doSeek()-is-a-no-op handling elsewhere in this class) does not apply to it.
    RefPtr<MediaSampleBuffer> m_sampleBuffer;
    bool isSampleBufferBacked() const { return m_sampleBuffer; }
    // The real, final size of the complete file in bytes if the client told us upfront, or -1 if
    // not -- see HTMLMediaElement::beginBinaryMediaData() and WebKitBufferSourceGStreamer.h.
    long long m_expectedTotalSize { -1 };
    // Shared by buffered() and maxTimeLoaded(): how much of the media's duration is covered by
    // bytes delivered so far, for a sample-buffer-backed load with a known total size. 0 if not
    // applicable (no sample buffer, no size hint, or duration not yet known).
    float sampleBufferLoadedTime() const;
    // monotonicallyIncreasingTime() at the last pause-for-buffering resume (sampleBufferDataAvailable()),
    // 0 if never. See sampleBufferStalled()'s own cooldown check for why this exists -- pausing
    // again too soon after a resume, before GStreamer's own async PLAYING transition has actually
    // settled, was live-confirmed to leave the pipeline permanently stuck straddling states
    // (gst_element_get_state() reporting current=PLAYING with pending=PAUSED indefinitely,
    // rejecting every subsequent state-change request as "already there").
    double m_lastBufferingResumeTime { 0 };
    // The position playbackPosition() reports while m_buffering is true, instead of a live
    // GST_QUERY_POSITION -- see playbackPosition()'s own comment for why the live query can't be
    // trusted during a sample-buffer-backed buffering pause. Set once, in sampleBufferStalled(),
    // at the moment buffering starts; irrelevant while m_buffering is false.
    float m_bufferingHoldPosition { 0 };
    // True after play() (regardless of pipeline/rate state), false after pause() -- genuine DOM-level
    // user intent, deliberately NOT the same thing as m_paused above. m_paused is a raw mirror of the
    // live GST pipeline state, rewritten unconditionally by updateStates()'s own state-tracking switch
    // on *every* call (`else m_paused = true;` whenever state != PLAYING) -- including the calls the
    // buffering-pause machinery itself triggers via changePipelineState(PAUSED). That means m_paused
    // goes true the moment a buffering-pause takes effect, with no way to tell "genuinely paused by
    // the user" apart from "paused by us, for buffering" -- live-confirmed via GST_DEBUG trace: a
    // buffering-pause near end of playback left m_paused true afterward, so
    // sampleBufferDataAvailable()'s resume check (which used to test `!m_paused`) silently refused to
    // ever resume once no further chunks arrived to retry it -- permanently stuck "paused" with
    // buffering complete. This member is the fix: set only by real play()/pause() calls, never
    // touched by updateStates()/changePipelineState(), so a buffering-pause can never masquerade as
    // user intent.
    bool m_userRequestedPlay { false };
    // A buffering-driven pipeline state request that couldn't be issued immediately because a
    // different transition was already in flight -- see requestBufferingPipelineState()'s own
    // comment. GST_STATE_VOID_PENDING means nothing queued.
    GstState m_pendingBufferingPipelineState { GST_STATE_VOID_PENDING };
};
}

#endif // USE(GSTREAMER)
#endif
