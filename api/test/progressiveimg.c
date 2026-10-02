/*  Copyright (c) 2026 Daniel Kos, General Development Systems

    SPDX-License-Identifier: 0BSD

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/* Demo for chunked/progressive image loading via HIPE_OP_SET_SRC's new arg[2] "more chunks
 * follow" flag (hipecore QWebElement::begin/append/finishBinaryImageData(), hiped's
 * Container::pendingImageUploads two-threshold sanity policy).
 *
 * Reads progressive_test.jpg from disk (run this from api/test/, or pass a path as argv[1]) and
 * sends it to an <img> element two different ways so the difference is visible side by side:
 *  - "Load instantly": one HIPE_OP_SET_SRC call with the whole file -- today's existing,
 *    unchanged behaviour (arg[2] omitted).
 *  - "Load progressively": the same file split into small chunks, each its own SET_SRC call with
 *    arg[2]="1" except the last, with a deliberate delay between chunks so the progressive
 *    decode/repaint is actually visible rather than happening faster than the eye can follow.
 *
 * Both buttons discard and recreate the <img> element before each load (see
 * resetImageElement()) so every click starts from a genuinely blank element -- hipecore's
 * anti-flicker guard (matching real browsers) only allows progressive repainting into an element
 * that isn't already displaying a complete image, so reusing the same element for a second load
 * would otherwise only ever show the swap once the whole file has arrived.
 *
 * Usage: hipe-progressiveimg [image_path] [host_key]
 */

#include <hipe.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define CHUNK_SIZE 8192
#define CHUNK_DELAY_MS 100

static hipe_session session;
static hipe_loc imgLoc;
static hipe_loc statusLoc;
static char imagePath[512] = "progressive_test.jpg";

static void sleepMs(int ms) {
    struct timespec t = {ms / 1000, (long)(ms % 1000) * 1000 * 1000};
    nanosleep(&t, NULL);
}

static hipe_loc lastAppended(void) {
    return hipe_newest_location();
}

/* Discards the current image element and appends a brand new one in its place. Needed because
 * hipecore's anti-flicker guard (matching real browsers) only lets a chunked load repaint
 * progressively when nothing is already displayed in that <img> -- reusing the same element for a
 * second load waits silently for the whole transfer before swapping in the new picture, same as
 * changing an ordinary web page's img.src does. A fresh element has nothing to protect, so every
 * click gets to show real progressive loading, not just the first one. */
static void resetImageElement(void) {
    hipe_send(session, HIPE_OP_DELETE, 0, imgLoc, 0);
    hipe_send(session, HIPE_OP_FREE_LOCATION, 0, imgLoc, 0);
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 2, "img", "theImage");
    imgLoc = lastAppended();
}

/* Reads the whole file into a malloc'd buffer; caller frees. Returns NULL (and writes a status
 * message) on failure. */
static char* readWholeFile(const char* path, long* outLength) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        hipe_send(session, HIPE_OP_SET_TEXT, 0, statusLoc, 1, "couldn't open image file");
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*) malloc(length);
    if (!buf || fread(buf, 1, length, f) != (size_t) length) {
        free(buf);
        fclose(f);
        hipe_send(session, HIPE_OP_SET_TEXT, 0, statusLoc, 1, "couldn't read image file");
        return NULL;
    }
    fclose(f);
    *outLength = length;
    return buf;
}

static void loadInstantly(void) {
    long length;
    char* data = readWholeFile(imagePath, &length);
    if (!data) return;

    resetImageElement();
    hipe_send(session, HIPE_OP_SET_TEXT, 0, statusLoc, 1, "loading instantly...");
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    hipe_instruction instruction;
    hipe_instruction_init(&instruction);
    instruction.opcode = HIPE_OP_SET_SRC;
    instruction.location = imgLoc;
    instruction.arg[0] = data;
    instruction.arg_length[0] = (uint64_t) length;
    instruction.arg[1] = "image/jpeg";
    instruction.arg_length[1] = strlen("image/jpeg");
    hipe_send_instruction(session, instruction);

    clock_gettime(CLOCK_MONOTONIC, &now);
    double ms = (now.tv_sec - start.tv_sec) * 1000.0 + (now.tv_nsec - start.tv_nsec) / 1e6;
    char buf[96];
    snprintf(buf, sizeof(buf), "loaded instantly: %ld bytes in %.1f ms", length, ms);
    hipe_send(session, HIPE_OP_SET_TEXT, 0, statusLoc, 1, buf);

    free(data);
}

