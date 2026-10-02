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

/* hipe-video: <video> test client with an app-driven control bar.
 *
 * Renders a <video controls loop> plus a row of Play / Pause / Restart / Vol- /
 * Vol+ / Mute buttons wired to HIPE_OP_AUDIOVIDEO_STATE, and a live state
 * readout polled via HIPE_OP_GET_AUDIOVIDEO_STATE. Also has "Reload instantly" /
 * "Reload progressively" buttons exercising sequential chunked SET_SRC delivery
 * (HTMLMediaElement::begin/append/finishBinaryMediaData(), backed by a plain
 * MediaSampleBuffer read directly by a dedicated appsrc-based GStreamer source
 * element, WebKitBufferSrc -- see project_hipe_sequential_media_loading_scope.md)
 * -- these resend whatever file was passed on the command line.
 *
 * Real early playback for chunked audio/video requires HIPE_OP_SET_SRC's arg[3]
 * (the total-size hint, sent on the first chunk only -- see hipe_instruction.h)
 * with the real, correct file size. Container layout (faststart vs. not,
 * fragmented vs. not) turned out NOT to matter on its own -- root-caused to two
 * separate causes in the GStreamer backend, both needing the true size upfront to
 * resolve: format detection (typefind) falling back to a blocking content-sniff
 * scan without a URL extension to go on, and a post-header end-of-file
 * verification read that can't distinguish "hasn't arrived yet" from "genuinely
 * past the end" without knowing the real total. loadProgressive() below supplies
 * arg[3] and gets genuine early playback as a result -- live-confirmed starting
 * from as little as 10% of a file having arrived. Without arg[3], chunked
 * delivery still completes without error, but nothing plays until the last chunk
 * lands -- expected graceful degradation, not a bug in the chunking path itself.
 * Single-shot loads ("Reload instantly") are unaffected either way: the whole
 * file is already present and finalized before decoding needs to start.
 * api/test/sample_faststart.mp4 / sample_nonfaststart.mp4 (same source, moov
 * moved to the front via `ffmpeg -movflags +faststart`, or left at the end) both
 * work identically now that early playback depends on arg[3] rather than
 * container layout.
 *
 * The older MediaSource-based version of this feature required a genuinely
 * fragmented container (moof+mdat) and could only chunk sample_fragmented.mp4 /
 * sample_fragmented_multifrag.mp4; those files still happen to work here too
 * (an ordinary demuxer reading a fragmented stream sequentially is not a new
 * requirement), but are no longer the recommended files for exercising chunking.
 * One difference remains: GStreamer's qtdemux cannot seek a fragmented mp4 fed in
 * push mode, so Restart and every loop wrap on those files rebuild the pipeline
 * (MediaPlayerPrivateGStreamer::restartPipelineFromStart()) instead of flush-
 * seeking -- correct, but with a ~1 s pause at each wrap. Before that existed, a
 * looping fragmented file made hiped spin forever at end-of-stream (a refused
 * seek-to-0 was retried on every EOS, ~67 times a second); if you ever see hiped
 * pinned at high CPU with this client's state poll stalled, suspect that path.
 *
 * FIXED bug, kept as a regression test: playback paused at or very near the end
 * of a chunk-loaded element's buffered range, then resumed via play(), used to
 * get permanently stuck -- HIPE_OP_GET_AUDIOVIDEO_STATE would keep reporting
 * playing=1 with position frozen forever, requiring a full reload. Root cause
 * (in the MediaSource-based version this replaced): MediaPlayerPrivateGStreamer
 * ::doSeek() is a pre-existing stub for MediaSource content that never actually
 * seeks the pipeline, which nonetheless left m_seeking permanently stuck true.
 * The real fix, not just a workaround: chunked loading no longer goes through
 * MediaSource at all, so that stub simply doesn't apply -- seeking (including
 * the automatic loop-restart seek-to-zero HTMLMediaElement issues at end of
 * playback) is genuine, ordinary GStreamer seeking, the same code path a plain
 * file load already used. Looping chunked playback now genuinely loops, not
 * just avoids freezing. "Resume-near-end test" (testResumeNearEnd()) seeks to
 * 7.95s (the sample files are ~8.0s), pauses, plays, and traces position for 5
 * seconds afterward -- expect position to advance smoothly, reach the real end
 * within a fraction of a second, and genuinely restart from ~0 (loop is set).
 *
 * Purposes:
 *   1. exercise the native C++ <video controls> UI (RenderThemeQt::paintMedia*,
 *      including the buffered-range highlight on the scrubber track, fed by the
 *      ordinary MediaPlayer::buffered() -- no protocol/client-side work needed
 *      for that, it's the same generic machinery any GStreamer-backed load uses)
 *      that hipecore falls back to when ENABLE_MEDIA_CONTROLS_SCRIPT is off;
 *   2. show that a hipe app can fully drive video playback itself;
 *   3. exercise chunked/progressive SET_SRC delivery to a <video> element, and
 *      confirm reloading the same element repeatedly (both modes) behaves
 *      sanely -- unlike <img>, <video> reload doesn't need a discard-and-
 *      recreate workaround: a new load just replaces the player's source;
 *   4. "Play-early test (multi-frag)" button (loadProgressiveWithEarlyPlay()):
 *      proves chunked delivery genuinely lets playback start before the whole
 *      file has arrived, not just avoid buffering client-side -- sends a play
 *      command partway through a chunked transfer of the multi-fragment file
 *      and traces position-over-time to stdout to show it. Requires arg[3] (the
 *      total-size hint) on the first chunk to actually achieve this, same as
 *      loadProgressive() -- confirmed live under the current appsrc-based
 *      version, playing back smoothly from as little as 10% of the file arrived
 *      (an earlier version of this comment credited the file's fragmentation
 *      alone for this under the old MediaSource-based version; that turned out
 *      not to generalize -- see the file-level comment above).
 *
 *   hipe-video <file.ogv|file.webm|file.mp4> [keyfile]
 *
 * Playback starts by itself, once, when the <video> first reports "canplay" (the DOM
 * media event for "enough is loaded to start"), which is requested with
 * HIPE_OP_EVENT_REQUEST before the file is sent; after that it is human-driven, via the
 * buttons. (It used to start after a fixed 3.5 s instead, a guess at "GStreamer is ready by
 * now" that raced the real readiness; the event is the actual signal. If autoplay never
 * happens, "canplay" never fired -- worth knowing about, so there is deliberately no
 * timer fallback.)
 *
 * FIXED bug, client-side only (not a hipecore bug): loadProgressive() used to sleep synchronously
 * between chunks (sleepMs(PROGRESSIVE_CHUNK_DELAY_MS), ~300ms x 87 = ~26s total), fully blocking
 * this process's single thread -- Play/Pause/etc were undeliverable and the "pos=" readout below
 * the video sat frozen for the whole load regardless of what was actually happening in the
 * pipeline, since main()'s poll loop never ran to process replies. Fixed by replacing the blocking
 * sleep with pollWait(), which sleeps in small steps and calls dispatchPending() (the same
 * non-blocking receive+dispatch logic main()'s loop uses) between them, so the client stays live
 * for the whole transfer instead of only between chunks. loadBusy guards against a second
 * load-triggering click being delivered mid-transfer and starting an interleaved second chunk
 * stream to the same <video> element -- everything else (Play/Pause/volume) is still deliverable
 * during a load, which is the point.
 */

