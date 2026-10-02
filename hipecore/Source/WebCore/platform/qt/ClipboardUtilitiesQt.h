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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ClipboardUtilitiesQt_h
#define ClipboardUtilitiesQt_h

#include "TextEncoding.h"
#include <QByteArray>
#include <wtf/text/WTFString.h>

namespace WebCore {

// Decodes clipboard data that is UTF-16 text: big-endian if it starts with that byte order
// mark, little-endian otherwise. The byte order mark is not part of the result.
inline String decodeUTF16ClipboardData(const QByteArray& data)
{
    const char* bytes = data.constData();
    size_t length = data.size();
    bool isBigEndian = false;
    if (length >= 2) {
        unsigned char first = bytes[0];
        unsigned char second = bytes[1];
        if ((first == 0xFE && second == 0xFF) || (first == 0xFF && second == 0xFE)) {
            isBigEndian = first == 0xFE;
            bytes += 2;
            length -= 2;
        }
    }
    return (isBigEndian ? UTF16BigEndianEncoding() : UTF16LittleEndianEncoding()).decode(bytes, length);
}

}

#endif // ClipboardUtilitiesQt_h
