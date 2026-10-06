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

/* Keyboard focus: SET_FOCUS and typing into each kind of element, Tab, clicks, the focused element removed, hidden or
 * disabled, the caret kept across SET_FOCUS, SET_FOCUS while the window is inactive, and focus moving between a
 * client and a framed child. Run by run.sh against a private hiped with no window manager, as the top-level client;
 * typing, clicks and window activation use xdotool on $DISPLAY. A forked second top-level window ("focus-decoy")
 * takes the focus to make this one inactive, and a forked child client fills an <iframe>.
 * Prints PASS/FAIL lines and exits non-zero if anything failed. */
#include <hipe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdarg.h>

static hipe_session s;
static int fails = 0;
static char evlog[8000];
static char v[4000];
static hipe_instruction last;

#define CHECK(name, cond) do { int ok_ = (cond); if(!ok_) fails++; \
    printf("%s %s  [val=%.60s] [ev=%.300s]\n", ok_ ? "PASS" : "FAIL", name, v, evlog); fflush(stdout); } while(0)

/* names for locations, for the event log */
static hipe_loc locs[64]; static const char* names[64]; static int nlocs;
static const char* nameOf(hipe_loc l) {
    if(!l) return "body";
    for(int i = 0; i < nlocs; i++) if(locs[i] == l) return names[i];
    static char b[32]; snprintf(b, sizeof b, "#%llu", (unsigned long long)l); return b;
}
static hipe_loc tag(hipe_loc parent, const char* t, const char* name, const char* text) {
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, parent, 4, t, "", "", text ? text : "");
    hipe_loc l = hipe_newest_location();
    if(name && nlocs < 64) { locs[nlocs] = l; names[nlocs++] = name; }
    return l;
}
static void attr(hipe_loc l, const char* a, const char* val) { hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, l, 2, a, val); }
static void style(hipe_loc l, const char* p, const char* val) { hipe_send(s, HIPE_OP_SET_STYLE, 0, l, 2, p, val); }
static void listen(hipe_loc l) {
    hipe_send(s, HIPE_OP_EVENT_REQUEST, 0, l, 1, "focus");
    hipe_send(s, HIPE_OP_EVENT_REQUEST, 0, l, 1, "blur");
    hipe_send(s, HIPE_OP_EVENT_REQUEST, 0, l, 1, "keydown");
}
static void focus(hipe_loc l) { hipe_send(s, HIPE_OP_SET_FOCUS, 0, l, 0); }

static void drain() {
    hipe_instruction in; hipe_instruction_init(&in);
    while(hipe_next_instruction(s, &in, 0)) {
        if(in.opcode == HIPE_OP_EVENT) {
            char e[96]; snprintf(e, sizeof e, "%.*s@%s%s%.*s ", (int)in.arg_length[0], in.arg[0], nameOf(in.location),
                in.arg_length[1] ? ":" : "", (int)(in.arg_length[1] > 12 ? 12 : in.arg_length[1]), in.arg[1] ? in.arg[1] : "");
            if(strlen(evlog) + strlen(e) < sizeof evlog) strcat(evlog, e);
        }
        hipe_instruction_clear(&in);
    }
}
/* round trip, so everything hiped did before it has reached us; then collect the events */
static void sync_() {
    hipe_instruction_clear(&last);
    hipe_send(s, HIPE_OP_GET_ATTRIBUTE, 0, 0, 1, "x");
    hipe_await_instruction(s, &last, HIPE_OP_ATTRIBUTE_RETURN);
    drain();
}
static void settle(int ms) { usleep(ms * 1000); sync_(); }
static void reset() { sync_(); evlog[0] = 0; }
static int has(const char* e) { return strstr(evlog, e) != NULL; }
static int ends(const char* t) { size_t a = strlen(v), b = strlen(t); return a >= b && !strcmp(v + a - b, t); }

