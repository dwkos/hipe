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

#ifndef MediaSampleBuffer_h
#define MediaSampleBuffer_h

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include <functional>
#include <wtf/Lock.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

// A growable, thread-safe byte store backing chunked binary media loading -- see
// HTMLMediaElement::beginBinaryMediaData()/appendBinaryMediaData()/finishBinaryMediaData() and
// WebKitBufferSrc (WebKitBufferSourceGStreamer.h). The main thread appends chunks as they arrive
// over hiped's protocol, pushed straight into WebKitBufferSrc's underlying appsrc (push/SEEKABLE
// scheduling, not pull) -- nothing ever blocks waiting for this buffer to grow. An earlier
// pull-mode design (a downstream GStreamer thread synchronously blocking inside read() while
// arrival caught up) deadlocked whenever the main thread needed to touch the pipeline at the same
// moment a read was blocked; see project_hipe_chunked_media_underrun_deadlock in memory for the
// full history. This class no longer has any blocking entry point at all -- readAvailable() always
// returns immediately with whatever's there, never less than requested unless that's genuinely all
// there currently is.
//
// Stored as a Vector of arrival-order chunks (each an independent heap allocation) rather than
// one contiguous growing buffer, so that regrowing the outer index only moves small chunk handles,
// never copies the actual byte payloads -- avoiding a reallocation-copy spike for large (up to the
// several-hundred-MB modest upload ceiling) files.
class MediaSampleBuffer : public RefCounted<MediaSampleBuffer> {
public:
    static Ref<MediaSampleBuffer> create() { return adoptRef(*new MediaSampleBuffer); }

    // Called from the main thread as chunks arrive. Safe to call after abort() (silently ignored).
    // Invokes the onDataAvailable callback (if set) after updating state, so a push-mode consumer
    // can try to push the newly-arrived bytes immediately rather than waiting for its own next
    // unrelated wakeup.
    void append(const uint8_t* data, size_t length);

    // Called from the main thread once the transfer is complete. Also invokes onDataAvailable, so
    // a consumer that's out of bytes to push right now (readAvailable() returning 0) and was
    // waiting to find out whether that's temporary or the real end can react immediately.
    void markFinished();

    // Called from the main thread when this buffer is being discarded before finishing normally (a
    // fresh load replacing it, or destruction of the element that owns it). Also invokes
    // onDataAvailable so a consumer waiting on more data learns to give up.  Idempotent; safe to
    // call more than once.
    void abort();

    // Reads up to maxLength bytes starting at offset into dest, returning the number of bytes
    // actually copied. Never blocks: returns however many contiguous bytes are available right now
    // starting at offset (which may be less than maxLength, or 0 if offset is at or past
    // sizeSoFar()) -- the caller is expected to check isFinished()/isAborted() separately to tell
    // "wait for more" apart from "this is genuinely the end."
    size_t readAvailable(uint8_t* dest, uint64_t offset, size_t maxLength);

    // True iff every byte of [offset, offset + length) has already been appended.
    bool hasDataAvailable(uint64_t offset, uint64_t length);

    // Total bytes appended so far. Safe to call from either thread.
    uint64_t sizeSoFar();

    // True once markFinished() has been called (the total size in sizeSoFar() is then final).
    bool isFinished();

    // True once abort() has been called -- no further bytes will ever be available, regardless of
    // offset.
    bool isAborted();

    // Registers a callback invoked (on the main thread, synchronously, with no lock held) from
    // append()/markFinished()/abort() -- the hook a push-mode consumer uses to know when it's
    // worth trying readAvailable() again instead of only reacting to its own need-data signal.
    // Only one callback at a time; a later call replaces the previous one. Pass nullptr to clear.
    void setOnDataAvailable(std::function<void()>);

private:
    MediaSampleBuffer() { }

    void notifyDataAvailable();

    Lock m_lock;
    Vector<Vector<uint8_t>> m_chunks;
    Vector<uint64_t> m_chunkStartOffsets;
    uint64_t m_totalSize { 0 };
    bool m_finished { false };
    bool m_aborted { false };
    std::function<void()> m_onDataAvailable;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)

#endif // MediaSampleBuffer_h
