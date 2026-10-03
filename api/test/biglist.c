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

/* Large-scale bulk-insertion stress demo -- deliberately NOT a table.
 *
 * hipecore's table-triggered layout-batching delay (HTMLTableElement/HTMLTablePartElement bumping
 * Document::bumpLayoutBatchingDelay() on insertion) only fires for table/tr/td/th/tbody/thead/tfoot,
 * and hiped's own queue-depth-based generalization (Connection::service() bumping the same delay
 * when its instruction backlog gets deep) doesn't care about tag names at all -- so this demo builds
 * a big pile of plain <div>s instead, to exercise both without ever touching a table tag.
 *
 * Two position modes (checkbox-toggled), both keeping the same random background-color per item:
 *  - random: each item is position:absolute'd to a random point in listArea, screensaver-style.
 *    Found to be dominated by PAINT cost (repaint scaling with how many items are already placed),
 *    not layout -- visibility:hidden is barely faster than fully visible, so neither the table hook
 *    nor the queue-depth generalization (both purely layout-side) can help this case.
 *  - sequential: items flow inline-block, wrapping at the end of each line like words in a
 *    paragraph, same as bigtable.c's cells/biglist's original shape -- LAYOUT-bound (visibility:hidden
 *    saves a real chunk vs fully visible), which is the case the layout-batching-delay work targets.
 *
 * Same three hide-while-building modes as bigtable.c (visible throughout / visibility:hidden /
 * display:none) layered on top of either position mode.
 *
 * Usage: hipe-biglist [host_key]
 */

#include <hipe.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define ITEMS 1500

enum HideMode { HIDE_NONE = 0, HIDE_VISIBILITY = 1, HIDE_DISPLAY = 2 };
enum PositionMode { POS_RANDOM = 0, POS_SEQUENTIAL = 1 };

static hipe_session session;
static hipe_loc controlsLoc;
static hipe_loc listAreaLoc;
static hipe_loc resultLoc;
static enum HideMode hideMode = HIDE_NONE;
static enum PositionMode posMode = POS_RANDOM;
/* populated by applyLayoutGeometry() -- an initial best-effort call in main() (see the TODO there)
 * plus every subsequent "resize" event. These defaults are what's used until the first of those
 * actually lands. */
static int areaWidth = 100;
static int areaHeight = 100;

static double elapsed_ms(struct timespec *start, struct timespec *now) {
    return (now->tv_sec - start->tv_sec) * 1000.0 + (now->tv_nsec - start->tv_nsec) / 1e6;
}

/* The client allocates a new tag's location itself and sends it as part of HIPE_OP_APPEND_TAG
 * (see hipe_send_instruction() in hipe.c) -- hipe_newest_location() just returns that same
 * client-side value, so calling it right after an APPEND_TAG needs no server round trip at all.
 * (A prior version of this demo used a GET_LAST_CHILD round trip here instead, which serialized
 * the whole build loop to per-item network latency -- worth ~50s on 1500 items.) */
static hipe_loc lastAppended(void) {
    return hipe_newest_location();
}

/* arg[i] is not null-terminated on the wire -- copy+terminate before atoi(). */
static int argToInt(hipe_instruction *instruction, int i) {
    char buf[16];
    size_t len = instruction->arg_length[i];
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, instruction->arg[i], len);
    buf[len] = '\0';
    return atoi(buf);
}

static void getGeometry(hipe_loc loc, int *outWidth, int *outHeight) {
    hipe_send(session, HIPE_OP_GET_GEOMETRY, 0, loc, 0);
    hipe_instruction instruction;
    hipe_instruction_init(&instruction);
    hipe_await_instruction(session, &instruction, HIPE_OP_GEOMETRY_RETURN);
    *outWidth = argToInt(&instruction, 2);
    *outHeight = argToInt(&instruction, 3);
}