#include <hipe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define CHUNK_SIZE 16384
#define CHUNK_DELAY_MS 100

/* Deliberately much slower/finer than the above -- used only by "Reload progressively"
 * (loadProgressive()), so there's time to actually interact with the scrubber (drag it, watch
 * the buffered-range highlight grow) while it streams in, instead of finishing in ~2 seconds.
 * loadProgressiveWithEarlyPlay()'s timing is calibrated around CHUNK_SIZE/CHUNK_DELAY_MS above
 * for its own proof (play at ~1 fragment in) and is left alone. */
#define PROGRESSIVE_CHUNK_SIZE 4096
#define PROGRESSIVE_CHUNK_DELAY_MS 300

hipe_session session;

/* Whatever file was loaded from argv[1] -- kept around so "Reload instantly" and "Reload
 * progressively" can resend it on demand without re-reading from disk. */
static char* fileData = NULL;
static size_t fileSize = 0;
static char fileMime[32];
static hipe_loc vid;
static hipe_loc loadStatus;
static hipe_loc state;
static double gVol = 1.0;

/* Accumulated ms since the last HIPE_OP_GET_AUDIOVIDEO_STATE request -- see tick() below. It just
 * paces the "pos=" readout's request cadence. */
static int stateRequestAccumMs = 0;