static void loadProgressively(void) {
    long length;
    char* data = readWholeFile(imagePath, &length);
    if (!data) return;

    /* Fresh element every time -- see resetImageElement()'s comment for why reusing the same one
     * would only ever show real progressive loading on the very first click. */
    resetImageElement();

    long sent = 0;
    int chunkCount = 0;
    while (sent < length) {
        long thisChunk = length - sent;
        if (thisChunk > CHUNK_SIZE) thisChunk = CHUNK_SIZE;
        int isLast = (sent + thisChunk >= length);

        hipe_instruction instruction;
        hipe_instruction_init(&instruction);
        instruction.opcode = HIPE_OP_SET_SRC;
        instruction.location = imgLoc;
        instruction.arg[0] = data + sent;
        instruction.arg_length[0] = (uint64_t) thisChunk;
        instruction.arg[1] = "image/jpeg";
        instruction.arg_length[1] = strlen("image/jpeg");
        if (!isLast) {
            instruction.arg[2] = "1";
            instruction.arg_length[2] = 1;
        }
        hipe_send_instruction(session, instruction);

        sent += thisChunk;
        chunkCount++;

        char buf[96];
        snprintf(buf, sizeof(buf), "loading progressively... chunk %d, %ld/%ld bytes",
                 chunkCount, sent, length);
        hipe_send(session, HIPE_OP_SET_TEXT, 0, statusLoc, 1, buf);

        if (!isLast) sleepMs(CHUNK_DELAY_MS);
    }

    char buf[96];
    snprintf(buf, sizeof(buf), "loaded progressively: %ld bytes in %d chunks", length, chunkCount);
    hipe_send(session, HIPE_OP_SET_TEXT, 0, statusLoc, 1, buf);

    free(data);
}

int main(int argc, char** argv) {
    const char* hostKey = argc > 2 ? argv[2] : 0;
    if (argc > 1) {
        strncpy(imagePath, argv[1], sizeof(imagePath) - 1);
        imagePath[sizeof(imagePath) - 1] = '\0';
    }

    session = hipe_open_session(hostKey, 0, 0, "ProgressiveImg");
    if (!session) exit(1);

    hipe_send(session, HIPE_OP_SET_TITLE, 0, 0, 1, "Progressive Image Loading Demo");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2,
              "body", "margin:0; padding:12px; font-family:sans-serif;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "#controls",
              "display:flex; gap:8px; align-items:center; margin-bottom:12px;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "#status",
              "font-family:monospace; font-size:90%; margin-bottom:12px; min-height:1.2em;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "#theImage",
              "max-width:100%; border:1px solid #888; display:block;");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 2, "div", "controls");
    hipe_loc controlsLoc = lastAppended();

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, controlsLoc, 2, "button", "btnInstant");
    hipe_loc btnInstantLoc = lastAppended();
    hipe_send(session, HIPE_OP_SET_TEXT, 0, btnInstantLoc, 1, "Load instantly");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, controlsLoc, 2, "button", "btnProgressive");
    hipe_loc btnProgressiveLoc = lastAppended();
    hipe_send(session, HIPE_OP_SET_TEXT, 0, btnProgressiveLoc, 1, "Load progressively");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 2, "div", "status");
    statusLoc = lastAppended();
    hipe_send(session, HIPE_OP_SET_TEXT, 0, statusLoc, 1, "ready");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 2, "img", "theImage");
    imgLoc = lastAppended();

    hipe_send(session, HIPE_OP_EVENT_REQUEST, 'I', btnInstantLoc, 2, "click", "");
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 'P', btnProgressiveLoc, 2, "click", "");

    hipe_instruction hi;
    hipe_instruction_init(&hi);
    while (1) {
        hipe_next_instruction(session, &hi, 1);
        if (hi.opcode == HIPE_OP_EVENT) {
            if (hi.requestor == 'I') {
                loadInstantly();
            } else if (hi.requestor == 'P') {
                loadProgressively();
            }
        } else if (hi.opcode == HIPE_OP_SERVER_DENIED || hi.opcode == HIPE_OP_FRAME_CLOSE) {
            return 0;
        }
    }
    return 0;
}
