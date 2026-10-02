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

#include "config.h"
#include "WebKitBufferSourceGStreamer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include "GStreamerUtilities.h"
#include "MediaSampleBuffer.h"
#include <gst/app/gstappsrc.h>
#include <gst/pbutils/missing-plugins.h>
#include <wtf/Lock.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;

#define WEBKIT_BUFFER_SRC_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_BUFFER_SRC, WebKitBufferSrcPrivate))

struct _WebKitBufferSrcPrivate {
    GstAppSrc* appsrc;
    GstPad* srcpad;
    gchar* uri;
    RefPtr<MediaSampleBuffer> buffer;
    guint64 offset;
    bool reportedFinalSize;
    // True from a need-data callback until the matching enough-data (or until a push attempt
    // finds nothing currently available) -- push-mode's own flow-control signal for "appsrc wants
    // more right now", separate from whether MediaSampleBuffer actually has anything to give yet.
    bool needsData;
    // True for the duration of one webKitBufferSrcTryPushData() call, including every foreign
    // GStreamer call it makes (gst_app_src_push_buffer()/set_size()/end_of_stream()). Those calls
    // can synchronously re-invoke need-data -- and therefore webKitBufferSrcNeedData(), and
    // therefore this same function, reentrantly, on the same thread, before the outer call has
    // returned -- whenever a just-pushed buffer still leaves appsrc wanting more. `isPushing`
    // makes a reentrant call an immediate no-op instead of attempting a second, overlapping push
    // for the same not-yet-advanced offset. Confirmed live: this reentrant path isn't
    // hypothetical, it's the normal case for some content shapes (a real audio+video file
    // triggered it immediately; a smaller audio-only file happened not to).
    bool isPushing;
    // See webKitBufferSrcSetOnStalled()'s own comment (WebKitBufferSourceGStreamer.h).
    std::function<void()> onStalled;
    // Serializes offset/needsData/isPushing/onStalled above -- they're touched both from
    // GStreamer's own streaming thread (need-data/seek-data, invoked synchronously from inside
    // GstBaseSrc's pull loop) and from the main thread (webKitBufferSrcDataAvailable()/
    // webKitBufferSrcSetOnStalled(), invoked via MediaSampleBuffer::notifyDataAvailable() whenever
    // appendBinaryMediaData() runs, or once at pipeline setup). Never held while calling into
    // appsrc/GStreamer -- see webKitBufferSrcTryPushData()'s own comment for why (a real,
    // live-confirmed self-deadlock otherwise, from the reentrancy above).
    Lock lock;

    _WebKitBufferSrcPrivate()
        : appsrc(nullptr)
        , srcpad(nullptr)
        , uri(nullptr)
        , offset(0)
        , reportedFinalSize(false)
        , needsData(false)
        , isPushing(false)
    {
    }
};

static GstStaticPadTemplate srcTemplate = GST_STATIC_PAD_TEMPLATE("src",
                                                                    GST_PAD_SRC,
                                                                    GST_PAD_ALWAYS,
                                                                    GST_STATIC_CAPS_ANY);

GST_DEBUG_CATEGORY_STATIC(webkit_buffer_src_debug);
#define GST_CAT_DEFAULT webkit_buffer_src_debug

static void webKitBufferSrcUriHandlerInit(gpointer gIface, gpointer);
static void webKitBufferSrcFinalize(GObject*);
static GstStateChangeReturn webKitBufferSrcChangeState(GstElement*, GstStateChange);
static gboolean webKitBufferSrcQueryWithParent(GstPad*, GstObject*, GstQuery*);

static void webKitBufferSrcNeedData(WebKitBufferSrc*, guint length);
static void webKitBufferSrcEnoughData(WebKitBufferSrc*);
static gboolean webKitBufferSrcSeek(WebKitBufferSrc*, guint64 offset);
static void webKitBufferSrcTryPushData(WebKitBufferSrc*);

static GstAppSrcCallbacks appsrcCallbacks = {
    // need_data
    [](GstAppSrc*, guint length, gpointer userData) {
        webKitBufferSrcNeedData(WEBKIT_BUFFER_SRC(userData), length);
    },
    // enough_data
    [](GstAppSrc*, gpointer userData) {
        webKitBufferSrcEnoughData(WEBKIT_BUFFER_SRC(userData));
    },
    // seek_data
    [](GstAppSrc*, guint64 offset, gpointer userData) -> gboolean {
        return webKitBufferSrcSeek(WEBKIT_BUFFER_SRC(userData), offset);
    },
    { nullptr }
};

