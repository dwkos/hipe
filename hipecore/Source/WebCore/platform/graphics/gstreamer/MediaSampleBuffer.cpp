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
#include "MediaSampleBuffer.h"

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include <algorithm>
#include <string.h>

namespace WebCore {

void MediaSampleBuffer::append(const uint8_t* data, size_t length)
{
    if (!length)
        return;

    {
        LockHolder locker(m_lock);
        if (m_aborted || m_finished)
            return;

        Vector<uint8_t> chunk;
        chunk.append(data, length);

        m_chunkStartOffsets.append(m_totalSize);
        m_totalSize += length;
        m_chunks.append(WTFMove(chunk));
    }

    notifyDataAvailable();
}

void MediaSampleBuffer::markFinished()
{
    {
        LockHolder locker(m_lock);
        m_finished = true;
    }
    notifyDataAvailable();
}

void MediaSampleBuffer::abort()
{
    {
        LockHolder locker(m_lock);
        m_aborted = true;
    }
    notifyDataAvailable();
}

size_t MediaSampleBuffer::readAvailable(uint8_t* dest, uint64_t offset, size_t maxLength)
{
    if (!maxLength)
        return 0;

    LockHolder locker(m_lock);
    if (m_aborted || offset >= m_totalSize)
        return 0;

    size_t available = static_cast<size_t>(std::min<uint64_t>(maxLength, m_totalSize - offset));

    auto it = std::upper_bound(m_chunkStartOffsets.begin(), m_chunkStartOffsets.end(), offset);
    size_t chunkIndex = static_cast<size_t>(it - m_chunkStartOffsets.begin());
    ASSERT(chunkIndex > 0);
    chunkIndex--;

    size_t bytesRead = 0;
    uint64_t pos = offset;
    while (bytesRead < available && chunkIndex < m_chunks.size()) {
        const Vector<uint8_t>& chunk = m_chunks[chunkIndex];
        uint64_t chunkStart = m_chunkStartOffsets[chunkIndex];
        size_t withinChunk = static_cast<size_t>(pos - chunkStart);
        if (withinChunk >= chunk.size()) {
            chunkIndex++;
            continue;
        }
        size_t chunkAvailable = chunk.size() - withinChunk;
        size_t toCopy = std::min(chunkAvailable, available - bytesRead);
        memcpy(dest + bytesRead, chunk.data() + withinChunk, toCopy);
        bytesRead += toCopy;
        pos += toCopy;
        if (withinChunk + toCopy >= chunk.size())
            chunkIndex++;
    }
    return bytesRead;
}

bool MediaSampleBuffer::hasDataAvailable(uint64_t offset, uint64_t length)
{
    LockHolder locker(m_lock);
    if (m_aborted)
        return false;
    // Overflow-safe: m_totalSize is itself bounded, so this can't wrap in practice, but guard
    // against a pathological huge `length` anyway.
    if (offset > m_totalSize)
        return false;
    return m_totalSize - offset >= length;
}

uint64_t MediaSampleBuffer::sizeSoFar()
{
    LockHolder locker(m_lock);
    return m_totalSize;
}

bool MediaSampleBuffer::isFinished()
{
    LockHolder locker(m_lock);
    return m_finished;
}

bool MediaSampleBuffer::isAborted()
{
    LockHolder locker(m_lock);
    return m_aborted;
}

void MediaSampleBuffer::setOnDataAvailable(std::function<void()> callback)
{
    LockHolder locker(m_lock);
    m_onDataAvailable = WTFMove(callback);
}

void MediaSampleBuffer::notifyDataAvailable()
{
    // Copy the callback out and invoke it with the lock released -- it may call straight back into
    // readAvailable()/sizeSoFar()/etc., which would deadlock against this same (non-reentrant) lock
    // if still held.
    std::function<void()> callback;
    {
        LockHolder locker(m_lock);
        callback = m_onDataAvailable;
    }
    if (callback)
        callback();
}

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
