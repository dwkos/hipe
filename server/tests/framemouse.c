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

/* mousedown/mouseup requested on a child's <iframe>: presses anywhere in the frame, including a frame nested in it,
 * are reported to the parent in its own page coordinates, while the child still gets its own events. Run by run.sh
 * against a private hiped with no window manager, as the top-level client; clicks use xdotool on $DISPLAY. A forked
 * child fills an <iframe> and reports its own mousedown events back with HIPE_OP_MESSAGE; a grandchild fills an
 * <iframe> at the top of the child. Prints PASS/FAIL lines and exits non-zero if anything failed. */

#include <hipe.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static hipe_session s;
static int fails = 0;
static char evlog[4000];
#define CHECK(name, cond) do { int ok_ = (cond); if(!ok_) fails++; \
    printf("%s %s  [%.300s]\n", ok_ ? "PASS" : "FAIL", name, evlog); fflush(stdout); } while(0)

static void ready(hipe_session x, hipe_instruction* r) { /* waits for a MESSAGE "ready" from location 0 or a child */
    do { hipe_instruction_clear(r); hipe_await_instruction(x, r, HIPE_OP_MESSAGE); }
    while(!(r->arg_length[0] == 5 && !memcmp(r->arg[0], "ready", 5)));
}

static void grandchildMain(const char* key) {
    hipe_session gs = hipe_open_session(key, 0, 0, "mouse-grandchild");
    if(!gs) _exit(1);
    hipe_send(gs, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "body", "margin: 0");
    hipe_send(gs, HIPE_OP_APPEND_TAG, 0, 0, 4, "div", "", "", "grandchild");
    hipe_send(gs, HIPE_OP_MESSAGE, 0, 0, 2, "ready", "");
    hipe_instruction in; hipe_instruction_init(&in);
    while(hipe_next_instruction(gs, &in, 1) > 0) hipe_instruction_clear(&in);
    _exit(0);
}

/* the child: a grandchild frame at the top, then tall content. Sends its own mousedowns to the parent, and scrolls
 * by 500px when the parent asks. */
static void childMain(const char* key) {
    hipe_session cs = hipe_open_session(key, 0, 0, "mouse-child");
    if(!cs) _exit(1);
    signal(SIGCHLD, SIG_IGN);
    hipe_send(cs, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "body", "margin: 0");
    hipe_send(cs, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "iframe", "display: block; margin: 0; border: 0; width: 200px; height: 100px");
    hipe_send(cs, HIPE_OP_APPEND_TAG, 0, 0, 4, "iframe", "", "", "");
    hipe_loc gf = hipe_newest_location();
    hipe_send(cs, HIPE_OP_APPEND_TAG, 0, 0, 4, "div", "", "", "child");
    hipe_send(cs, HIPE_OP_SET_STYLE, 0, hipe_newest_location(), 2, "height", "3000px");
    hipe_instruction r; hipe_instruction_init(&r);
    hipe_send(cs, HIPE_OP_GET_FRAME_KEY, 0, gf, 0);
    hipe_await_instruction(cs, &r, HIPE_OP_KEY_RETURN);
    char gkey[256]; snprintf(gkey, sizeof gkey, "%.*s", (int)r.arg_length[0], r.arg[0]);
    if(!fork()) grandchildMain(gkey);
    ready(cs, &r);
    hipe_send(cs, HIPE_OP_EVENT_REQUEST, 0, 0, 1, "mousedown");
    hipe_send(cs, HIPE_OP_MESSAGE, 0, 0, 2, "ready", "");
    while(hipe_next_instruction(cs, &r, 1) > 0) {
        if(r.opcode == HIPE_OP_EVENT && r.location == 0) {
            char d[100]; snprintf(d, sizeof d, "%.*s", (int)r.arg_length[1], r.arg[1]);
            hipe_send(cs, HIPE_OP_MESSAGE, 0, 0, 2, "child", d);
        } else if(r.opcode == HIPE_OP_MESSAGE && r.arg_length[0] == 6 && !memcmp(r.arg[0], "scroll", 6)) {
            hipe_send(cs, HIPE_OP_SCROLL_BY, 0, 0, 3, "", "500", "");
            hipe_send(cs, HIPE_OP_MESSAGE, 0, 0, 2, "ready", "");
        }
        hipe_instruction_clear(&r);
    }
    _exit(0);
}

static const char* window() {
    static char b[32]; FILE* p = popen("xdotool search --name '^mouse-test$' | head -1", "r"); b[0] = 0;
    if(p) { if(!fgets(b, sizeof b, p)) b[0] = 0; pclose(p); }
    b[strcspn(b, "\n")] = 0; return b;
}
/* clicks at x,y in the window, then logs for 1s what arrives: "mousedown#7:<detail>;" for events on the frame,
 * "child:<detail>;" for the child's own mousedowns */