#define webkit_buffer_src_parent_class parent_class
#define WEBKIT_BUFFER_SRC_CATEGORY_INIT GST_DEBUG_CATEGORY_INIT(webkit_buffer_src_debug, "webkitbuffersrc", 0, "buffersrc element");
G_DEFINE_TYPE_WITH_CODE(WebKitBufferSrc, webkit_buffer_src, GST_TYPE_BIN,
                         G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, webKitBufferSrcUriHandlerInit);
                         WEBKIT_BUFFER_SRC_CATEGORY_INIT);

static void webkit_buffer_src_class_init(WebKitBufferSrcClass* klass)
{
    GObjectClass* oklass = G_OBJECT_CLASS(klass);
    GstElementClass* eklass = GST_ELEMENT_CLASS(klass);

    oklass->finalize = webKitBufferSrcFinalize;

    gst_element_class_add_pad_template(eklass, gst_static_pad_template_get(&srcTemplate));
    gst_element_class_set_metadata(eklass, "WebKit buffer source element", "Source",
        "Feeds a growing, hiped-owned in-memory buffer into a playback pipeline",
        "General Development Systems");

    eklass->change_state = webKitBufferSrcChangeState;

    g_type_class_add_private(klass, sizeof(WebKitBufferSrcPrivate));
}

static void webkit_buffer_src_init(WebKitBufferSrc* src)
{
    WebKitBufferSrcPrivate* priv = WEBKIT_BUFFER_SRC_GET_PRIVATE(src);
    src->priv = priv;
    new (priv) WebKitBufferSrcPrivate();

    priv->appsrc = GST_APP_SRC(gst_element_factory_make("appsrc", nullptr));
    if (!priv->appsrc) {
        GST_ERROR_OBJECT(src, "Failed to create appsrc");
        return;
    }

    gst_bin_add(GST_BIN(src), GST_ELEMENT(priv->appsrc));

    GRefPtr<GstPad> targetPad = adoptGRef(gst_element_get_static_pad(GST_ELEMENT(priv->appsrc), "src"));
    priv->srcpad = webkitGstGhostPadFromStaticTemplate(&srcTemplate, "src", targetPad.get());
    gst_element_add_pad(GST_ELEMENT(src), priv->srcpad);

    GST_OBJECT_FLAG_SET(priv->srcpad, GST_PAD_FLAG_NEED_PARENT);
    gst_pad_set_query_function(priv->srcpad, webKitBufferSrcQueryWithParent);

    gst_app_src_set_callbacks(priv->appsrc, &appsrcCallbacks, src, nullptr);
    gst_app_src_set_emit_signals(priv->appsrc, FALSE);
    // Unlike a network source, every byte we could ever serve either already exists in the
    // buffer or will exist once more of it arrives -- there is no "unsupported seek target",
    // only "not there yet." SEEKABLE (push-mode, with seek-data support) rather than
    // RANDOM_ACCESS (pull-mode) -- see this element's own header comment for why pull-mode was
    // abandoned. need-data/enough-data are flow-control hints in this mode, never a synchronous
    // demand for an exact byte range the way a pull-mode getrange() call would be.
    gst_app_src_set_stream_type(priv->appsrc, GST_APP_STREAM_TYPE_SEEKABLE);
    gst_app_src_set_caps(priv->appsrc, nullptr);
    // Unknown until the transfer finishes -- see webKitBufferSrcNeedData()'s size reporting.
    gst_app_src_set_size(priv->appsrc, -1);
}

static void webKitBufferSrcFinalize(GObject* object)
{
    WebKitBufferSrc* src = WEBKIT_BUFFER_SRC(object);
    WebKitBufferSrcPrivate* priv = src->priv;

    g_free(priv->uri);
    priv->~WebKitBufferSrcPrivate();

    GST_CALL_PARENT(G_OBJECT_CLASS, finalize, (object));
}

static GstStateChangeReturn webKitBufferSrcChangeState(GstElement* element, GstStateChange transition)
{
    WebKitBufferSrc* src = WEBKIT_BUFFER_SRC(element);
    WebKitBufferSrcPrivate* priv = src->priv;

    if (transition == GST_STATE_CHANGE_NULL_TO_READY && !priv->appsrc) {
        gst_element_post_message(element, gst_missing_element_message_new(element, "appsrc"));
        GST_ELEMENT_ERROR(src, CORE, MISSING_PLUGIN, (nullptr), ("no appsrc"));
        return GST_STATE_CHANGE_FAILURE;
    }

    return GST_ELEMENT_CLASS(parent_class)->change_state(element, transition);
}

