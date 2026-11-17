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

/* The Tab order: each client's <body> is a Tab stop before its first element, top-level or framed; Tab past the last
 * element of the window wraps round; a client opts out with tabindex -1 on its body (even as its first instruction).
 * Also: clicking empty space and SET_FOCUS on location 0 focus the body. Run by run.sh against a private hiped with
 * no window manager, as the top-level client, with framed clients opened from the same process; keys and clicks use
 * xdotool on $DISPLAY. Prints PASS/FAIL lines and exits non-zero if anything failed. */
#include <hipe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails = 0;
static char evlog[4000]; /* "focus@P1 blur@P1 focus@C.body ..." across all sessions, in the order collected */

#define CHECK(name, cond) do { int ok_ = (cond); if(!ok_) fails++; \
    printf("%s %s  [ev=%.300s]\n", ok_ ? "PASS" : "FAIL", name, evlog); fflush(stdout); } while(0)

/* a session, with names for its locations (location 0 is "<prefix>body") */
typedef struct { hipe_session s; const char* prefix; hipe_loc locs[16]; const char* names[16]; int n; } client;
static client P = {0, ""}, C1 = {0, "C."}, C2 = {0, "D."}, C3 = {0, "E."};
static client* all[] = {&P, &C1, &C2, &C3};

static const char* nameOf(client* c, hipe_loc l) {
    static char b[48];
    if(!l) { snprintf(b, sizeof b, "%sbody", c->prefix); return b; }
    for(int i = 0; i < c->n; i++) if(c->locs[i] == l) { snprintf(b, sizeof b, "%s%s", c->prefix, c->names[i]); return b; }
    snprintf(b, sizeof b, "%s#%llu", c->prefix, (unsigned long long)l); return b;
}
static void listen(client* c, hipe_loc l) {
    hipe_send(c->s, HIPE_OP_EVENT_REQUEST, 0, l, 1, "focus");
    hipe_send(c->s, HIPE_OP_EVENT_REQUEST, 0, l, 1, "blur");
}
static hipe_loc tag(client* c, const char* t, const char* name) {
    hipe_send(c->s, HIPE_OP_APPEND_TAG, 0, 0, 4, t, "", "", "");
    hipe_loc l = hipe_newest_location();
    c->locs[c->n] = l; c->names[c->n++] = name;
    if(strcmp(t, "iframe")) listen(c, l);
    return l;
}
static void drain(client* c) {
    hipe_instruction in; hipe_instruction_init(&in);
    while(hipe_next_instruction(c->s, &in, 0) > 0) {
        if(in.opcode == HIPE_OP_EVENT) {
            char e[96]; snprintf(e, sizeof e, "%.*s@%s ", (int)in.arg_length[0], in.arg[0], nameOf(c, in.location));
            if(strlen(evlog) + strlen(e) < sizeof evlog) strcat(evlog, e);
        }
        hipe_instruction_clear(&in);
    }
}
/* a round trip on every open session, so everything hiped did before has reached us; then collect the events */
static void sync_() {
    for(int i = 0; i < 4; i++) if(all[i]->s) {
        hipe_instruction r; hipe_instruction_init(&r);
        hipe_send(all[i]->s, HIPE_OP_GET_ATTRIBUTE, 0, 0, 1, "x");
        hipe_await_instruction(all[i]->s, &r, HIPE_OP_ATTRIBUTE_RETURN);
        hipe_instruction_clear(&r);
        drain(all[i]);
    }
}
static void settle(int ms) { usleep(ms * 1000); sync_(); }
static void reset() { sync_(); evlog[0] = 0; }
static int has(const char* e) { return strstr(evlog, e) != NULL; }
static int hasFocus(client* c, hipe_loc l) {
    hipe_instruction r; hipe_instruction_init(&r);
    hipe_send(c->s, HIPE_OP_HAS_FOCUS, 0, l, 0);
    hipe_await_instruction(c->s, &r, HIPE_OP_FOCUS_RETURN);
    int f = r.arg[0] && r.arg_length[0] == 1 && r.arg[0][0] == '1';
    hipe_instruction_clear(&r); drain(c);
    return f;
}
static void xdotool(const char* a) {
    char cmd[300]; snprintf(cmd, sizeof cmd, "xdotool %s", a);
    if(system(cmd) != 0) printf("  (xdotool failed: %s)\n", a);
}
static void key(const char* k) { char a[64]; snprintf(a, sizeof a, "key %s", k); xdotool(a); settle(300); }