/* Sizes #listArea to fill whatever's left of the real window below the controls panel, and
 * refreshes areaWidth/areaHeight for random-mode placement. Called from main() as an initial
 * best-effort attempt, and again on every subsequent "resize" event -- confirmed live that
 * "resize" does NOT fire for a newly-docked frame's first sizing (periscope apparently creates it
 * already at its final size, no separate small-then-resized transition to generate an event from),
 * so it's only useful for correctness on later, genuine resizes (periscope re-tiling when another
 * client docks/undocks), not for the initial race. Returns the controlsWidth it read, so callers
 * can tell whether the reading looked real (padding-only == not ready yet). */
static int applyLayoutGeometry(void) {
    int controlsWidth, controlsHeight;
    getGeometry(controlsLoc, &controlsWidth, &controlsHeight);

    /* overflow:auto, not hidden -- sequential mode's content can exceed listArea's fixed height,
     * and without a scrollbar there'd be no way to confirm the clipped tail actually rendered */
    char areaStyleBuf[96];
    snprintf(areaStyleBuf, sizeof(areaStyleBuf),
             "position:fixed; top:%dpx; left:0; right:0; bottom:0; overflow:auto; border:1px solid;",
             controlsHeight);
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "#listArea", areaStyleBuf);

    /* now listArea's own geometry reflects the real available space -- use it, not a guess */
    getGeometry(listAreaLoc, &areaWidth, &areaHeight);
    return controlsWidth;
}

/* Builds ITEMS fresh <div>s under listAreaLoc at random screensaver-style positions/colors, times
 * it, and writes the elapsed time into resultLoc. */
static void buildList(void) {
    hipe_send(session, HIPE_OP_SET_TEXT, 0, resultLoc, 1, "building...");
    hipe_send(session, HIPE_OP_CLEAR, 0, listAreaLoc, 0);
    if (hideMode == HIDE_VISIBILITY) {
        hipe_send(session, HIPE_OP_SET_STYLE, 0, listAreaLoc, 2, "visibility", "hidden");
    } else if (hideMode == HIDE_DISPLAY) {
        hipe_send(session, HIPE_OP_SET_STYLE, 0, listAreaLoc, 2, "display", "none");
    }

    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    int i;
    char buf[48];
    for (i = 0; i < ITEMS; i++) {
        hipe_send(session, HIPE_OP_APPEND_TAG, 0, listAreaLoc, 2, "div", "");
        hipe_loc rowLoc = lastAppended();

        /* content width cycles 1-7 characters, same as bigtable.c's cells */
        int width = 1 + (i % 7);
        snprintf(buf, sizeof(buf), "%.*d", width, i);
        hipe_send(session, HIPE_OP_SET_TEXT, 0, rowLoc, 1, buf);

        if (posMode == POS_RANDOM) {
            /* left/top directly, not transform:translate() -- transform offsets from the
             * element's static (flow) position, which is no longer pinned to a shared origin
             * now that #listArea div's base rule doesn't force top:0;left:0 any more */
            int x = rand() % areaWidth;
            int y = rand() % areaHeight;
            hipe_send(session, HIPE_OP_SET_STYLE, 0, rowLoc, 2, "position", "absolute");
            snprintf(buf, sizeof(buf), "%dpx", x);
            hipe_send(session, HIPE_OP_SET_STYLE, 0, rowLoc, 2, "left", buf);
            snprintf(buf, sizeof(buf), "%dpx", y);
            hipe_send(session, HIPE_OP_SET_STYLE, 0, rowLoc, 2, "top", buf);
        }
        /* background colour varies either way -- it's the positioning we're isolating, not this */
        int hue = rand() % 360;
        snprintf(buf, sizeof(buf), "hsl(%d, 70%%, 55%%)", hue);
        hipe_send(session, HIPE_OP_SET_STYLE, 0, rowLoc, 2, "background-color", buf);

        hipe_send(session, HIPE_OP_FREE_LOCATION, 0, rowLoc, 0);
    }

    clock_gettime(CLOCK_MONOTONIC, &now);
    if (hideMode == HIDE_VISIBILITY) {
        hipe_send(session, HIPE_OP_SET_STYLE, 0, listAreaLoc, 2, "visibility", "visible");
    } else if (hideMode == HIDE_DISPLAY) {
        hipe_send(session, HIPE_OP_SET_STYLE, 0, listAreaLoc, 2, "display", "block");
    }

    const char *modeLabel = hideMode == HIDE_VISIBILITY ? "visibility:hidden"
                           : hideMode == HIDE_DISPLAY    ? "display:none"
                                                          : "rendered while building";
    const char *posLabel = posMode == POS_SEQUENTIAL ? "sequential" : "random position";
    char resultBuf[80];
    snprintf(resultBuf, sizeof(resultBuf), "%d items in %.1f ms (%s, %s)", ITEMS,
             elapsed_ms(&start, &now), modeLabel, posLabel);
    hipe_send(session, HIPE_OP_SET_TEXT, 0, resultLoc, 1, resultBuf);
}