/* Set once the one-shot autoplay on the first "canplay" has happened -- later loads ("Reload ...")
 * fire "canplay" again, and must not start playing on their own. */
static int autoplayed = 0;

/* Guards loadInstant()/loadProgressive()/loadProgressiveWithEarlyPlay()/testResumeNearEnd()
 * against re-entry: dispatchPending() (and hence one of these being re-invoked from a button
 * click) can now run from inside loadProgressive()'s own inter-chunk wait -- see pollWait(). */
static int loadBusy = 0;

static void sleepMs(int ms) {
    struct timespec t = {ms / 1000, (long)(ms % 1000) * 1000 * 1000};
    nanosleep(&t, NULL);
}

/* Forward declarations: dispatchPending() (defined after the load-/test-prefixed functions, since
 * it calls all of them) is itself called from pollWait(), which loadProgressive() calls -- circular. */
static void loadInstant(void);
static void loadProgressive(void);
static void loadProgressiveWithEarlyPlay(void);
static void testResumeNearEnd(void);
static void dispatchPending(void);
static void tick(int stepMs);
static void pollWait(int totalMs);

hipe_loc getLoc(char* id) {
    hipe_send(session, HIPE_OP_GET_BY_ID, 0, 0, 1, id);
    hipe_instruction ins; hipe_instruction_init(&ins);
    hipe_await_instruction(session, &ins, HIPE_OP_LOCATION_RETURN);
    return ins.location;
}

static const char* mimeForName(const char* n) {
    const char* d = strrchr(n, '.');
    if (d) {
        if (!strcasecmp(d, ".ogv") || !strcasecmp(d, ".ogg")) return "video/ogg";
        if (!strcasecmp(d, ".webm"))                          return "video/webm";
        if (!strcasecmp(d, ".mp4") || !strcasecmp(d, ".m4v")) return "video/mp4";
        if (!strcasecmp(d, ".mkv"))                           return "video/x-matroska";
    }
    return "video/ogg";
}

static hipe_loc mkbtn(hipe_loc bar, const char* id, const char* label, uint64_t req) {
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, bar, 2, "button", (char*)id);
    hipe_loc b = getLoc((char*)id);
    hipe_send(session, HIPE_OP_SET_TEXT, 0, b, 1, (char*)label);
    hipe_send(session, HIPE_OP_SET_STYLE, 0, b, 2, "margin", "0 4px");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, b, 2, "padding", "4px 10px");
    hipe_send(session, HIPE_OP_EVENT_REQUEST, req, b, 1, "click");
    return b;
}

/* One HIPE_OP_SET_SRC call with the whole file -- arg[2] omitted, today's existing single-shot
 * behaviour. Routes through begin/append/finishBinaryMediaData() the same as chunked delivery
 * (no base64 data: URI involved), just as one call instead of many -- works with any format the
 * backend can decode, no container-layout requirement at all since the whole file is already
 * present before decoding needs to start. */