static gboolean webKitBufferSrcQueryWithParent(GstPad* pad, GstObject* parent, GstQuery* query)
{
    if (GST_QUERY_TYPE(query) == GST_QUERY_URI) {
        WebKitBufferSrc* src = WEBKIT_BUFFER_SRC(GST_ELEMENT(parent));
        gst_query_set_uri(query, src->priv->uri);
        return TRUE;
    }

    GRefPtr<GstPad> target = adoptGRef(gst_ghost_pad_get_target(GST_GHOST_PAD_CAST(pad)));
    return target ? gst_pad_query(target.get(), query) : FALSE;
}

static void webKitBufferSrcTryPushData(WebKitBufferSrc* src)
{
    WebKitBufferSrcPrivate* priv = src->priv;

    // Pushes at most ONE bounded chunk per call, deliberately not a loop pushing everything
    // MediaSampleBuffer currently has. appsrc's own internal queue/flow-control (enough-data) is
    // designed around being fed incrementally, one push at a time, with appsrc itself deciding
    // when it's ready for the next one via a fresh need-data call issued from its own scheduling --
    // it is not designed to have its entire backlog handed to it synchronously in one burst. That
    // matters a lot here specifically because, unlike a live network source, MediaSampleBuffer can
    // easily already hold the *entire rest of the file* at once (e.g. right after a loop-restart
    // seek back to a position whose data arrived during an earlier play-through) -- a "keep
    // pushing while needsData is true" loop would dump everything in a single synchronous call
    // before enough-data ever has a chance to fire and throttle it, since nothing yields back to
    // GStreamer's own scheduling (letting downstream actually drain the queue) in between pushes.
    // Confirmed live: that was the direct cause of a real bug, an infinite seek-to-0 retry loop on
    // every loop-restart -- see project_hipe_pull_to_push_redesign in memory.
    guint64 offset;
    bool shouldReportFinalSize = false;
    gint64 finalSize = 0;
    {
        LockHolder locker(priv->lock);
        if (!priv->buffer || !priv->needsData || priv->isPushing)
            return;
        priv->isPushing = true;
        offset = priv->offset;
        // Report the real total size the first chance we get after the transfer finishes --
        // until then this is legitimately unknown, matching how WebKitWebSrc reports -1 for a
        // response with no Content-Length.
        if (!priv->reportedFinalSize && priv->buffer->isFinished()) {
            priv->reportedFinalSize = true;
            shouldReportFinalSize = true;
            finalSize = static_cast<gint64>(priv->buffer->sizeSoFar());
        }
    }

    // From here on, `priv->lock` is deliberately NOT held. gst_app_src_set_size()/
    // push_buffer()/end_of_stream() below can synchronously re-invoke need-data (and therefore
    // webKitBufferSrcNeedData(), and therefore this same function, reentrantly, on the same
    // thread) before returning -- confirmed live, and not a rare edge case: it happened on the
    // very first push of a real audio+video file, self-deadlocking hiped's entire streaming
    // thread solid (a smaller audio-only file happened not to trigger it in testing, which is why
    // an earlier version of this fix looked correct against that repro alone and wasn't). Holding
    // a non-reentrant lock across any of these calls deadlocks the instant that reentrant path is
    // taken. `isPushing` (set above, under lock, before any of these calls) makes a reentrant
    // call an immediate no-op instead of attempting a second, overlapping push for the same
    // not-yet-advanced offset -- the still-in-progress outer call remains the only one entitled
    // to advance `offset` or push data this cycle, and a subsequent genuine need-data picks up
    // from wherever it left off.
    if (shouldReportFinalSize)
        gst_app_src_set_size(priv->appsrc, finalSize);

    static const guint pushChunkSize = 32768;
    GstBuffer* gstBuffer = gst_buffer_new_and_alloc(pushChunkSize);
    GstMapInfo mapInfo;
    gst_buffer_map(gstBuffer, &mapInfo, GST_MAP_WRITE);
    size_t bytesRead = priv->buffer->readAvailable(mapInfo.data, offset, pushChunkSize);
    gst_buffer_unmap(gstBuffer, &mapInfo);

    if (!bytesRead) {
        gst_buffer_unref(gstBuffer);
        // Real EOF (finished, nothing left at/past the current offset) or an aborted load --
        // either way, no more data will ever come. Otherwise: genuinely nothing available yet at
        // this offset -- the read/playback position has caught up to what's currently loaded,
        // with more still to come. We'll be called again either via a fresh need-data or via
        // webKitBufferSrcDataAvailable() once MediaSampleBuffer actually has more to give; in the
        // meantime, tell onStalled (if set) so the player can pause for real buffering instead of
        // letting the pipeline clock keep advancing past content that was never actually rendered
        // -- see webKitBufferSrcSetOnStalled()'s own comment.
        bool shouldEnd = priv->buffer->isFinished() || priv->buffer->isAborted();
        std::function<void()> stalledCallback;
        {
            LockHolder locker(priv->lock);
            priv->isPushing = false;
            if (!shouldEnd)
                stalledCallback = priv->onStalled;
        }
        if (shouldEnd)
            gst_app_src_end_of_stream(priv->appsrc);
        else if (stalledCallback)
            stalledCallback();
        return;
    }

    gst_buffer_set_size(gstBuffer, bytesRead);
    // Stamp the real byte range this buffer represents, matching
    // WebKitWebSourceGStreamer.cpp's StreamingClient::didReceiveData() -- gives GStreamer's own
    // machinery an authoritative way to notice a gap/discontinuity even if this element's own
    // offset bookkeeping is ever wrong again, rather than silently trusting delivery order.
    GST_BUFFER_OFFSET(gstBuffer) = offset;
    GST_BUFFER_OFFSET_END(gstBuffer) = offset + bytesRead;

    GstFlowReturn pushRet = gst_app_src_push_buffer(priv->appsrc, gstBuffer);

    bool shouldError = false;
    {
        LockHolder locker(priv->lock);
        priv->isPushing = false;
        if (pushRet == GST_FLOW_OK) {
            // Only commit forward progress on confirmed success, and only if nothing repositioned
            // `offset` while this push was in flight (unlocked, deliberately, per above) -- a
            // concurrent seek's own value must win, not this now-stale push's result. A push
            // landing while a flushing seek is still settling downstream reliably returns
            // GST_FLOW_FLUSHING -- silently advancing `offset` regardless of that (the earlier,
            // buggy version of this code) meant that exact chunk was never actually delivered but
            // was never retried either, a real gap that qtdemux would eventually hit and
            // misreport as legitimate end-of-stream well short of the true duration, on every
            // single loop-restart. Root-caused live via GST_DEBUG tracing (gst_app_src_push_buffer
            // returning -3/GST_FLOW_FLUSHING at the exact offset where playback later truncated)
            // -- see project_hipe_pull_to_push_redesign in memory.
            if (priv->offset == offset)
                priv->offset = offset + bytesRead;
        } else if (pushRet != GST_FLOW_FLUSHING) {
            // GST_FLOW_FLUSHING is expected and transient, not an error: webKitBufferSrcSeek()
            // deliberately never pushes synchronously (see its own comment), but a push triggered
            // from webKitBufferSrcDataAvailable() -- new bytes arriving on the main thread,
            // unrelated to GStreamer's own scheduling -- can still land while a flushing seek is
            // mid-flight. `offset` is left untouched above, so this exact chunk gets retried
            // automatically: by the next need-data GStreamer issues once the flush actually
            // completes (its streaming loop always resumes and asks again, since the flush empties
            // appsrc's internal queue), or by the next data-available call. Anything else is a
            // genuine, unexpected failure, matching how WebKitWebSourceGStreamer.cpp's own push
            // loop treats it.
            shouldError = true;
        }
    }
    if (shouldError)
        GST_ELEMENT_ERROR(src, CORE, FAILED, (nullptr), ("gst_app_src_push_buffer failed: %d", pushRet));
}