static void clickAt(int x, int y, hipe_loc fr) {
    char cmd[200]; snprintf(cmd, sizeof cmd, "xdotool mousemove --window %s %d %d sleep 0.1 click 1", window(), x, y);
    if(system(cmd) != 0) printf("  (xdotool failed)\n");
    evlog[0] = 0;
    hipe_instruction r; hipe_instruction_init(&r);
    for(int i = 0; i < 20; i++) {
        usleep(50000);
        while(hipe_next_instruction(s, &r, 0) > 0) {
            char e[200] = "";
            if(r.opcode == HIPE_OP_EVENT && r.location == fr)
                snprintf(e, sizeof e, "%.*s#%llu:%.*s;", (int)r.arg_length[0], r.arg[0], (unsigned long long)r.requestor,
                         (int)r.arg_length[1], r.arg[1]);
            else if(r.opcode == HIPE_OP_MESSAGE && r.location == fr)
                snprintf(e, sizeof e, "%.*s:%.*s;", (int)r.arg_length[0], r.arg[0], (int)r.arg_length[1], r.arg[1]);
            strncat(evlog, e, sizeof evlog - strlen(evlog) - 1);
            hipe_instruction_clear(&r);
        }
    }
}
static int has(const char* e) { return strstr(evlog, e) != NULL; }

int main() {
    s = hipe_open_session(0, 0, 0, "mouse-test");
    if(!s) { fprintf(stderr, "no session: %s\n", hipe_last_error(0)); return 2; }
    /* the child's viewport starts at (55,105): 50px margin + 5px border, 100px spacer + 5px border */
    hipe_send(s, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "body", "margin: 0");
    hipe_send(s, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "iframe",
              "display: block; margin: 0 0 0 50px; border: 5px solid black; padding: 0; width: 400px; height: 300px");
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, 0, 4, "div", "", "", "");
    hipe_send(s, HIPE_OP_SET_STYLE, 0, hipe_newest_location(), 2, "height", "100px");
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, 0, 4, "iframe", "", "", "");
    hipe_loc fr = hipe_newest_location();
    hipe_instruction r; hipe_instruction_init(&r);
    hipe_send(s, HIPE_OP_GET_FRAME_KEY, 0, fr, 0);
    hipe_await_instruction(s, &r, HIPE_OP_KEY_RETURN);
    char key[256]; snprintf(key, sizeof key, "%.*s", (int)r.arg_length[0], r.arg[0]);
    pid_t child = fork();
    if(!child) childMain(key);
    ready(s, &r);
    usleep(500000);

    clickAt(155, 305, fr);
    CHECK("M1 without a request, the parent gets no mouse events from the frame", !has("mousedown#") && !has("mouseup#"));
    CHECK("M2 the child gets its own mousedown, in its own coordinates", has("child:1,100,200,"));

    hipe_send(s, HIPE_OP_EVENT_REQUEST, 7, fr, 1, "mousedown");
    hipe_send(s, HIPE_OP_EVENT_REQUEST, 8, fr, 1, "mouseup");
    clickAt(155, 305, fr);
    CHECK("M3 mousedown in the frame reaches the parent in its page coordinates, with its requestor",
          has("mousedown#7:1,155,305,105,205,0;"));
    CHECK("M4 mouseup too", has("mouseup#8:1,155,305,105,205,0;"));
    CHECK("M5 the child still gets its own mousedown", has("child:1,100,200,"));

    clickAt(75, 125, fr);
    CHECK("M6 a press in a frame nested in the child reaches the parent too", has("mousedown#7:1,75,125,"));
    CHECK("M6 and isn't reported as the child's own", !has("child:"));

    hipe_send(s, HIPE_OP_MESSAGE, 0, fr, 2, "scroll", "");
    ready(s, &r);
    usleep(300000);
    clickAt(155, 305, fr);
    CHECK("M7 after the child scrolls by 500px, the parent still gets the press where it is on screen",
          has("mousedown#7:1,155,305,105,205,0;") && has("child:1,100,700,"));

    hipe_send(s, HIPE_OP_EVENT_CANCEL, 0, fr, 2, "mousedown", "");
    clickAt(155, 305, fr);
    CHECK("M8 EVENT_CANCEL stops mousedown; mouseup still comes", !has("mousedown#") && has("mouseup#8:"));

    kill(child, SIGTERM);
    hipe_close_session(s);
    printf("%s: %d failed\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