static void loadInstant(void) {
    if (!fileData || loadBusy) return;
    hipe_send(session, HIPE_OP_SET_TEXT, 0, loadStatus, 1, "loading instantly...");

    hipe_instruction s; hipe_instruction_init(&s);
    s.opcode = HIPE_OP_SET_SRC; s.location = vid;
    s.arg[0] = fileData;    s.arg_length[0] = (uint64_t) fileSize;
    s.arg[1] = fileMime;    s.arg_length[1] = strlen(fileMime);
    hipe_send_instruction(session, s);

    char buf[96];
    snprintf(buf, sizeof buf, "loaded instantly: %zu bytes", fileSize);
    hipe_send(session, HIPE_OP_SET_TEXT, 0, loadStatus, 1, buf);
}

/* Same file, split into CHUNK_SIZE pieces sent as separate SET_SRC calls with arg[2]="1" on every
 * chunk but the last -- exercises HTMLMediaElement::begin/append/finishBinaryMediaData(), backed
 * by a MediaSampleBuffer/WebKitBufferSrc appsrc pipeline (see the file-level comment). Sends the
 * real file size as arg[3] on the first chunk, which is what actually lets metadata/playback
 * become available early rather than only once the whole transfer completes -- container layout
 * (faststart vs. not) makes no difference on its own; see the file-level comment for why. */
static void loadProgressive(void) {
    if (!fileData || loadBusy) return;
    loadBusy = 1;

    size_t sent = 0;
    int chunkCount = 0;
    while (sent < fileSize) {
        size_t thisChunk = fileSize - sent;
        if (thisChunk > PROGRESSIVE_CHUNK_SIZE) thisChunk = PROGRESSIVE_CHUNK_SIZE;
        int isLast = (sent + thisChunk >= fileSize);

        hipe_instruction s; hipe_instruction_init(&s);
        s.opcode = HIPE_OP_SET_SRC; s.location = vid;
        s.arg[0] = fileData + sent; s.arg_length[0] = (uint64_t) thisChunk;
        s.arg[1] = fileMime;        s.arg_length[1] = strlen(fileMime);
        if (!isLast) {
            s.arg[2] = "1";
            s.arg_length[2] = 1;
        }
        char sizeHint[32];
        if (sent == 0) {
            snprintf(sizeHint, sizeof sizeHint, "%zu", fileSize);
            s.arg[3] = sizeHint;
            s.arg_length[3] = strlen(sizeHint);
        }
        hipe_send_instruction(session, s);

        sent += thisChunk;
        chunkCount++;

        char buf[96];
        snprintf(buf, sizeof buf, "loading progressively... chunk %d, %zu/%zu bytes",
                 chunkCount, sent, fileSize);
        hipe_send(session, HIPE_OP_SET_TEXT, 0, loadStatus, 1, buf);

        if (!isLast) pollWait(PROGRESSIVE_CHUNK_DELAY_MS);
    }

    char buf[96];
    snprintf(buf, sizeof buf, "loaded progressively: %zu bytes in %d chunks", fileSize, chunkCount);
    hipe_send(session, HIPE_OP_SET_TEXT, 0, loadStatus, 1, buf);
    loadBusy = 0;
}

/* Does the file loading actually let playback *start* before the whole file has arrived -- the
 * actual point of chunked delivery, as opposed to just buffering a complete file client-side
 * first? Sends arg[3] (the real file size) on the first chunk, same as loadProgressive() -- that,
 * not the file's fragmentation, is what actually unlocks this (see the file-level comment). Loads
 * sample_fragmented_multifrag.mp4 fresh from disk (ignores whatever was passed on the command
 * line) mainly for historical continuity with this test's MediaSource-era origins; an ordinary
 * single-fragment or faststart file works just as well now. Sends roughly the first fragment's
 * worth of chunks, issues HIPE_OP_AUDIOVIDEO_STATE play *while the remaining chunks are still
 * being sent*, then keeps sending -- if this actually works, position should be seen advancing
 * well before "loaded progressively" ever prints, using only however much of the file has arrived
 * so far. */
