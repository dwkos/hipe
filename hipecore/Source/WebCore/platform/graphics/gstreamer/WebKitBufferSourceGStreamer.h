/*
 * Copyright (C) 2026 General Development Systems
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef WebKitBufferSourceGStreamer_h
#define WebKitBufferSourceGStreamer_h
#if ENABLE(VIDEO) && USE(GSTREAMER)

#include <functional>
#include <gst/gst.h>

namespace WebCore {
class MediaSampleBuffer;
}

G_BEGIN_DECLS

// A GstURIHandler bin wrapping a single push-mode (SEEKABLE) appsrc, backing chunked binary media
// loading (HTMLMediaElement::beginBinaryMediaData() and friends) directly off a
// WebCore::MediaSampleBuffer -- no MediaSource/SourceBuffer, no network/ResourceHandle involved.
// Registers the "hipebuffer" URI scheme; MediaPlayerPrivateGStreamer::sourceChanged() associates
// a specific MediaSampleBuffer with the element via webKitBufferSrcSetBuffer() once playbin has
// resolved that scheme and created one, the same way it already does for WebKitMediaSrc.
//
// Unlike WebKitWebSrc (network-shaped: a seek there means "abort the current fetch, restart a new
// one with a Range header"), a seek here is simply repositioning a push cursor into data this
// process already owns -- no request/response machinery needed at all.
//
// Deliberately push-mode (GST_APP_STREAM_TYPE_SEEKABLE), not pull-mode (RANDOM_ACCESS): an earlier
// pull-mode version had appsrc's need-data callback synchronously block (inside
// MediaSampleBuffer::read()) on whichever thread happened to be pulling -- typically the demuxer's
// own streaming thread -- until enough of the buffer had arrived. That held an internal demuxer
// lock for the entire wait, and deadlocked whenever the main thread needed to touch the pipeline
// (even just a Pause) at the same moment a pull was blocked. Push mode never blocks: need-data is a
// hint, not a demand, and data is pushed asynchronously via webKitBufferSrcDataAvailable() (wired
// up in MediaPlayerPrivateGStreamer::sourceChanged() to MediaSampleBuffer's onDataAvailable
// callback) whenever MediaSampleBuffer actually has something new to give. See
// project_hipe_chunked_media_underrun_deadlock / project_hipe_pull_to_push_redesign in memory for
// the full history of why this exists.
//
// A seek only ever repositions this element's own read cursor -- it never pushes data
// synchronously from inside seek handling, deliberately, even though it could reach straight into
// MediaSampleBuffer and hand something back immediately (the data is already resident, unlike a
// real network fetch). Pushing data is left entirely to need-data (GStreamer's own signal that
// it's actually ready, issued once its streaming loop resumes after a seek's flush has fully
// settled) and to webKitBufferSrcDataAvailable(). This mirrors why WebKitWebSourceGStreamer.cpp's
// real HTTP-backed pathway never races its own flushing seeks: an HTTP response is inherently
// asynchronous, so it can never supply data from inside the seek call either. All three push
// entry points funnel through one locked, GstFlowReturn-checked implementation that never
// commits forward progress on anything but confirmed success -- see
// project_hipe_pull_to_push_redesign in memory for the root-caused bug this replaced (a push
// landing inside a still-flushing seek was silently treated as delivered, permanently losing that
// chunk and truncating every loop-restart well short of the real duration).
#define WEBKIT_TYPE_BUFFER_SRC            (webkit_buffer_src_get_type())
#define WEBKIT_BUFFER_SRC(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_BUFFER_SRC, WebKitBufferSrc))
#define WEBKIT_BUFFER_SRC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_BUFFER_SRC, WebKitBufferSrcClass))
#define WEBKIT_IS_BUFFER_SRC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_BUFFER_SRC))
#define WEBKIT_IS_BUFFER_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_BUFFER_SRC))

typedef struct _WebKitBufferSrc        WebKitBufferSrc;
typedef struct _WebKitBufferSrcClass   WebKitBufferSrcClass;
typedef struct _WebKitBufferSrcPrivate WebKitBufferSrcPrivate;

struct _WebKitBufferSrc {
    GstBin parent;

    WebKitBufferSrcPrivate* priv;
};

struct _WebKitBufferSrcClass {
    GstBinClass parentClass;
};

GType webkit_buffer_src_get_type(void);

// Associates this element with the buffer it should read from. Must be called (from the main
// thread, via MediaPlayerPrivateGStreamer::sourceChanged()) before the pipeline starts pulling
// data -- the element has nothing to read until this happens.
//
// expectedTotalSize is the real, final size of the complete file in bytes if the client told us
// upfront (HIPE_OP_SET_SRC's optional size-hint argument -- see HTMLMediaElement::
// beginBinaryMediaData()), or -1 if not. When known, it's reported to the underlying appsrc
// immediately instead of waiting for the transfer to finish, which is what lets GStreamer resolve
// its own end-of-file verification read (issued right after parsing the file's header) to EOS
// immediately instead of blocking until the whole file has arrived -- the read is for a location
// genuinely past the end of the file, which MediaSampleBuffer::read() alone has no way to
// distinguish from "hasn't arrived yet" without this hint.
void webKitBufferSrcSetBuffer(WebKitBufferSrc*, WebCore::MediaSampleBuffer*, long long expectedTotalSize);

// Called (main thread) whenever the associated MediaSampleBuffer has new data, finished, or was
// aborted -- see MediaSampleBuffer::setOnDataAvailable(). Tries to push any newly-available bytes
// into the underlying appsrc immediately, if it's currently asking for more (need-data having
// fired since the last enough-data). Safe to call speculatively at any time; a no-op if appsrc
// doesn't currently want data.
void webKitBufferSrcDataAvailable(WebKitBufferSrc*);

// Repositions the element to the start of its MediaSampleBuffer. For a pipeline that is about to be
// taken through READY and back to PAUSED (see MediaPlayerPrivateGStreamer::restartPipelineFromStart()):
// seek-data only ever fires for a real GStreamer seek, so an element that survives the state cycle
// would otherwise resume from wherever it had stopped (typically the end of the file). Call it only
// once the pipeline has reached READY, so that no streaming thread can still be pushing and advance
// the offset again behind this call's back.
void webKitBufferSrcRewind(WebKitBufferSrc*);

// Registers a callback invoked (on whichever thread a push attempt happens to run on -- the
// caller is responsible for hopping to whatever thread it actually needs, the same way
// MediaPlayerPrivateGStreamer::sourceChanged() does for this via MainThreadNotifier) whenever a
// push attempt finds genuinely nothing available at the current read position -- not a real
// end-of-stream (MediaSampleBuffer::isFinished()/isAborted() are both false), just the read/
// playback position having caught up to what's currently loaded, with more still to come. This is
// the sample-buffer equivalent of a real network source's queue2 signaling GST_QUERY_BUFFERING
// dropping below 100% -- see MediaPlayerPrivateGStreamer::sampleBufferStalled(). Only one callback
// at a time; a later call replaces the previous one. Pass nullptr to clear.
void webKitBufferSrcSetOnStalled(WebKitBufferSrc*, std::function<void()>);

G_END_DECLS

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
#endif
