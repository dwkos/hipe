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

/* mediaevents: shows which DOM media events a <video> fires after a file is loaded into it, and
 * when -- and how a client should wait for the right one before starting playback.
 *
 * Any DOM event name can be requested with HIPE_OP_EVENT_REQUEST, media events included. They do
 * not bubble, so they are requested on the <video> element itself; each arrives as a HIPE_OP_EVENT
 * whose arg[0] is the event name. The readiness ladder, in the order it normally happens:
 *
 *   loadstart -> durationchange -> loadedmetadata -> loadeddata -> canplay -> canplaythrough
 *
 * "canplay" is the one that means "enough is loaded to start playing". Starting playback on it,
 * rather than after a guessed delay, is what --play-on-canplay demonstrates; this program never
 * starts playback otherwise.
 *
 * It doubles as a regression test. Once, the first video played after hiped started never fired
 * "canplay" (it reached "loadeddata" and stopped: the element's readyState was stuck one step
 * short), so a client that waited for "canplay" as it should have hung forever. The exit status
 * says whether the readiness signal arrived:
 *
 *   0  "canplay" fired (and, with --play-on-canplay, "playing" followed it)
 *   4  "canplay" never fired
 *   5  --play-on-canplay was given and playback did not start after "canplay"
 *
 * usage: hipe-mediaevents <file.mp4|file.webm|file.ogv> [--play-on-canplay] [--seconds N]
 * Build:  cc mediaevents.c -o hipe-mediaevents -lhipe   (see the "testing" target in ../Makefile)
 */

#include <hipe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double nowMs(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

hipe_session session;

static hipe_loc getLoc(char* id) {
    hipe_send(session, HIPE_OP_GET_BY_ID, 0, 0, 1, id);
    hipe_instruction ins; hipe_instruction_init(&ins);
    hipe_await_instruction(session, &ins, HIPE_OP_LOCATION_RETURN);
    return ins.location;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: %s <file.mp4|file.webm|file.ogv> [--play-on-canplay] [--seconds N]\n", argv[0]);
        return 1;
    }
    int playOnCanplay = 0, seconds = 12;
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--play-on-canplay")) playOnCanplay = 1;
        else if (!strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = atoi(argv[++i]);
    }
    const char* mime = strstr(argv[1], ".mp4") ? "video/mp4" : strstr(argv[1], ".webm") ? "video/webm" : "video/ogg";

    FILE* f = fopen(argv[1], "rb");
    if (!f) { printf("cannot open %s\n", argv[1]); return 2; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char* data = malloc(size);
    if (fread(data, 1, size, f) != (size_t) size) { printf("read error\n"); return 2; }
    fclose(f);

    session = hipe_open_session(0, 0, 0, "mediaevents");
    if (!session) return 3;

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 2, "video", "v");
    hipe_loc vid = getLoc("v");
    hipe_send(session, HIPE_OP_SET_ATTRIBUTE, 0, vid, 2, "controls", "controls");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, vid, 2, "display", "block");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, vid, 2, "width", "100%");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, vid, 2, "height", "500px");

    /* The requestor value is echoed back with each event; here it is just the name's index + 1. */
    static const char* names[] = { "loadstart", "durationchange", "loadedmetadata", "loadeddata",
        "progress", "canplay", "canplaythrough", "play", "playing", "waiting", "stalled", "pause",
        "seeking", "seeked", "ended", "error", "suspend", "emptied", "ratechange", "timeupdate", 0 };
    enum { LOADEDDATA = 3, CANPLAY = 5, PLAYING = 8, TIMEUPDATE = 19 };
    for (int i = 0; names[i]; i++)
        hipe_send(session, HIPE_OP_EVENT_REQUEST, (uint64_t) (i + 1), vid, 1, (char*) names[i]);

    /* Request the events BEFORE loading, so none can be missed. */
    double t0 = nowMs();
    hipe_instruction s; hipe_instruction_init(&s);
    s.opcode = HIPE_OP_SET_SRC; s.location = vid;
    s.arg[0] = data; s.arg_length[0] = (uint64_t) size;
    s.arg[1] = (char*) mime; s.arg_length[1] = strlen(mime);
    hipe_send_instruction(session, s);
    printf("[%6.0f ms] SET_SRC sent (%ld bytes, single shot)%s\n", nowMs() - t0, size,
           playOnCanplay ? "; will play on canplay" : "; will NOT start playback");
    fflush(stdout);

    int seen[32] = {0}, timeupdates = 0, playSent = 0;
    hipe_instruction hi; hipe_instruction_init(&hi);
    while (nowMs() - t0 < seconds * 1000.0) {
        while (hipe_next_instruction(session, &hi, 0)) {
            if (hi.opcode == HIPE_OP_SERVER_DENIED || hi.opcode == HIPE_OP_FRAME_CLOSE) return 0;
            if (hi.opcode != HIPE_OP_EVENT) continue;
            int idx = (int) hi.requestor - 1;
            if (idx < 0 || idx >= 20) continue;
            seen[idx]++;
            /* "timeupdate" fires continuously during playback: show the first couple only. */
            if (idx == TIMEUPDATE && ++timeupdates > 2) continue;
            printf("[%6.0f ms] event: %.*s\n", nowMs() - t0, (int) hi.arg_length[0], hi.arg[0]);
            fflush(stdout);
            if (playOnCanplay && !playSent && idx == CANPLAY) {
                hipe_send(session, HIPE_OP_AUDIOVIDEO_STATE, 0, vid, 4, "", "", "1", "");
                playSent = 1;
                printf("[%6.0f ms] -> sent play (on canplay)\n", nowMs() - t0);
                fflush(stdout);
            }
        }
        struct timespec nap = {0, 20 * 1000 * 1000};
        nanosleep(&nap, NULL);
    }

    hipe_send(session, HIPE_OP_GET_AUDIOVIDEO_STATE, 0, vid, 0);
    hipe_instruction st; hipe_instruction_init(&st);
    hipe_await_instruction(session, &st, HIPE_OP_AUDIOVIDEO_STATE);
    printf("final state: position,duration=%.*s  paused=%s\n", (int) st.arg_length[0], st.arg[0],
           (st.arg_length[2] && st.arg[2][0] == '1') ? "no" : "yes");
    printf("events seen:");
    for (int i = 0; names[i]; i++)
        if (seen[i]) printf(" %s(x%d)", names[i], seen[i]);
    printf("\nloadeddata=%s canplay=%s playing=%s\n", seen[LOADEDDATA] ? "yes" : "NO",
           seen[CANPLAY] ? "yes" : "NO", seen[PLAYING] ? "yes" : "no");

    if (!seen[CANPLAY]) return 4;
    if (playOnCanplay && !seen[PLAYING]) return 5;
    return 0;
}