static void webKitBufferSrcNeedData(WebKitBufferSrc* src, guint)
{
    {
        LockHolder locker(src->priv->lock);
        src->priv->needsData = true;
    }
    webKitBufferSrcTryPushData(src);
}

static void webKitBufferSrcEnoughData(WebKitBufferSrc* src)
{
    LockHolder locker(src->priv->lock);
    src->priv->needsData = false;
}

static gboolean webKitBufferSrcSeek(WebKitBufferSrc* src, guint64 offset)
{
    // Always accepted: every offset either already has data behind it or will once more of the
    // buffer arrives (or, sequentially, never will if it's genuinely past the eventual end --
    // which webKitBufferSrcTryPushData() resolves as an ordinary EOS once reached, not a seek
    // failure). There is no request/response round trip to restart here, unlike a network source.
    //
    // Deliberately does NOT push data synchronously from here, unlike an earlier version of this
    // code. seek-data fires as part of GstBaseSrc's own flushing-seek handling, before the flush
    // has necessarily finished settling downstream -- a push attempted from inside that window
    // reliably raced it (see webKitBufferSrcTryPushData()'s GST_FLOW_FLUSHING handling for the
    // full story of the bug this caused). Simply repositioning `offset` here and trusting
    // GstBaseSrc to call need-data again once its own streaming loop actually resumes post-flush
    // (which it always does, since the flush empties appsrc's internal queue) avoids the race
    // structurally instead of detecting or working around it after the fact -- matching why
    // WebKitWebSourceGStreamer.cpp's real HTTP-backed pathway never hits this class of bug
    // either: it never supplies data synchronously from inside seek handling, only in response
    // to genuinely asynchronous readiness (an HTTP response arriving).
    LockHolder locker(src->priv->lock);
    src->priv->offset = offset;
    return TRUE;
}

