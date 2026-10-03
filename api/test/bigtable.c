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

/* Large-scale table stress demo, interactive version.
 *
 * Two buttons: one builds a big table with the default table-layout:auto, the other builds the
 * same table with table-layout:fixed. Each button gets its own result line underneath showing how
 * long that build took, so the two layout algorithms can be compared side by side without needing
 * launch-time arguments.
 *
 * Table construction appends a row, reads its client-allocated location with no server round trip
 * (see lastAppended() below), appends a cell the same way, and sets its text -- at a scale where
 * any per-row/per-cell relayout cost becomes visible instead of lost in a handful of cells. Cell
 * content length varies row to row so table-layout:auto has to keep re-deriving column widths from
 * newly-seen content as construction proceeds, rather than settling on final widths after the
 * first row.
 *
 * Usage: hipe-bigtable [host_key]
 */

#include <hipe.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define ROWS 150
#define COLS 10

enum HideMode { HIDE_NONE = 0, HIDE_VISIBILITY = 1, HIDE_DISPLAY = 2 };

static hipe_session session;
static hipe_loc tableAreaLoc;
static hipe_loc resultAutoLoc;
static hipe_loc resultFixedLoc;
static enum HideMode hideMode = HIDE_NONE;

static double elapsed_ms(struct timespec *start, struct timespec *now) {
    return (now->tv_sec - start->tv_sec) * 1000.0 + (now->tv_nsec - start->tv_nsec) / 1e6;
}

/* The client allocates a new tag's location itself and sends it as part of HIPE_OP_APPEND_TAG
 * (see hipe_send_instruction() in hipe.c) -- hipe_newest_location() just returns that same
 * client-side value, so calling it right after an APPEND_TAG needs no server round trip at all.
 * (A prior version of this demo used a GET_LAST_CHILD round trip here instead, which serialized
 * the whole build loop to per-item network latency -- the same bug biglist.c had, fixed there in
 * 4d89e9c.) */
static hipe_loc lastAppended(void) {
    return hipe_newest_location();
}

/* Builds a fresh ROWS x COLS table under tableAreaLoc with the requested layout mode, times it,
 * and writes the elapsed time into resultLoc. */
static void buildTable(int fixedLayout, hipe_loc resultLoc) {
    hipe_send(session, HIPE_OP_SET_TEXT, 0, resultLoc, 1, "building...");
    hipe_send(session, HIPE_OP_CLEAR, 0, tableAreaLoc, 0);
    if (hideMode == HIDE_VISIBILITY) {
        hipe_send(session, HIPE_OP_SET_STYLE, 0, tableAreaLoc, 2, "visibility", "hidden");
    } else if (hideMode == HIDE_DISPLAY) {
        hipe_send(session, HIPE_OP_SET_STYLE, 0, tableAreaLoc, 2, "display", "none");
    }

    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, tableAreaLoc, 1, "table");
    hipe_loc tableLoc = lastAppended();
    hipe_send(session, HIPE_OP_SET_STYLE, 0, tableLoc, 2, "table-layout", fixedLayout ? "fixed" : "auto");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, tableLoc, 2, "width", "100%");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, tableLoc, 2, "border-collapse", "collapse");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, tableLoc, 1, "tbody");
    hipe_loc tbodyLoc = lastAppended();

    int row, col;
    char buf[32];
    for (row = 0; row < ROWS; row++) {
        hipe_send(session, HIPE_OP_APPEND_TAG, 0, tbodyLoc, 1, "tr");
        hipe_loc rowLoc = lastAppended();

        for (col = 0; col < COLS; col++) {
            hipe_send(session, HIPE_OP_APPEND_TAG, 0, rowLoc, 1, "td");
            hipe_loc cellLoc = lastAppended();

            /* content width cycles 1-7 characters as rows progress, so auto layout keeps
             * discovering wider content long after the first few rows have already been laid out */
            int width = 1 + ((row * COLS + col) % 7);
            int value = row * COLS + col;
            snprintf(buf, sizeof(buf), "%.*d", width, value);
            hipe_send(session, HIPE_OP_SET_TEXT, 0, cellLoc, 1, buf);
            hipe_send(session, HIPE_OP_FREE_LOCATION, 0, cellLoc, 0);
        }
        hipe_send(session, HIPE_OP_FREE_LOCATION, 0, rowLoc, 0);
    }

    clock_gettime(CLOCK_MONOTONIC, &now);
    if (hideMode == HIDE_VISIBILITY) {
        hipe_send(session, HIPE_OP_SET_STYLE, 0, tableAreaLoc, 2, "visibility", "visible");
    } else if (hideMode == HIDE_DISPLAY) {
        hipe_send(session, HIPE_OP_SET_STYLE, 0, tableAreaLoc, 2, "display", "block");
    }

    const char *modeLabel = hideMode == HIDE_VISIBILITY ? "visibility:hidden"
                           : hideMode == HIDE_DISPLAY    ? "display:none"
                                                          : "rendered while building";
    char resultBuf[64];
    snprintf(resultBuf, sizeof(resultBuf), "%d cells in %.1f ms (%s)", ROWS * COLS,
             elapsed_ms(&start, &now), modeLabel);
    hipe_send(session, HIPE_OP_SET_TEXT, 0, resultLoc, 1, resultBuf);
}