int main(int argc, char **argv) {
    const char *hostKey = argc > 1 ? argv[1] : 0;
    srand((unsigned) time(NULL));

    session = hipe_open_session(hostKey, 0, 0, "BigList");
    if (!session) exit(1);

    hipe_send(session, HIPE_OP_SET_TITLE, 0, 0, 1, "BigList Layout Demo");

    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2,
              "body", "margin:0; overflow:hidden; font-family:sans-serif;");
    /* inline-block (not block) so sequential mode wraps items left-to-right within the container
     * instead of stacking one per line -- position:absolute gets applied per-item only in random
     * mode, since an item positioned that way ignores this base display value anyway. */
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "#listArea div",
              "display:inline-block; padding:2px 6px; font-family:monospace; color:black;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "#controls",
              "display:flex; flex-direction:column; gap:6px; padding:12px;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "#result",
              "font-family:monospace; font-size:90%; min-height:1.2em;");

    /* hide-mode radio group -- applies to the next build */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 2, "div", "controls");
    controlsLoc = lastAppended();

    const char *radioIds[3] = {"hmNone", "hmVisibility", "hmDisplay"};
    const char *radioLabels[3] = {"Render while building", "visibility:hidden while building",
                                   "display:none while building"};
    char radioRequestor[3] = {'N', 'V', 'D'};
    hipe_loc radioLoc[3];

    int i;
    for (i = 0; i < 3; i++) {
        hipe_send(session, HIPE_OP_APPEND_TAG, 0, controlsLoc, 1, "label");
        hipe_loc labelLoc = lastAppended();
        hipe_send(session, HIPE_OP_SET_STYLE, 0, labelLoc, 2, "display", "inline-flex");
        hipe_send(session, HIPE_OP_SET_STYLE, 0, labelLoc, 2, "align-items", "center");
        hipe_send(session, HIPE_OP_SET_STYLE, 0, labelLoc, 2, "gap", "6px");

        hipe_send(session, HIPE_OP_APPEND_TAG, 0, labelLoc, 2, "input", radioIds[i]);
        radioLoc[i] = lastAppended();
        hipe_send(session, HIPE_OP_SET_ATTRIBUTE, 0, radioLoc[i], 2, "type", "radio");
        hipe_send(session, HIPE_OP_SET_ATTRIBUTE, 0, radioLoc[i], 2, "name", "hidemode");
        if (i == 0) {
            hipe_send(session, HIPE_OP_SET_ATTRIBUTE, 0, radioLoc[i], 2, "checked", "checked");
        }

        hipe_send(session, HIPE_OP_APPEND_TAG, 0, labelLoc, 1, "span");
        hipe_loc spanLoc = lastAppended();
        hipe_send(session, HIPE_OP_SET_TEXT, 0, spanLoc, 1, radioLabels[i]);
    }

    /* position-mode checkbox -- random (screensaver) scatter vs sequential inline flow */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, controlsLoc, 1, "label");
    hipe_loc posLabelLoc = lastAppended();
    hipe_send(session, HIPE_OP_SET_STYLE, 0, posLabelLoc, 2, "display", "inline-flex");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, posLabelLoc, 2, "align-items", "center");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, posLabelLoc, 2, "gap", "6px");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, posLabelLoc, 2, "input", "chkSequential");
    hipe_loc chkSequentialLoc = lastAppended();
    hipe_send(session, HIPE_OP_SET_ATTRIBUTE, 0, chkSequentialLoc, 2, "type", "checkbox");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, posLabelLoc, 1, "span");
    hipe_loc posSpanLoc = lastAppended();
    hipe_send(session, HIPE_OP_SET_TEXT, 0, posSpanLoc, 1, "Sequential (inline) positioning");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, controlsLoc, 2, "button", "btnBuild");
    hipe_loc btnBuildLoc = lastAppended();
    hipe_send(session, HIPE_OP_SET_TEXT, 0, btnBuildLoc, 1, "Build list (1500 items)");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, controlsLoc, 2, "div", "result");
    resultLoc = lastAppended();

    /* listArea itself doesn't need real geometry to exist -- until the first "resize" event
     * lands (see applyLayoutGeometry()), it just renders in normal document flow right after
     * #controls, which is a perfectly reasonable fallback on its own; position:fixed anchoring
     * and its real dimensions get applied reactively once the window is actually sized. */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 2, "div", "listArea");
    listAreaLoc = lastAppended();

    /* "resize" covers *later* real resizes (periscope re-tiling when another client docks/
     * undocks) but confirmed live that it does NOT fire for this frame's own initial sizing --
     * periscope apparently creates it already at its final size, no small-then-resized transition
     * to generate an event from. So it can't replace an initial attempt, only supplement it. */
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 'W', 0, 2, "resize", "");

    /* CRUTCH, slimmed down: periscope's own instructions that create/size this frame's docking
     * iframe are sent asynchronously from a *different* client connection (periscope's own), so
     * querying geometry immediately after this frame merely exists can race ahead of periscope's
     * instructions having reached/been processed by hiped yet -- a fast cross-connection ordering
     * race, not an open-ended window-system negotiation, so a handful of short retries covers it
     * in practice (usually resolves in 1 attempt).
     * TODO (parked -- nice to fix properly, not blocking): no protocol signal exists for "the
     * thing that determines your geometry has actually been processed" -- revisit if a better idea
     * comes up. */
    {
        int attempt;
        for (attempt = 0; attempt < 5; attempt++) {
            if (applyLayoutGeometry() > 24) break;
            struct timespec sleepTime = {0, 50 * 1000 * 1000}; /* 50ms */
            nanosleep(&sleepTime, NULL);
        }
    }

    hipe_send(session, HIPE_OP_EVENT_REQUEST, 'B', btnBuildLoc, 2, "click", "");
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 'S', chkSequentialLoc, 2, "click", "");
    for (i = 0; i < 3; i++) {
        hipe_send(session, HIPE_OP_EVENT_REQUEST, radioRequestor[i], radioLoc[i], 2, "click", "");
    }

    hipe_instruction hi;
    hipe_instruction_init(&hi);
    while (1) {
        hipe_next_instruction(session, &hi, 1);
        if (hi.opcode == HIPE_OP_EVENT) {
            if (hi.requestor == 'B') {
                buildList();
            } else if (hi.requestor == 'N') {
                hideMode = HIDE_NONE;
            } else if (hi.requestor == 'V') {
                hideMode = HIDE_VISIBILITY;
            } else if (hi.requestor == 'D') {
                hideMode = HIDE_DISPLAY;
            } else if (hi.requestor == 'S') {
                /* native checkbox toggles itself in the DOM on click; mirror that locally */
                posMode = posMode == POS_RANDOM ? POS_SEQUENTIAL : POS_RANDOM;
            } else if (hi.requestor == 'W') {
                applyLayoutGeometry();
            }
        } else if (hi.opcode == HIPE_OP_SERVER_DENIED || hi.opcode == HIPE_OP_FRAME_CLOSE) {
            return 0;
        }
    }
    return 0;
}
