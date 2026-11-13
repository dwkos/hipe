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

/* A framed client's body colours reach its parent as HIPE_FRAME_EVENT_BACKGROUND_CHANGED / COLOR_CHANGED, always as
 * "rgb(r, g, b)": a see-through (rgba) colour is reported by its colour components, without the alpha. Run by run.sh
 * against a private hiped as the top-level client. A forked child fills an <iframe>, sets its body colours with
 * SET_STYLE, and sends HIPE_OP_MESSAGE "ready" after each step. Prints PASS/FAIL lines and exits non-zero if anything
 * failed. */

#include <hipe.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static hipe_session s;
static int fails = 0;
static char evlog[2000];
#define CHECK(name, cond) do { int ok_ = (cond); if(!ok_) fails++; \
    printf("%s %s  [%.300s]\n", ok_ ? "PASS" : "FAIL", name, evlog); fflush(stdout); } while(0)

static void childMain(const char* key) {
    hipe_session cs = hipe_open_session(key, 0, 0, "colour-child");
    if(!cs) _exit(1);
    static const char* steps[][2] = {
        {"", ""},
        {"background-color", "rgb(11, 12, 18)"},
        {"background-color", "rgba(0,0,128,0.5)"},
        {"background-color", "rgba(0,0,128,0.25)"},
        {"color", "rgba(17,34,51,0.25)"},
        {"background-color", "transparent"},
        {"background-color", "#0B0C12"},
        {"rule", ""}, /* a stylesheet background, as a theme gives */
        {"background-color", ""},
        {"background-color", "transparent"},
        {"background-color", ""},
        {"background-color", "transparent"},
    };
    hipe_instruction r; hipe_instruction_init(&r);
    for(unsigned i = 0; i < sizeof steps / sizeof steps[0]; i++) {
        if(i == 0) hipe_send(cs, HIPE_OP_APPEND_TAG, 0, 0, 4, "div", "", "", "child"); /* initialises the body */
        else if(!strcmp(steps[i][0], "rule")) {
            hipe_send(cs, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "body", "background-color: rgb(26, 27, 39)");
            hipe_send(cs, HIPE_OP_SET_STYLE, 0, 0, 2, "color", "rgb(17, 34, 51)"); /* SET_STYLE makes the check */
        } else hipe_send(cs, HIPE_OP_SET_STYLE, 0, 0, 2, steps[i][0], steps[i][1]);
        hipe_send(cs, HIPE_OP_MESSAGE, 0, 0, 2, "ready", "");
        hipe_await_instruction(cs, &r, HIPE_OP_MESSAGE); /* the parent's "next" */
        hipe_instruction_clear(&r);
    }
    while(hipe_next_instruction(cs, &r, 1) > 0) hipe_instruction_clear(&r);
    _exit(0);
}

/* logs the colour events from the frame, "bg:<colour>;" and "fg:<colour>;", until the child's next "ready" */
static void step(hipe_loc fr) {
    evlog[0] = 0;
    hipe_instruction r; hipe_instruction_init(&r);
    for(;;) {
        hipe_next_instruction(s, &r, 1);
        if(r.opcode == HIPE_OP_MESSAGE && r.location == fr && r.arg_length[0] == 5 && !memcmp(r.arg[0], "ready", 5)) {
            hipe_instruction_clear(&r);
            return;
        }
        if(r.opcode == HIPE_OP_FRAME_EVENT && r.location == fr && r.arg_length[0] == 1) {
            char e[200] = "";
            if(r.arg[0][0] == HIPE_FRAME_EVENT_BACKGROUND_CHANGED)
                snprintf(e, sizeof e, "bg:%.*s;", (int)r.arg_length[1], r.arg[1]);
            else if(r.arg[0][0] == HIPE_FRAME_EVENT_COLOR_CHANGED)
                snprintf(e, sizeof e, "fg:%.*s;", (int)r.arg_length[1], r.arg[1]);
            strncat(evlog, e, sizeof evlog - strlen(evlog) - 1);
        }
        hipe_instruction_clear(&r);
    }
}
static void next(hipe_loc fr) { hipe_send(s, HIPE_OP_MESSAGE, 0, fr, 2, "next", ""); }
static int has(const char* e) { return strstr(evlog, e) != NULL; }

int main() {
    s = hipe_open_session(0, 0, 0, "colour-test");
    if(!s) { fprintf(stderr, "no session: %s\n", hipe_last_error(0)); return 2; }
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, 0, 4, "iframe", "", "", "");
    hipe_loc fr = hipe_newest_location();
    hipe_instruction r; hipe_instruction_init(&r);
    hipe_send(s, HIPE_OP_GET_FRAME_KEY, 0, fr, 0);
    hipe_await_instruction(s, &r, HIPE_OP_KEY_RETURN);
    char key[256]; snprintf(key, sizeof key, "%.*s", (int)r.arg_length[0], r.arg[0]);
    pid_t child = fork();
    if(!child) childMain(key);

    step(fr);
    CHECK("C1 the first colours are reported as rgb(), even an unset (transparent) background",
          has("bg:rgb(") && has("fg:rgb(") && !has("rgba"));
    next(fr); step(fr);
    CHECK("C2 a solid background", !strcmp(evlog, "bg:rgb(11, 12, 18);"));
    next(fr); step(fr);
    CHECK("C3 a see-through background is reported without its alpha", !strcmp(evlog, "bg:rgb(0, 0, 128);"));
    next(fr); step(fr);
    CHECK("C4 a change of alpha alone isn't reported", !evlog[0]);
    next(fr); step(fr);
    CHECK("C5 a see-through text colour is reported without its alpha", !strcmp(evlog, "fg:rgb(17, 34, 51);"));
    next(fr); step(fr);
    CHECK("C6 transparent is reported as rgb(0, 0, 0)", !strcmp(evlog, "bg:rgb(0, 0, 0);"));
    next(fr); step(fr);
    CHECK("C7 a solid background after a see-through one", !strcmp(evlog, "bg:rgb(11, 12, 18);"));
    next(fr); step(fr);
    CHECK("C8 (a stylesheet background is set; the body's own still wins)", !evlog[0]);
    next(fr); step(fr);
    CHECK("C9 clearing the body's background reports the stylesheet's", !strcmp(evlog, "bg:rgb(26, 27, 39);"));
    next(fr); step(fr);
    CHECK("C10 then transparent", !strcmp(evlog, "bg:rgb(0, 0, 0);"));
    next(fr); step(fr);
    CHECK("C11 clearing again", !strcmp(evlog, "bg:rgb(26, 27, 39);"));
    next(fr); step(fr);
    CHECK("C12 transparent again", !strcmp(evlog, "bg:rgb(0, 0, 0);"));

    next(fr);
    kill(child, SIGTERM);
    hipe_close_session(s);
    printf("%s: %d failed\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
