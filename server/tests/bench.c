/*  Copyright (c) 2026 General Development Systems

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

/* Timing of APPEND_TAG and mode 3 SET_TEXT, for comparing hiped builds: run.sh bench [count]. */
#include <hipe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static double now() { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec*1e3 + t.tv_nsec/1e6; }
static void sync_(hipe_session s) { hipe_instruction in; hipe_instruction_init(&in); hipe_send(s, HIPE_OP_GET_CONTENT, 0, 0, 1, "3"); hipe_await_instruction(s, &in, HIPE_OP_CONTENT_RETURN); hipe_instruction_clear(&in); }
int main(int argc, char** argv) {
    int n = atoi(argv[1]);
    hipe_session s = hipe_open_session(0, 0, 0, "micro");
    hipe_set_buffered(s, 1);
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, 0, 1, "div"); hipe_loc box = hipe_newest_location();
    hipe_send(s, HIPE_OP_SET_STYLE, 0, box, 2, "display", "none");
    sync_(s);
    double t0 = now();
    for(int i = 0; i < n; i++) hipe_send(s, HIPE_OP_APPEND_TAG, 0, box, 4, "div", "", "", "line");
    sync_(s);
    double t1 = now();
    hipe_loc last = hipe_newest_location();
    for(int i = 0; i < n; i++)
        hipe_send(s, HIPE_OP_SET_TEXT, 0, last - (hipe_loc)(i % 1000), 2,
            "<span class=k>int</span> <span class=i>x</span> = <span class=n>42</span>; <span class=c>// note</span>", "3");
    sync_(s);
    double t2 = now();
    printf("append_tag %d: %.0f ms   set_text mode3 %d: %.0f ms\n", n, t1 - t0, n, t2 - t1);
    hipe_close_session(s); return 0;
}