static void loadProgressiveWithEarlyPlay(void) {
    if (loadBusy) return;
    loadBusy = 1;

    const char* path = "sample_fragmented_multifrag.mp4";
    FILE* f = fopen(path, "rb");
    if (!f) {
        hipe_send(session, HIPE_OP_SET_TEXT, 0, loadStatus, 1, "couldn't open sample_fragmented_multifrag.mp4");
        loadBusy = 0;
        return;
    }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* data = malloc(size);
    if (fread(data, 1, size, f) != (size_t) size) { fclose(f); free(data); loadBusy = 0; return; }
    fclose(f);

    /* ~1 fragment's worth (file is ~8 fragments, roughly evenly sized) before triggering play */
    size_t playAfterBytes = (size_t) size / 8 + CHUNK_SIZE;

    struct timespec t0; clock_gettime(CLOCK_MONOTONIC, &t0);
    printf("[play-early] file=%ld bytes, playAfterBytes=%zu\n", size, playAfterBytes);
    fflush(stdout);

    size_t sent = 0;
    int chunkCount = 0;
    int playedEarly = 0;
    while (sent < (size_t) size) {
        size_t thisChunk = (size_t) size - sent;
        if (thisChunk > CHUNK_SIZE) thisChunk = CHUNK_SIZE;
        int isLast = (sent + thisChunk >= (size_t) size);

        hipe_instruction s; hipe_instruction_init(&s);
        s.opcode = HIPE_OP_SET_SRC; s.location = vid;
        s.arg[0] = data + sent; s.arg_length[0] = (uint64_t) thisChunk;
        s.arg[1] = "video/mp4"; s.arg_length[1] = 9;
        if (!isLast) { s.arg[2] = "1"; s.arg_length[2] = 1; }
        char sizeHint[32];
        if (sent == 0) {
            snprintf(sizeHint, sizeof sizeHint, "%zu", (size_t) size);
            s.arg[3] = sizeHint;
            s.arg_length[3] = strlen(sizeHint);
        }
        hipe_send_instruction(session, s);

        sent += thisChunk;
        chunkCount++;

        char buf[128];
        snprintf(buf, sizeof buf, "play-early test: chunk %d, %zu/%ld bytes sent%s",
                 chunkCount, sent, size, playedEarly ? " (already told to play)" : "");
        hipe_send(session, HIPE_OP_SET_TEXT, 0, loadStatus, 1, buf);

        if (!playedEarly && sent >= playAfterBytes) {
            hipe_send(session, HIPE_OP_AUDIOVIDEO_STATE, 0, vid, 4, "", "", "1", "");
            playedEarly = 1;
            struct timespec tp; clock_gettime(CLOCK_MONOTONIC, &tp);
            double elapsed = (tp.tv_sec - t0.tv_sec) + (tp.tv_nsec - t0.tv_nsec) / 1e9;
            printf("[play-early] t=%.2fs: sent play command after %zu/%ld bytes (chunk %d)\n",
                   elapsed, sent, size, chunkCount);
            fflush(stdout);
        }

        /* once playing, record actual playback position after every remaining chunk -- this is
         * the precise proof: if pos advances meaningfully while sent < size, playback is genuinely
         * using progressively-arriving data, not waiting for the whole file. */
        if (playedEarly) {
            hipe_send(session, HIPE_OP_GET_AUDIOVIDEO_STATE, 0, vid, 0);
            hipe_instruction st; hipe_instruction_init(&st);
            hipe_await_instruction(session, &st, HIPE_OP_AUDIOVIDEO_STATE);
            struct timespec tp; clock_gettime(CLOCK_MONOTONIC, &tp);
            double elapsed = (tp.tv_sec - t0.tv_sec) + (tp.tv_nsec - t0.tv_nsec) / 1e9;
            printf("[play-early] t=%.2fs  sent=%zu/%ld (%.0f%%)  pos=%.*s  playing=%.*s\n",
                   elapsed, sent, size, 100.0 * sent / size,
                   (int)st.arg_length[0], st.arg[0], (int)st.arg_length[2], st.arg[2]);
            fflush(stdout);
        }

        if (!isLast) sleepMs(CHUNK_DELAY_MS);
    }

    char buf[128];
    snprintf(buf, sizeof buf, "play-early test done: %ld bytes in %d chunks, played after %zu bytes",
             size, chunkCount, playAfterBytes);
    hipe_send(session, HIPE_OP_SET_TEXT, 0, loadStatus, 1, buf);

    free(data);
    loadBusy = 0;
}