/* the focus events (only) of the Tab presses, e.g. "focus@B focus@body focus@A": checks the whole route */
static char route[600];
static const char* tabs(const char* k, int n) {
    route[0] = 0;
    for(int i = 0; i < n; i++) {
        reset(); key(k);
        const char* f = evlog; char one[64] = "-";
        while((f = strstr(f, "focus@"))) { /* the last focus event of this press */
            if(f == evlog || f[-1] == ' ') { int len = strcspn(f, " "); snprintf(one, sizeof one, "%.*s", len, f); }
            f++;
        }
        if(route[0]) strcat(route, " ");
        strcat(route, one);
    }
    printf("  %s x%d: %s\n", k, n, route);
    return route;
}

/* a framed client in the iframe fr of P, opened from this process */
static void frameClient(client* c, hipe_loc fr) {
    hipe_instruction r; hipe_instruction_init(&r);
    hipe_send(P.s, HIPE_OP_GET_FRAME_KEY, 0, fr, 0);
    hipe_await_instruction(P.s, &r, HIPE_OP_KEY_RETURN);
    char k[256]; snprintf(k, sizeof k, "%.*s", (int)r.arg_length[0], r.arg[0]);
    hipe_instruction_clear(&r);
    c->s = hipe_open_session(k, 0, 0, "tab-child");
    if(!c->s) { fprintf(stderr, "no framed session: %s\n", hipe_last_error(0)); exit(2); }
}

