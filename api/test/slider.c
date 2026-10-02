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

/* Range slider demo: four <input type="range"> sliders, each given its own range and step with
   HIPE_OP_SET_ATTRIBUTE ("min", "max", "step"), and a readout beside each that follows it.

   The display server keeps each slider on its own steps, whether it is dragged or moved with the arrow
   keys. Each time one moves, an "input" event arrives; the slider's current value is then read back with
   HIPE_OP_GET_ATTRIBUTE ("value") and shown with HIPE_OP_SET_TEXT. */

#include <hipe.h>
#include <stdio.h>

struct slider {
    const char *min, *max, *step, *start;
    hipe_loc input;   /* the slider itself */
    hipe_loc readout; /* the table cell showing its value */
};

static struct slider sliders[] = {
    {"0",   "100", "10",   "50",  0, 0},
    {"10",  "20",  "5",    "15",  0, 0},
    {"0",   "1",   "0.05", "0.5", 0, 0},
    {"-50", "50",  "25",   "0",   0, 0}
};
#define NSLIDERS (sizeof sliders / sizeof sliders[0])

static hipe_session session;

static void showValue(size_t i)
/* reads slider i's current value from the display server and displays it in its readout cell. */
{
    hipe_instruction reply;
    char value[64];

    hipe_instruction_init(&reply);
    hipe_send(session, HIPE_OP_GET_ATTRIBUTE, 0, sliders[i].input, 1, "value");
    hipe_await_instruction(session, &reply, HIPE_OP_ATTRIBUTE_RETURN);
    snprintf(value, sizeof value, "%.*s", (int) reply.arg_length[1], reply.arg[1]);
    hipe_instruction_clear(&reply);

    hipe_send(session, HIPE_OP_SET_TEXT, 0, sliders[i].readout, 1, value);
}

int main(int argc, char** argv)
{
    hipe_loc table, rows, row, cell;
    hipe_instruction event;
    char label[96];
    size_t i;

    session = hipe_open_session(argc>1 ? argv[1] : 0, 0, 0, "Range sliders");
    if(!session) return 1;
    hipe_send(session, HIPE_OP_SET_TITLE, 0, 0, 1, "Range sliders");

    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "body", "padding:1em 1.5em;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "td", "padding:0.6em 1em; vertical-align:middle;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "input", "width:20em;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, ".value",
              "font-weight:bold; font-size:150%; min-width:4em; text-align:right;");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 4, "h2", "", "", "Range sliders: min, max and step");
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 4, "p", "", "",
              "Drag a slider, or click it and use the arrow keys. Each one moves only in its own steps.");

    /* Table rows go inside a tbody. A row appended straight to the table would be wrapped in a tbody
       of its own, and the location returned would refer to that wrapper instead of the row. */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 1, "table");
    table = hipe_newest_location();
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, table, 1, "tbody");
    rows = hipe_newest_location();

    for(i=0; i<NSLIDERS; i++) {
        hipe_send(session, HIPE_OP_APPEND_TAG, 0, rows, 1, "tr");
        row = hipe_newest_location();

        snprintf(label, sizeof label, "min %s, max %s, step %s", sliders[i].min, sliders[i].max, sliders[i].step);
        hipe_send(session, HIPE_OP_APPEND_TAG, 0, row, 4, "td", "", "", label);

        hipe_send(session, HIPE_OP_APPEND_TAG, 0, row, 1, "td");
        cell = hipe_newest_location();
        hipe_send(session, HIPE_OP_APPEND_TAG, 0, cell, 1, "input");
        sliders[i].input = hipe_newest_location();
        hipe_send(session, HIPE_OP_SET_ATTRIBUTE, 0, sliders[i].input, 2, "type", "range");
        /* the range and step are set before the value, so the value is kept as given if it fits them. */
        hipe_send(session, HIPE_OP_SET_ATTRIBUTE, 0, sliders[i].input, 2, "min", sliders[i].min);
        hipe_send(session, HIPE_OP_SET_ATTRIBUTE, 0, sliders[i].input, 2, "max", sliders[i].max);
        hipe_send(session, HIPE_OP_SET_ATTRIBUTE, 0, sliders[i].input, 2, "step", sliders[i].step);
        hipe_send(session, HIPE_OP_SET_ATTRIBUTE, 0, sliders[i].input, 2, "value", sliders[i].start);

        hipe_send(session, HIPE_OP_APPEND_TAG, 0, row, 3, "td", "", "value");
        sliders[i].readout = hipe_newest_location();

        /* the requestor value (i) comes back with each event, identifying which slider moved. */
        hipe_send(session, HIPE_OP_EVENT_REQUEST, i, sliders[i].input, 1, "input");
        showValue(i);
    }

    hipe_instruction_init(&event);
    for(;;) {
        if(hipe_next_instruction(session, &event, 1) < 0) break; /* the connection has been lost */
        if(event.opcode == HIPE_OP_SERVER_DENIED || event.opcode == HIPE_OP_FRAME_CLOSE) break;
        if(event.opcode == HIPE_OP_EVENT && event.requestor < NSLIDERS)
            showValue((size_t) event.requestor);
        hipe_instruction_clear(&event);
    }

    hipe_close_session(session);
    return 0;
}