/* Investigates a user-reported bug: after playback reaches (or is paused at/near) end of stream,
 * pressing play again does nothing visible -- reported "playing" but position never advances,
 * frame frozen, until the element is reloaded. Seeks directly to near the end (removing guesswork
 * about timing a click), pauses there, then plays again and traces position over the next 5
 * seconds to see whether it actually resumes or gets stuck. */
static void testResumeNearEnd(void) {
    if (loadBusy) return;
    loadBusy = 1;

    hipe_send(session, HIPE_OP_AUDIOVIDEO_STATE, 0, vid, 4, "7.95", "", "0", "");
    sleepMs(500);

    hipe_instruction st0; hipe_instruction_init(&st0);
    hipe_send(session, HIPE_OP_GET_AUDIOVIDEO_STATE, 0, vid, 0);
    hipe_await_instruction(session, &st0, HIPE_OP_AUDIOVIDEO_STATE);
    printf("[resume-test] after seek to 7.95s + pause: pos=%.*s playing=%.*s\n",
           (int)st0.arg_length[0], st0.arg[0], (int)st0.arg_length[2], st0.arg[2]);
    fflush(stdout);

    hipe_send(session, HIPE_OP_AUDIOVIDEO_STATE, 0, vid, 4, "", "", "1", "");
    printf("[resume-test] sent play\n");
    fflush(stdout);

    for (int i = 0; i < 10; i++) {
        sleepMs(500);
        hipe_send(session, HIPE_OP_GET_AUDIOVIDEO_STATE, 0, vid, 0);
        hipe_instruction st; hipe_instruction_init(&st);
        hipe_await_instruction(session, &st, HIPE_OP_AUDIOVIDEO_STATE);
        printf("[resume-test] t+%.1fs: pos=%.*s playing=%.*s\n", (i + 1) * 0.5,
               (int)st.arg_length[0], st.arg[0], (int)st.arg_length[2], st.arg[2]);
        fflush(stdout);
    }

    loadBusy = 0;
}

/* Non-blocking receive+dispatch -- drains and handles everything currently pending (button
 * clicks, HIPE_OP_AUDIOVIDEO_STATE replies) without waiting. Used both by main()'s own poll loop
 * and by pollWait() below, so a chunked load in progress doesn't go deaf to the rest of the UI. */