static const char* value(hipe_loc l) {
    hipe_instruction_clear(&last);
    hipe_send(s, HIPE_OP_GET_CONTENT, 0, l, 1, "3");
    hipe_await_instruction(s, &last, HIPE_OP_CONTENT_RETURN);
    drain();
    snprintf(v, sizeof v, "%.*s", (int)last.arg_length[0], last.arg[0] ? last.arg[0] : "");
    return v;
}
static const char* text(hipe_loc l) {
    hipe_instruction_clear(&last);
    hipe_send(s, HIPE_OP_GET_CONTENT, 0, l, 1, "0");
    hipe_await_instruction(s, &last, HIPE_OP_CONTENT_RETURN);
    drain();
    snprintf(v, sizeof v, "%.*s", (int)last.arg_length[0], last.arg[0] ? last.arg[0] : "");
    return v;
}
static void geometry(hipe_loc l, int* x, int* y) {
    hipe_instruction_clear(&last);
    hipe_send(s, HIPE_OP_GET_GEOMETRY, 0, l, 0);
    hipe_await_instruction(s, &last, HIPE_OP_GEOMETRY_RETURN);
    drain();
    *x = atoi(last.arg[0]); *y = atoi(last.arg[1]);
}

/* the X window of a top-level client, by its name */
static const char* xwindow(const char* name) {
    static char ids[2][32]; char* b = ids[strcmp(name, "focus-test") ? 1 : 0]; char cmd[100];
    snprintf(cmd, sizeof cmd, "xdotool search --name '^%s$' | head -1", name);
    FILE* p = popen(cmd, "r"); b[0] = 0; if(p) { if(!fgets(b, 32, p)) b[0] = 0; pclose(p); }
    b[strcspn(b, "\n")] = 0; return b;
}
static void xdotool(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void xdotool(const char* fmt, ...) {
    char cmd[512], a[400]; va_list ap; va_start(ap, fmt); vsnprintf(a, sizeof a, fmt, ap); va_end(ap);
    snprintf(cmd, sizeof cmd, "xdotool %s", a);
    if(system(cmd) != 0) printf("  (xdotool failed: %s)\n", a);
}
static void key(const char* k) { xdotool("key %s", k); settle(300); }
static void activate() { const char* w = xwindow("focus-test"); xdotool("windowraise %s windowfocus --sync %s", w, w); settle(300); }
static void deactivate() { xdotool("windowfocus --sync %s", xwindow("focus-decoy")); settle(300); } /* not raised: still visible */
static int isActive() {
    char b[32] = ""; FILE* p = popen("xdotool getwindowfocus", "r");
    if(p) { if(!fgets(b, sizeof b, p)) b[0] = 0; pclose(p); }
    b[strcspn(b, "\n")] = 0; return !strcmp(b, xwindow("focus-test"));
}
static void type(const char* t) { xdotool("type --delay 40 '%s'", t); settle(400); }
static void clickAt(int x, int y) { xdotool("mousemove --window %s %d %d sleep 0.1 click 1", xwindow("focus-test"), x, y); settle(400); }
static void click(hipe_loc l) { int x, y; geometry(l, &x, &y); clickAt(x + 8, y + 8); }

/* ---- the framed child, in a forked process, driven over a pipe ---- */
static int toChild[2], fromChild[2]; static FILE* childIn;
static void childMain(const char* key) {
    hipe_session cs = hipe_open_session(key, 0, 0, "focus-child");
    if(!cs) { write(fromChild[1], "nosession\n", 10); _exit(1); }
    hipe_send(cs, HIPE_OP_APPEND_TAG, 0, 0, 4, "input", "", "", "");
    hipe_loc ci = hipe_newest_location();
    char line[64]; FILE* in = fdopen(toChild[0], "r");
    write(fromChild[1], "ready\n", 6);
    while(fgets(line, sizeof line, in)) {
        hipe_instruction r; hipe_instruction_init(&r);
        char out[300] = "ok";
        if(!strncmp(line, "focus", 5)) hipe_send(cs, HIPE_OP_SET_FOCUS, 0, ci, 0);
        else if(!strncmp(line, "val", 3)) {
            hipe_send(cs, HIPE_OP_GET_CONTENT, 0, ci, 1, "3");
            hipe_await_instruction(cs, &r, HIPE_OP_CONTENT_RETURN);
            snprintf(out, sizeof out, "%.*s", (int)r.arg_length[0], r.arg[0] ? r.arg[0] : "");
        } else if(!strncmp(line, "geo", 3)) {
            hipe_send(cs, HIPE_OP_GET_GEOMETRY, 0, ci, 0);
            hipe_await_instruction(cs, &r, HIPE_OP_GEOMETRY_RETURN);
            snprintf(out, sizeof out, "%.*s %.*s", (int)r.arg_length[0], r.arg[0], (int)r.arg_length[1], r.arg[1]);
        } else if(!strncmp(line, "quit", 4)) break;
        hipe_instruction_clear(&r);
        strcat(out, "\n"); write(fromChild[1], out, strlen(out));
    }
    hipe_close_session(cs); _exit(0);
}
static const char* child(const char* cmd) {
    static char b[300]; FILE* f = childIn;
    write(toChild[1], cmd, strlen(cmd)); write(toChild[1], "\n", 1);
    if(!fgets(b, sizeof b, f)) b[0] = 0;
    b[strcspn(b, "\n")] = 0; snprintf(v, sizeof v, "%s", b); return b;
}

/* the second top-level window: it only has to exist, until the test ends */
static void decoyMain() {
    hipe_session d = hipe_open_session(0, 0, 0, "focus-decoy");
    if(!d) _exit(1);
    hipe_send(d, HIPE_OP_APPEND_TAG, 0, 0, 4, "div", "", "", "decoy window");
    hipe_instruction in; hipe_instruction_init(&in);
    while(hipe_next_instruction(d, &in, 1)) hipe_instruction_clear(&in);
    _exit(0);
}
static char* readKey() {
    static char b[256]; FILE* f = fopen(getenv("HIPE_KEYFILE"), "r"); b[0] = 0;
    if(f) { if(!fgets(b, sizeof b, f)) b[0] = 0; fclose(f); } return b;
}

int main() {
    hipe_instruction_init(&last);
    /* the decoy takes the top-level key first; this client waits for the next one */
    if(!getenv("HIPE_KEYFILE")) { fprintf(stderr, "set HIPE_KEYFILE\n"); return 2; }
    char key0[256]; snprintf(key0, sizeof key0, "%s", readKey());
    pid_t decoy = fork();
    if(!decoy) decoyMain();
    for(int i = 0; i < 100 && !strcmp(readKey(), key0); i++) usleep(50000);
    usleep(300000);

    /* ---- SET_FOCUS straight after the window opens ---- */
    s = hipe_open_session(0, 0, 0, "focus-test");
    if(!s) { fprintf(stderr, "no session: %s\n", hipe_last_error(0)); return 2; }
    hipe_send(s, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "input, textarea, div, button", "display: block; margin: 4px; font: 16px sans-serif; min-height: 20px; width: 300px");
    hipe_loc i1 = tag(0, "input", "i1", 0); listen(i1);
    hipe_send(s, HIPE_OP_EVENT_REQUEST, 0, 0, 1, "keydown"); /* body */
    focus(i1);
    sync_();
    usleep(300000); /* window mapping */
    CHECK("T1 the new window has the keyboard focus", isActive());
    type("abc");
    value(i1); CHECK("T1 SET_FOCUS right after opening: typing reaches the input", !strcmp(v, "abc"));
    CHECK("T1 focus event fired", has("focus@i1"));
    CHECK("T1 keydown also reported on body (propagation)", has("keydown@body"));

    hipe_loc i2 = tag(0, "input", "i2", 0); attr(i2, "value", "hello"); listen(i2);
    hipe_loc ta = tag(0, "textarea", "ta", 0); listen(ta);
    hipe_loc ce = tag(0, "div", "ce", "edit:"); attr(ce, "contenteditable", "true"); listen(ce);
    hipe_loc td = tag(0, "div", "td", "tabindex div"); attr(td, "tabindex", "0"); listen(td);
    hipe_loc pd = tag(0, "div", "pd", "plain div"); listen(pd);
    hipe_loc bt = tag(0, "button", "bt", "button"); listen(bt); hipe_send(s, HIPE_OP_EVENT_REQUEST, 0, bt, 1, "click");
    hipe_loc di = tag(0, "input", "di", 0); attr(di, "disabled", "disabled"); listen(di);
    hipe_loc hi = tag(0, "input", "hi", 0); style(hi, "display", "none"); listen(hi);
    hipe_loc box = tag(0, "div", "box", 0);
    reset();

    /* ---- active window ---- */
    {
        focus(i2); settle(100); type("X"); value(i2);
        CHECK("T2 first SET_FOCUS on an input with a value: caret at the end", !strcmp(v, "helloX"));
        CHECK("T2 i1 blurred, i2 focused", has("blur@i1") && has("focus@i2"));
        value(i1); CHECK("T2 i1 unchanged", !strcmp(v, "abc"));
        reset();

        focus(ta); type("t1"); value(ta); CHECK("T3 textarea", !strcmp(v, "t1")); reset();
        hipe_loc ta2 = tag(0, "textarea", "ta2", "text"); focus(ta2); settle(100); type("A"); value(ta2);
        CHECK("T3b first SET_FOCUS on a textarea with text: caret at the start", !strcmp(v, "Atext")); reset();
        hipe_loc fi2 = tag(0, "input", "fi2", 0); attr(fi2, "value", "abc"); attr(fi2, "value", "xyz"); focus(fi2); settle(100);
        type("B"); value(fi2); CHECK("T3c first SET_FOCUS on an input whose value changed: caret at the end", !strcmp(v, "xyzB")); reset();
        focus(ce); type("ce"); text(ce); CHECK("T4 contenteditable", strstr(v, "ce") != NULL); reset();
        focus(td); settle(100); type("q"); CHECK("T5 tabindex div gets focus and keys", has("focus@td") && has("keydown@td")); reset();

        focus(pd); settle(100); type("w");
        CHECK("T6 SET_FOCUS on a non-focusable div does nothing", !has("blur@td") && has("keydown@td")); reset();
        focus(i1); settle(100); reset(); focus(0); settle(100); type("y");
        CHECK("T7 SET_FOCUS on location 0 (body) does nothing", !has("blur@i1") && has("keydown@i1")); reset();

        focus(bt); settle(100); key("space"); settle(100);
        CHECK("T8 button focused, Space clicks it", has("focus@bt") && has("click@bt")); reset();

        focus(i1); settle(100); reset();
        focus(di); settle(100); type("d"); value(di);
        CHECK("T9 disabled input refuses focus", !has("focus@di") && !strcmp(v, ""));
        reset();
        focus(hi); settle(100); type("h"); value(hi);
        CHECK("T10 display:none input refuses focus", !has("focus@hi") && !strcmp(v, ""));
        reset();

        /* focused element hidden / removed */
        focus(i2); settle(100); reset();
        style(i2, "display", "none"); settle(200);
        CHECK("T11 focused input hidden with display:none is blurred", has("blur@i2"));
        value(i2); char hb[sizeof v]; snprintf(hb, sizeof hb, "%s", v);
        type("z"); value(i2); CHECK("T11 then typing goes to body, not the hidden input", !strcmp(v, hb) && has("keydown@body:90"));
        style(i2, "display", "block"); reset();
        focus(i1); settle(100); reset();
        attr(i1, "disabled", "disabled"); settle(200); CHECK("T11b focused input disabled is blurred", has("blur@i1"));
        hipe_send(s, HIPE_OP_REMOVE_ATTRIBUTE, 0, i1, 1, "disabled");
        focus(i1); settle(100); style(i1, "visibility", "hidden"); settle(200); CHECK("T11c focused input made visibility:hidden is blurred", has("blur@i1"));
        style(i1, "visibility", "visible"); reset();
        focus(i1); settle(100); style(i1, "color", "red"); settle(200); CHECK("T11d an unrelated style change doesn't blur", !has("blur@i1")); reset();

        hipe_loc tmp = tag(box, "input", "tmp", 0); listen(tmp); focus(tmp); settle(100); reset();
        hipe_send(s, HIPE_OP_DELETE, 0, tmp, 0); settle(200);
        type("k"); CHECK("T12 after deleting the focused input, keys still reach body", has("keydown@body"));
        focus(i1); settle(100); type("m"); value(i1); CHECK("T12 SET_FOCUS afterwards works", ends("m")); reset();

        tmp = tag(box, "input", "tmp2", 0); listen(tmp); focus(tmp); settle(100); reset();
        hipe_send(s, HIPE_OP_CLEAR, 0, box, 0); settle(200);
        type("k"); CHECK("T13 after CLEAR of the focused input's parent, keys reach body", has("keydown@body"));
        focus(i1); settle(100); type("n"); value(i1); CHECK("T13 SET_FOCUS afterwards works", ends("n")); reset();

        focus(i1); focus(i1); focus(i1); settle(200); type("o"); value(i1);
        CHECK("T14 repeated SET_FOCUS on the focused element: no extra focus events, typing fine", !has("focus@i1") && ends("no")); reset();

        /* Tab */
        focus(i1); settle(100); reset();
        key("Tab"); CHECK("T15 Tab from i1 moves focus to i2", has("blur@i1") && has("focus@i2")); reset();
        key("shift+Tab"); CHECK("T15 Shift+Tab moves back to i1", has("focus@i1")); reset();
        hipe_send(s, HIPE_OP_EVENT_REQUEST, 0, 0, 2, "keydown", "9,0");
        key("Tab"); CHECK("T16 cancelled Tab keeps focus on i1", !has("blur@i1") && has("keydown@body:9"));
        hipe_send(s, HIPE_OP_EVENT_REQUEST, 0, 0, 2, "keydown", "-"); reset();

        /* clicks */
        click(i2); CHECK("T17 click on i2 focuses it", has("focus@i2"));
        type("c"); reset();
        value(i2); char b18[sizeof v]; snprintf(b18, sizeof b18, "%s", v);
        click(pd); type("p"); value(i2);
        CHECK("T18 click on a non-focusable div takes the focus off i2; keys go to body", has("blur@i2") && !strcmp(v, b18) && has("keydown@body:80")); reset();
        click(td); CHECK("T18b click on tabindex div focuses it", has("focus@td")); reset();

        /* the toolbar pattern: user types in a field, clicks a button, the client puts focus back */
        hipe_send(s, HIPE_OP_SET_TEXT, 0, ta, 1, ""); focus(ta); settle(100); type("abc");
        click(bt); focus(ta); settle(100); type("Z"); value(ta);
        CHECK("T19 textarea: typed abc, clicked a button, SET_FOCUS back, typed Z: caret restored", strstr(v, "abcZ") != NULL); reset();
        click(i1); key("End"); type("xy");
        click(bt); focus(i1); settle(100); type("Z"); value(i1);
        CHECK("T19 input: typed xy at the end, clicked a button, SET_FOCUS back, typed Z: caret restored", ends("xyZ") && strlen(v) > 3); reset();
        click(ce); key("End"); type("Q");
        click(bt); focus(ce); settle(100); type("Z"); text(ce);
        CHECK("T19 contenteditable: typed Q at the end, clicked a button, SET_FOCUS back, typed Z: caret restored", strstr(v, "QZ") != NULL); reset();
        /* contenteditable: focus moved to another field (selection leaves it), then SET_FOCUS back */
        click(ce); key("End"); type("R");
        click(i2); type("s"); focus(ce); settle(100); type("T"); text(ce);
        CHECK("T19b contenteditable: typed R, typed in another input, SET_FOCUS back, typed T: caret restored", strstr(v, "RT") != NULL); reset();
        /* textarea: caret set by CARAT_POSITION while unfocused, then SET_FOCUS */
        focus(i1); settle(100); value(ta); char tv[sizeof v]; snprintf(tv, sizeof tv, "%s", v);
        hipe_send(s, HIPE_OP_CARAT_POSITION, 0, ta, 2, "1", "1"); focus(ta); settle(100); type("C"); value(ta);
        CHECK("T20 CARAT_POSITION then SET_FOCUS: typing at the set position", strlen(v) == strlen(tv) + 1 && v[1] == 'C'); reset();
        hipe_loc fi = tag(0, "input", "fi", 0); attr(fi, "value", "abcdef"); focus(i1); settle(100);
        hipe_send(s, HIPE_OP_CARAT_POSITION, 0, fi, 2, "2", "2"); focus(fi); settle(100); type("Q"); value(fi);
        CHECK("T20b input never focused: CARAT_POSITION then SET_FOCUS types at the set position", !strcmp(v, "abQcdef")); reset();
    }

    /* ---- inactive window: deactivated by focusing the decoy, activated again without a click ---- */
    {
        focus(i1); hipe_send(s, HIPE_OP_CARAT_POSITION, 0, i1, 2, "999", "999"); settle(100); reset();
        deactivate();
        CHECK("D1 window deactivated: the focused input gets a blur event", has("blur@i1"));
        reset();
        activate();
        CHECK("D1 window activated again (no click): the input gets a focus event", has("focus@i1"));
        value(i1); char before[sizeof v]; snprintf(before, sizeof before, "%s", v);
        type("1"); value(i1);
        CHECK("D1 typing after reactivation goes back to i1, caret kept at the end", strlen(v) == strlen(before) + 1 && ends("1")); reset();

        deactivate(); reset();
        focus(ta); settle(200);
        activate();
        value(ta); snprintf(before, sizeof before, "%s", v);
        type("2"); value(ta); CHECK("D2 SET_FOCUS while inactive, reactivate, type: lands in ta", strlen(v) == strlen(before) + 1); reset();

        hipe_loc tmp = tag(box, "input", "tmp3", 0); listen(tmp);
        deactivate();
        focus(tmp); settle(100); reset();
        activate(); type("3"); value(tmp); CHECK("D3 new input focused while inactive gets the keys", !strcmp(v, "3")); reset();

        deactivate();
        hipe_send(s, HIPE_OP_DELETE, 0, tmp, 0); settle(200);
        activate(); reset();
        type("4"); CHECK("D4 focused input deleted while inactive: keys reach body after reactivation", has("keydown@body"));
        focus(i2); settle(100); value(i2); snprintf(before, sizeof before, "%s", v);
        type("5"); value(i2); CHECK("D4 SET_FOCUS afterwards works", strlen(v) == strlen(before) + 1 && strchr(v, '5')); reset();

        /* several moves while inactive: only the last element gets the keys */
        {
            char v1[sizeof v], v2[sizeof v], v3[sizeof v];
            focus(i1); settle(100); deactivate(); reset();
            focus(ta); settle(100); focus(ce); settle(100); focus(i2); settle(100); focus(ta); settle(100); focus(i2); settle(200);
            value(i1); snprintf(v1, sizeof v1, "%s", v); value(ta); snprintf(v2, sizeof v2, "%s", v); text(ce); snprintf(v3, sizeof v3, "%s", v);
            value(i2); char b2[sizeof v]; snprintf(b2, sizeof b2, "%s", v);
            activate(); type("w");
            value(i2); int okI2 = strlen(v) == strlen(b2) + 1 && strchr(v, 'w');
            value(i1); int ok1 = !strcmp(v, v1); value(ta); int ok2 = !strcmp(v, v2); text(ce); int ok3 = !strcmp(v, v3);
            CHECK("D7 after moving focus 5 times while inactive, keys go to the last element only", okI2 && ok1 && ok2 && ok3);
            reset();
        }

        /* click into the inactive window */
        focus(i1); settle(100);
        deactivate(); reset();
        click(i2);
        if(isActive()) {
            value(i2); snprintf(before, sizeof before, "%s", v);
            type("6"); value(i2); CHECK("D5 click activated the window and typing reaches i2", strlen(v) == strlen(before) + 1);
        } else {
            printf("FAIL D5 clicking the inactive hiped window did not make it the keyboard-focused window\n"); fails++;
            activate();
        }
        reset();
        /* after the click-activation: wait, re-activate, click again */
        usleep(1000000); type("7"); CHECK("D6 a second later, keys reach the page", has("keydown@"));
        reset(); deactivate(); activate(); type("8");
        CHECK("D6 after deactivate+activate, keys reach the page", has("keydown@")); reset();
        click(i2); type("9"); CHECK("D6 after a click in the active window, keys reach the page", has("keydown@")); reset();
    }

    /* ---- a framed child client ---- */
    {
        hipe_loc fr = tag(0, "iframe", "frame", 0);
        style(fr, "width", "320px"); style(fr, "height", "60px"); style(fr, "display", "block");
        listen(fr);
        hipe_instruction_clear(&last);
        hipe_send(s, HIPE_OP_GET_FRAME_KEY, 0, fr, 0);
        hipe_await_instruction(s, &last, HIPE_OP_KEY_RETURN);
        char key[256]; snprintf(key, sizeof key, "%.*s", (int)last.arg_length[0], last.arg[0]);
        pipe(toChild); pipe(fromChild);
        pid_t pid = fork();
        if(!pid) childMain(key);
        { char b[64]; FILE* f0 = fdopen(fromChild[0], "r"); if(!fgets(b, sizeof b, f0)) b[0] = 0; childIn = f0; }
        focus(i1); settle(300); reset();
        child("focus"); settle(300);
        type("f"); child("val"); CHECK("F1 the child's SET_FOCUS takes the keys", !strcmp(v, "f"));
        reset();
        focus(i1); settle(200); value(i1); char before[sizeof v]; snprintf(before, sizeof before, "%s", v);
        type("g"); value(i1); CHECK("F2 parent SET_FOCUS i1 takes keys back from the child", ends("g"));
        child("val"); snprintf(before, sizeof before, "%s", v);
        focus(fr); settle(200); type("h"); child("val");
        CHECK("F3 SET_FOCUS on the iframe returns keys to the child's input", strlen(v) == strlen(before) + 1); reset();
        snprintf(before, sizeof before, "%s", v);
        deactivate(); activate();
        type("i"); child("val"); CHECK("F4 focus stays in the child across deactivate/activate", strlen(v) == strlen(before) + 1);
        focus(i1); settle(200);
        child("geo"); int cx = atoi(v), cy = atoi(strchr(v, ' ') ? strchr(v, ' ') + 1 : "0");
        int fx, fy; geometry(fr, &fx, &fy);
        child("val"); snprintf(before, sizeof before, "%s", v);
        clickAt(fx + cx + 8, fy + cy + 8);
        type("j"); child("val"); CHECK("F5 click into the child's input moves keys there", strlen(v) == strlen(before) + 1);
        /* while inactive: into the child, then back to the parent, then into the child again */
        deactivate();
        child("focus"); settle(200); child("val"); snprintf(before, sizeof before, "%s", v);
        activate(); type("k"); child("val");
        CHECK("F6 child SET_FOCUS while window inactive: child gets the keys after activation", strlen(v) == strlen(before) + 1);
        deactivate();
        child("focus"); settle(100); focus(i1); settle(100); focus(fr); settle(200);
        child("val"); snprintf(before, sizeof before, "%s", v); value(i1); char p1[sizeof v]; snprintf(p1, sizeof p1, "%s", v);
        activate(); type("l"); child("val"); int okc = strlen(v) == strlen(before) + 1;
        value(i1); CHECK("F6 child, parent i1, iframe while inactive: keys go back to the child's input", okc && !strcmp(v, p1));
        reset();
        write(toChild[1], "quit\n", 5); waitpid(pid, 0, 0);
    }

    printf("%s: %d failed\n", fails ? "FAILED" : "OK", fails);
    hipe_close_session(s);
    kill(decoy, SIGTERM); waitpid(decoy, 0, 0);
    return fails ? 1 : 0;
}