void webKitBufferSrcDataAvailable(WebKitBufferSrc* src)
{
    webKitBufferSrcTryPushData(src);
}

void webKitBufferSrcRewind(WebKitBufferSrc* src)
{
    // Same bookkeeping webKitBufferSrcSeek() does for a real seek-data request, for the case where
    // no GStreamer seek is involved (see the header comment). The lock is enough here: the caller
    // guarantees the pipeline is in READY, so nothing else is pushing.
    LockHolder locker(src->priv->lock);
    src->priv->offset = 0;
}

void webKitBufferSrcSetOnStalled(WebKitBufferSrc* src, std::function<void()> callback)
{
    LockHolder locker(src->priv->lock);
    src->priv->onStalled = WTFMove(callback);
}

void webKitBufferSrcSetBuffer(WebKitBufferSrc* src, MediaSampleBuffer* buffer, long long expectedTotalSize)
{
    src->priv->buffer = buffer;

    // If the client told us the real final size upfront (HIPE_OP_SET_SRC's optional size-hint
    // argument), report it to appsrc immediately instead of waiting for the transfer to finish.
    // Under the earlier pull-mode design this is what let GstBaseSrc's own bounds check
    // (gst_base_src_update_length()) resolve qtdemux's post-moov end-of-file verification pull
    // straight to EOS instead of blocking for the rest of the transfer -- see
    // project_hipe_sequential_media_loading_scope in memory. Kept for push mode too (still lets
    // GST_QUERY_DURATION/GST_QUERY_SEEKING resolve correctly from the real total rather than
    // "unknown" throughout the transfer), but the *exact* mechanism this was originally fixing
    // (a specific pull-mode gst_pad_pull_range() at the past-the-end offset) may not recur in the
    // same shape now that qtdemux is being fed via push/seek-event scheduling instead -- needs live
    // verification, not an assumption, once this change is otherwise working (see
    // project_hipe_pull_to_push_redesign). See webKitBufferSrcTryPushData() above for the
    // equivalent, unconditional fallback once the transfer actually finishes, which covers loads
    // that don't provide this hint.
    if (expectedTotalSize >= 0) {
        gst_app_src_set_size(src->priv->appsrc, expectedTotalSize);
        src->priv->reportedFinalSize = true;
    }
}

// uri handler interface

static GstURIType webKitBufferSrcUriGetType(GType)
{
    return GST_URI_SRC;
}

static const gchar* const* webKitBufferSrcGetProtocols(GType)
{
    static const char* protocols[] = { "hipebuffer", nullptr };
    return protocols;
}

static gchar* webKitBufferSrcGetUri(GstURIHandler* handler)
{
    return g_strdup(WEBKIT_BUFFER_SRC(handler)->priv->uri);
}

static gboolean webKitBufferSrcSetUri(GstURIHandler* handler, const gchar* uri, GError** error)
{
    WebKitBufferSrc* src = WEBKIT_BUFFER_SRC(handler);
    WebKitBufferSrcPrivate* priv = src->priv;

    if (GST_STATE(src) >= GST_STATE_PAUSED) {
        GST_ERROR_OBJECT(src, "URI can only be set in states < PAUSED");
        return FALSE;
    }

    g_free(priv->uri);
    priv->uri = nullptr;
    if (!uri)
        return TRUE;

    if (!g_str_has_prefix(uri, "hipebuffer:")) {
        g_set_error(error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI, "Invalid URI '%s'", uri);
        return FALSE;
    }

    priv->uri = g_strdup(uri);
    return TRUE;
}

static void webKitBufferSrcUriHandlerInit(gpointer gIface, gpointer)
{
    GstURIHandlerInterface* iface = static_cast<GstURIHandlerInterface*>(gIface);
    iface->get_type = webKitBufferSrcUriGetType;
    iface->get_protocols = webKitBufferSrcGetProtocols;
    iface->get_uri = webKitBufferSrcGetUri;
    iface->set_uri = webKitBufferSrcSetUri;
}

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