int main(int argc, char **argv) {
    const char *hostKey = argc > 1 ? argv[1] : 0;

    session = hipe_open_session(hostKey, 0, 0, "BigTable");
    if (!session) exit(1);

    hipe_send(session, HIPE_OP_SET_TITLE, 0, 0, 1, "BigTable Layout Demo");

    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2,
              "body", "margin:0; overflow:auto; font-family:sans-serif;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2,
              "td", "border:1px solid; padding:2px 6px; font-family:monospace;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "#controls",
              "display:flex; gap:24px; padding:12px;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "#controls div",
              "display:flex; flex-direction:column; align-items:flex-start; gap:6px;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "#controls div div",
              "font-family:monospace; font-size:90%; min-height:1.2em;");

    /* hide-mode radio group -- applies to whichever build button is clicked next */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 1, "div");
    hipe_loc hideRowLoc = lastAppended();
    hipe_send(session, HIPE_OP_SET_STYLE, 0, hideRowLoc, 2, "display", "flex");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, hideRowLoc, 2, "flex-direction", "column");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, hideRowLoc, 2, "gap", "6px");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, hideRowLoc, 2, "padding", "12px 12px 0 12px");

    const char *radioIds[3] = {"hmNone", "hmVisibility", "hmDisplay"};
    const char *radioLabels[3] = {"Render while building", "visibility:hidden while building",
                                   "display:none while building"};
    char radioRequestor[3] = {'N', 'V', 'D'};
    hipe_loc radioLoc[3];

    int i;
    for (i = 0; i < 3; i++) {
        hipe_send(session, HIPE_OP_APPEND_TAG, 0, hideRowLoc, 1, "label");
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

    /* controls bar: two columns, each a button plus its own result line */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 2, "div", "controls");
    hipe_loc controlsLoc = lastAppended();

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, controlsLoc, 1, "div");
    hipe_loc colAutoLoc = lastAppended();
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, colAutoLoc, 2, "button", "btnAuto");
    hipe_loc btnAutoLoc = lastAppended();
    hipe_send(session, HIPE_OP_SET_TEXT, 0, btnAutoLoc, 1, "Build (table-layout:auto)");
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, colAutoLoc, 1, "div");
    resultAutoLoc = lastAppended();

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, controlsLoc, 1, "div");
    hipe_loc colFixedLoc = lastAppended();
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, colFixedLoc, 2, "button", "btnFixed");
    hipe_loc btnFixedLoc = lastAppended();
    hipe_send(session, HIPE_OP_SET_TEXT, 0, btnFixedLoc, 1, "Build (table-layout:fixed)");
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, colFixedLoc, 1, "div");
    resultFixedLoc = lastAppended();

    /* where the built table gets inserted/cleared on each run */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 1, "div");
    tableAreaLoc = lastAppended();

    hipe_send(session, HIPE_OP_EVENT_REQUEST, 'A', btnAutoLoc, 2, "click", "");
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 'F', btnFixedLoc, 2, "click", "");
    for (i = 0; i < 3; i++) {
        hipe_send(session, HIPE_OP_EVENT_REQUEST, radioRequestor[i], radioLoc[i], 2, "click", "");
    }

    hipe_instruction hi;
    hipe_instruction_init(&hi);
    while (1) {
        hipe_next_instruction(session, &hi, 1);
        if (hi.opcode == HIPE_OP_EVENT) {
            if (hi.requestor == 'A') {
                buildTable(0, resultAutoLoc);
            } else if (hi.requestor == 'F') {
                buildTable(1, resultFixedLoc);
            } else if (hi.requestor == 'N') {
                hideMode = HIDE_NONE;
            } else if (hi.requestor == 'V') {
                hideMode = HIDE_VISIBILITY;
            } else if (hi.requestor == 'D') {
                hideMode = HIDE_DISPLAY;
            }
        } else if (hi.opcode == HIPE_OP_SERVER_DENIED || hi.opcode == HIPE_OP_FRAME_CLOSE) {
            return 0;
        }
    }
    return 0;
}