static void dispatchPending(void) {
    hipe_instruction hi; hipe_instruction_init(&hi);
    while (hipe_next_instruction(session, &hi, 0)) {

        if (hi.opcode == HIPE_OP_SERVER_DENIED || hi.opcode == HIPE_OP_FRAME_CLOSE)
            exit(0);

        if (hi.opcode == HIPE_OP_EVENT) {
            switch (hi.requestor) {
                case 'p': hipe_send(session, HIPE_OP_AUDIOVIDEO_STATE, 0, vid, 4, "",  "", "1", ""); break;
                case 'P': hipe_send(session, HIPE_OP_AUDIOVIDEO_STATE, 0, vid, 4, "",  "", "0", ""); break;
                case 'r': hipe_send(session, HIPE_OP_AUDIOVIDEO_STATE, 0, vid, 4, "0", "", "1", ""); break;
                case 'd': gVol = gVol > 0.2 ? gVol - 0.2 : 0.0; goto setvol;
                case 'u': gVol = gVol < 0.8 ? gVol + 0.2 : 1.0; goto setvol;
                case 'm': gVol = 0.0; goto setvol;
                setvol: {
                    char volstr[16];
                    snprintf(volstr, sizeof volstr, "%.2f", gVol);
                    hipe_send(session, HIPE_OP_AUDIOVIDEO_STATE, 0, vid, 4, "", "", "", volstr);
                    break;
                }
                case 'c': /* "canplay": autoplay, once */
                    if (!autoplayed) {
                        autoplayed = 1;
                        hipe_send(session, HIPE_OP_AUDIOVIDEO_STATE, 0, vid, 4, "", "", "1", "");
                    }
                    break;
                case 'L': loadInstant(); break;
                case 'G': loadProgressive(); break;
                case 'E': loadProgressiveWithEarlyPlay(); break;
                case 'R': testResumeNearEnd(); break;
            }
        }

        if (hi.opcode == HIPE_OP_AUDIOVIDEO_STATE) {
            char buf[160];
            snprintf(buf, sizeof buf, "pos=%.*s  rate=%.*s  playing=%.*s  vol=%.*s",
                     (int)hi.arg_length[0], hi.arg[0], (int)hi.arg_length[1], hi.arg[1],
                     (int)hi.arg_length[2], hi.arg[2], (int)hi.arg_length[3], hi.arg[3]);
            printf("  %s\n", buf); fflush(stdout);
            hipe_send(session, HIPE_OP_SET_TEXT, 0, state, 1, buf);
        }
    }
}

/* One "stay alive" beat: drains pending instructions/replies via dispatchPending(), and -- at
 * roughly the same ~2x/second cadence main()'s loop used before this refactor -- requests a fresh
 * HIPE_OP_AUDIOVIDEO_STATE so the "pos=" readout keeps advancing. Shared by main()'s own loop and
 * pollWait() below: without this, the readout only updated from *main()*'s loop body, which stays
 * suspended on the call stack for the whole ~26s of a progressive load (loadProgressive() calls
 * pollWait() calls dispatchPending(), all nested under main()'s own dispatchPending() call that
 * delivered the "reload progressively" click in the first place) -- so requests to refresh it never
 * went out even once dispatchPending() alone made the client responsive to clicks again. stepMs is
 * how much wall-clock time this particular beat represents, for pacing the request cadence. */
static void tick(int stepMs) {
    dispatchPending();
    stateRequestAccumMs += stepMs;
    if (stateRequestAccumMs >= 500) {
        stateRequestAccumMs = 0;
        hipe_send(session, HIPE_OP_GET_AUDIOVIDEO_STATE, 0, vid, 0);
    }
}

/* Replaces a flat sleepMs(totalMs) between progressive-load chunks: sleeps in small steps, ticking
 * between each so the client stays responsive (and the "pos=" readout keeps updating) for the
 * whole duration of a chunk delay, not just between loads. */