int main() {
    P.s = hipe_open_session(0, 0, 0, "tab-test");
    if(!P.s) { fprintf(stderr, "no session: %s\n", hipe_last_error(0)); return 2; }
    hipe_send(P.s, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "input", "display: block; margin: 4px; width: 300px");
    hipe_loc A = tag(&P, "input", "A"), B = tag(&P, "input", "B");
    listen(&P, 0);
    sync_(); usleep(300000); /* window mapping */
    { FILE* p = popen("xdotool search --name '^tab-test$' | head -1", "r"); char w[32] = "";
      if(p) { if(!fgets(w, sizeof w, p)) w[0] = 0; pclose(p); } w[strcspn(w, "\n")] = 0;
      char a[100]; snprintf(a, sizeof a, "windowfocus --sync %s", w); xdotool(a);
      /* empty space, well below the inputs */
      snprintf(a, sizeof a, "mousemove --window %s 500 500", w); xdotool(a); }

    /* ---- top-level ---- */
    hipe_send(P.s, HIPE_OP_SET_FOCUS, 0, A, 0); settle(200);
    CHECK("B1 Tab: A, B, the body, A, B", !strcmp(tabs("Tab", 4), "focus@B focus@body focus@A focus@B"));
    hipe_send(P.s, HIPE_OP_SET_FOCUS, 0, A, 0); settle(200);
    CHECK("B1 Shift+Tab: A, the body, B, A", !strcmp(tabs("shift+Tab", 3), "focus@body focus@B focus@A"));
    reset(); key("Tab"); key("Tab"); /* A -> B -> body */
    CHECK("B2 at the body stop B was blurred", has("blur@B") && has("focus@body"));
    CHECK("B2 HAS_FOCUS: the body 1, B 0", hasFocus(&P, 0) && !hasFocus(&P, B));
    hipe_send(P.s, HIPE_OP_SET_FOCUS, 0, A, 0); settle(200); reset();
    xdotool("click 1"); settle(300);
    CHECK("B3 clicking empty space blurs A and focuses the body", has("blur@A") && has("focus@body") && hasFocus(&P, 0));
    hipe_send(P.s, HIPE_OP_SET_FOCUS, 0, B, 0); settle(200); reset();
    hipe_send(P.s, HIPE_OP_SET_FOCUS, 0, 0, 0); settle(200);
    CHECK("B4 SET_FOCUS on location 0 focuses the body", has("blur@B") && has("focus@body"));
    hipe_send(P.s, HIPE_OP_SET_ATTRIBUTE, 0, 0, 2, "tabindex", "-1");
    hipe_send(P.s, HIPE_OP_SET_FOCUS, 0, A, 0); settle(200);
    CHECK("B5 opted out (tabindex -1): Tab goes A, B, A", !strcmp(tabs("Tab", 3), "focus@B focus@A focus@B"));
    CHECK("B5 ...and Shift+Tab B, A, B", !strcmp(tabs("shift+Tab", 3), "focus@A focus@B focus@A"));
    hipe_send(P.s, HIPE_OP_REMOVE_ATTRIBUTE, 0, 0, 1, "tabindex");
    hipe_send(P.s, HIPE_OP_SET_FOCUS, 0, A, 0); settle(200);
    CHECK("B6 tabindex removed: Tab goes A, B, A", !strcmp(tabs("Tab", 3), "focus@B focus@A focus@B"));
    hipe_send(P.s, HIPE_OP_SET_ATTRIBUTE, 0, 0, 2, "tabindex", "0");
    hipe_send(P.s, HIPE_OP_SET_FOCUS, 0, A, 0); settle(200);
    CHECK("B7 tabindex 0 again: the body is a stop again", !strcmp(tabs("Tab", 3), "focus@B focus@body focus@A"));

    /* ---- a framed client between two of the parent's inputs: P1 [C.body C.A C.B] P2 ---- */
    hipe_send(P.s, HIPE_OP_CLEAR, 0, 0, 0); P.n = 0;
    hipe_send(P.s, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "iframe", "display: block; width: 400px; height: 90px");
    hipe_loc P1 = tag(&P, "input", "P1"), f1 = tag(&P, "iframe", "f1"); tag(&P, "input", "P2");
    frameClient(&C1, f1);
    hipe_send(C1.s, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "input", "display: block; margin: 4px");
    tag(&C1, "input", "A"); tag(&C1, "input", "B"); listen(&C1, 0);
    settle(500);
    hipe_send(P.s, HIPE_OP_SET_FOCUS, 0, P1, 0); settle(200);
    CHECK("F1 Tab through a framed client: its body is a stop before its first element",
        !strcmp(tabs("Tab", 6), "focus@C.body focus@C.A focus@C.B focus@P2 focus@body focus@P1"));
    CHECK("F1 Shift+Tab: the same in reverse",
        !strcmp(tabs("shift+Tab", 6), "focus@body focus@P2 focus@C.B focus@C.A focus@C.body focus@P1"));

    /* ---- P1 [D: opted out with its first instruction; D.A D.B] [E: nothing focusable] P2 ---- */
    hipe_close_session(C1.s); C1.s = 0;
    hipe_send(P.s, HIPE_OP_CLEAR, 0, 0, 0); P.n = 0;
    P1 = tag(&P, "input", "P1"); hipe_loc f2 = tag(&P, "iframe", "f2"), f3 = tag(&P, "iframe", "f3"); tag(&P, "input", "P2");
    frameClient(&C2, f2);
    hipe_send(C2.s, HIPE_OP_SET_ATTRIBUTE, 0, 0, 2, "tabindex", "-1");
    tag(&C2, "input", "A"); tag(&C2, "input", "B"); listen(&C2, 0);
    frameClient(&C3, f3);
    hipe_send(C3.s, HIPE_OP_APPEND_TEXT, 0, 0, 1, "nothing to focus"); listen(&C3, 0);
    settle(500);
    hipe_send(P.s, HIPE_OP_SET_FOCUS, 0, P1, 0); settle(200);
    CHECK("F2 a framed client opted out (in its first instruction) has no body stop; one with nothing focusable has only its body",
        !strcmp(tabs("Tab", 5), "focus@D.A focus@D.B focus@E.body focus@P2 focus@body"));

    printf("%s: %d failed\n", fails ? "FAILED" : "OK", fails);
    hipe_close_session(C2.s); hipe_close_session(C3.s); hipe_close_session(P.s);
    return fails ? 1 : 0;
}