static void pollWait(int totalMs) {
    const int stepMs = 20;
    int waited = 0;
    while (waited < totalMs) {
        tick(stepMs);
        int step = totalMs - waited < stepMs ? totalMs - waited : stepMs;
        sleepMs(step);
        waited += step;
    }
    tick(0);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <file.ogv|file.webm|file.mp4> [keyfile]\n", argv[0]);
        return 1;
    }

    strncpy(fileMime, mimeForName(argv[1]), sizeof(fileMime) - 1);
    fileMime[sizeof(fileMime) - 1] = '\0';
    FILE* f = fopen(argv[1], "rb");
    if (!f) { printf("Cannot open '%s'.\n", argv[1]); return 2; }
    fseek(f, 0, SEEK_END); fileSize = ftell(f); rewind(f);
    fileData = malloc(fileSize);
    if (fread(fileData, 1, fileSize, f) != fileSize) { printf("Read error.\n"); return 2; }
    fclose(f);

    session = hipe_open_session((argc > 2) ? argv[2] : 0, 0, 0, argv[0]);
    if (!session) return 3;

    hipe_send(session, HIPE_OP_SET_STYLE, 0, 0, 2, "margin", "0");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, 0, 2, "background", "#202020");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, 0, 2, "font-family", "sans-serif");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, 0, 2, "color", "#ddd");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 2, "div", "wrap");
    hipe_loc wrap = getLoc("wrap");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, wrap, 2, "video", "v");
    vid = getLoc("v");
    hipe_send(session, HIPE_OP_SET_ATTRIBUTE, 0, vid, 2, "controls", "controls");
    hipe_send(session, HIPE_OP_SET_ATTRIBUTE, 0, vid, 2, "loop", "loop");
    // Deliberately NOT flex-based sizing (flex:1 + min-height:0 inside a flex-direction:column
    // parent) -- that was this file's original layout, and it silently collapsed the <video> to
    // 0x0 with no error, no matter what was loaded into it (confirmed via a fresh, isolated,
    // minimal A/B test client: the exact same flex setup collapses a plain <div> too, whereas an
    // explicit width/height on the same elements renders correctly every time, native controls
    // and all). A real fix for sizing a flex item to "fill remaining space after N sibling rows"
    // for a replaced element in this engine is future work, not this test client's job -- plain
    // block flow with an explicit height sidesteps it entirely and is what's actually verified to
    // display a picture.
    hipe_send(session, HIPE_OP_SET_STYLE, 0, vid, 2, "display", "block");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, vid, 2, "width", "100%");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, vid, 2, "height", "600px");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, vid, 2, "background", "#000");

    // Before anything is loaded, so the first "canplay" cannot be missed.
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 'c', vid, 1, "canplay");

    // loadbar/loadStatus are built *before* the initial load below -- loadInstant()/
    // loadProgressive() both write status text into loadStatus (a global hipe_loc), and calling
    // either one while it's still its zero-initialized default would send that SET_TEXT to
    // location 0 (the document body) instead of a real element: silently replacing the body's
    // entire content -- including the <video> subtree just built above -- with a plain status
    // string. That's exactly what happened here the first time this was written: the element
    // handle stayed valid (GET_GEOMETRY never errored), but was orphaned outside the document, so
    // it could never paint anything -- a demo-client ordering bug, not a hipecore rendering bug.
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, wrap, 2, "div", "loadbar");
    hipe_loc loadbar = getLoc("loadbar");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, loadbar, 2, "padding", "6px");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, loadbar, 2, "text-align", "center");
    mkbtn(loadbar, "binstant", "Reload instantly", 'L');
    mkbtn(loadbar, "bprog",    "Reload progressively", 'G');
    mkbtn(loadbar, "bplayearly", "Play-early test (multi-frag)", 'E');
    mkbtn(loadbar, "bresumetest", "Resume-near-end test", 'R');
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, loadbar, 2, "span", "loadstatus");
    loadStatus = getLoc("loadstatus");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, loadStatus, 2, "margin-left", "12px");
    hipe_send(session, HIPE_OP_SET_TEXT, 0, loadStatus, 1, "loading...");

    loadInstant();

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, wrap, 2, "div", "bar");
    hipe_loc bar = getLoc("bar");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, bar, 2, "padding", "6px");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, bar, 2, "text-align", "center");

    mkbtn(bar, "bplay",  "\xE2\x96\xB6 Play",    'p');
    mkbtn(bar, "bpause", "\xE2\x8F\xB8 Pause",   'P');
    mkbtn(bar, "brst",   "\xE2\x8F\xAE Restart", 'r');
    mkbtn(bar, "bvd",    "Vol \xE2\x88\x92",     'd');
    mkbtn(bar, "bvu",    "Vol +",                'u');
    mkbtn(bar, "bmute",  "\xF0\x9F\x94\x87 Mute",'m');
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, bar, 2, "span", "state");
    state = getLoc("state");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, state, 2, "margin-left", "12px");
    hipe_send(session, HIPE_OP_SET_TEXT, 0, state, 1, "(starts playing when the video is ready)");

    printf("hipe-video ready. Playback starts by itself when the video reports \"canplay\"; use the buttons after that.\n");

    while (1) {
        tick(100);
        usleep(100000);
    }
}
