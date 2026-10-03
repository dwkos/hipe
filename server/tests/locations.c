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

/* Location numbering tests (doc/design/location-numbering.md). Run by run.sh against a private hiped; clicks are
 * made with xdotool on $DISPLAY. Prints PASS/FAIL lines and exits non-zero if anything failed. */

#include <hipe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

static hipe_session s;
static char v[8192];
static int fails = 0;

#define CHECK(name, cond) do { int ok_ = (cond); if(!ok_) fails++; \
    printf("%s %s  [%.100s]\n", ok_ ? "PASS" : "FAIL", name, v); fflush(stdout); } while(0)

static const char* reply(hipe_session ss, hipe_loc l, char op, char ret, const char* a, int idx) {
    hipe_instruction in; hipe_instruction_init(&in);
    hipe_send(ss, op, 0, l, 1, a);
    if(hipe_await_instruction(ss, &in, ret) < 0) { strcpy(v, "<disconnected>"); return v; }
    snprintf(v, sizeof v, "%.*s", (int)in.arg_length[idx], in.arg[idx]);
    hipe_instruction_clear(&in);
    return v;
}
#define content(ss, l, m) reply(ss, l, HIPE_OP_GET_CONTENT, HIPE_OP_CONTENT_RETURN, m, 0)

static hipe_loc lookup(hipe_session ss, char op, hipe_loc l, const char* arg) {
    hipe_instruction in; hipe_instruction_init(&in);
    hipe_send(ss, op, 0, l, arg ? 1 : 0, arg);
    hipe_await_instruction(ss, &in, HIPE_OP_LOCATION_RETURN);
    hipe_loc r = in.location;
    hipe_instruction_clear(&in);
    snprintf(v, sizeof v, "%llu", (unsigned long long)r);
    return r;
}
#define byId(ss, id) lookup(ss, HIPE_OP_GET_BY_ID, 0, id)

static hipe_session openSession(const char* key, const char* name) {
    hipe_session ss = hipe_open_session(key, 0, 0, name);
    if(!ss) { fprintf(stderr, "could not open session %s: %s\n", name, hipe_last_error(0)); exit(2); }
    return ss;
}

/* expects ss to have been disconnected for a protocol violation; leaves the reason in v */
static void awaitFatal(hipe_session ss) {
    hipe_instruction in; hipe_instruction_init(&in);
    hipe_send(ss, HIPE_OP_GET_CONTENT, 0, 0, 1, "0");
    int r = hipe_await_instruction(ss, &in, HIPE_OP_CONTENT_RETURN);
    snprintf(v, sizeof v, "%s", r < 0 ? hipe_last_error(ss) : "<still connected>");
}

static hipe_loc tag(hipe_session ss, hipe_loc parent, const char* t, const char* id, const char* text) {
    hipe_send(ss, HIPE_OP_APPEND_TAG, 0, parent, 4, t, id ? id : "", "", text ? text : "");
    return hipe_newest_location();
}

/* returns the location of the next click event within a second, or 0 */
static hipe_loc nextClick(void) {
    hipe_instruction in; hipe_instruction_init(&in);
    for(int i = 0; i < 20; i++) {
        while(hipe_next_instruction(s, &in, 0) > 0) {
            if(in.opcode == HIPE_OP_EVENT && in.arg_length[0] == 5 && !strncmp(in.arg[0], "click", 5)) {
                hipe_loc l = in.location;
                hipe_instruction_clear(&in);
                return l;
            }
            hipe_instruction_clear(&in);
        }
        usleep(50000);
    }
    return 0;
}

static void* appender(void* arg) {
    hipe_loc parent = *(hipe_loc*)arg;
    for(int i = 0; i < 200; i++) hipe_send(s, HIPE_OP_APPEND_TAG, 0, parent, 4, "span", "", "", "t");
    return 0;
}

static void markupBinding(hipe_loc box) {
    char m[512];
    hipe_loc n = hipe_reserve_location(s);
    snprintf(m, sizeof m, "<p id=\"pa\" hipe-loc=\"%llu\">alpha</p>", (unsigned long long)n);
    hipe_send_markup(s, box, m, 0, &n, 1);
    content(s, n, "0"); CHECK("markup: listed number binds its element", !strcmp(v, "alpha"));
    reply(s, n, HIPE_OP_GET_ATTRIBUTE, HIPE_OP_ATTRIBUTE_RETURN, "hipe-loc", 1); CHECK("markup: attribute removed", !strcmp(v, ""));
    content(s, box, "1"); CHECK("markup: no hipe-loc left in the DOM", !strstr(v, "hipe-loc"));
    CHECK("markup: lookup returns the number", byId(s, "pa") == n);

    hipe_send_markup(s, box, "<p id=\"pb\" hipe-loc=\"999999\">beta</p>", 1, 0, 0);
    content(s, box, "1"); CHECK("markup: unlisted hipe-loc removed", !strstr(v, "hipe-loc") && strstr(v, "beta"));
    CHECK("markup: unlisted element has no number", byId(s, "pb") == 0);

    hipe_loc lost = hipe_reserve_location(s);
    hipe_send_markup(s, box, "<p>gamma</p>", 1, &lost, 1);
    content(s, lost, "0"); CHECK("markup: unused listed number names no element", !strcmp(v, ""));

    hipe_loc d = hipe_reserve_location(s);
    snprintf(m, sizeof m, "<i hipe-loc=\"%llu\">one</i><i hipe-loc=\"%llu\">two</i>", (unsigned long long)d, (unsigned long long)d);
    hipe_send_markup(s, box, m, 1, &d, 1);
    content(s, d, "0"); CHECK("markup: duplicate, first element bound", !strcmp(v, "one"));

    hipe_loc w = hipe_reserve_location(s);
    snprintf(m, sizeof m, "<i hipe-loc=\" %llu\">spaced</i>", (unsigned long long)w);
    hipe_send_markup(s, box, m, 1, &w, 1);
    content(s, w, "0"); CHECK("markup: values must be digits only", !strcmp(v, ""));

    hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, n, 2, "hipe-loc", "5");
    reply(s, n, HIPE_OP_GET_ATTRIBUTE, HIPE_OP_ATTRIBUTE_RETURN, "hipe-loc", 1); CHECK("SET_ATTRIBUTE refuses hipe-loc", !strcmp(v, ""));
}

static void rangesAndOrder(hipe_loc* grpOut, hipe_loc* firstOut, char* big, hipe_loc* list) {
    hipe_loc early = hipe_reserve_location(s);
    hipe_loc first = hipe_reserve_locations(s, 500);
    size_t len = 0;
    for(int i = 0; i < 500; i++) {
        list[i] = first + i;
        len += sprintf(big + len, "<div hipe-loc=\"%llu\">line %d</div>", (unsigned long long)(first + i), i);
    }
    hipe_loc grp = tag(s, 0, "div", "grp", 0);
    hipe_send_markup(s, grp, big, 0, list, 500);
    content(s, first, "0"); CHECK("ranges: first line bound", !strcmp(v, "line 0"));
    content(s, first + 499, "0"); CHECK("ranges: last line bound", !strcmp(v, "line 499"));
    char m[128];
    snprintf(m, sizeof m, "<b hipe-loc=\"%llu\">early</b>", (unsigned long long)early);
    hipe_send_markup(s, grp, m, 1, &early, 1);
    content(s, early, "0"); CHECK("order: number reserved earlier, sent later", !strcmp(v, "early"));
    *grpOut = grp;
    *firstOut = first;
}

static void freeing(hipe_loc grp, hipe_loc first, const char* big, hipe_loc* list) {
    content(s, first + 3, "4"); /* take a mode 4 snapshot */
    char fl[64];
    snprintf(fl, sizeof fl, "%llu-%llu", (unsigned long long)first, (unsigned long long)(first + 499));
    hipe_send(s, HIPE_OP_FREE_LOCATION, 0, 0, 1, fl);
    content(s, first, "0"); CHECK("free: ranged free, number names nothing", !strcmp(v, ""));
    CHECK("free: freed run reused first-fit", hipe_reserve_locations(s, 500) == first);
    hipe_send_markup(s, grp, big, 0, list, 500);
    hipe_instruction in; hipe_instruction_init(&in);
    hipe_send(s, HIPE_OP_GET_CONTENT, 0, first + 3, 1, "4");
    hipe_await_instruction(s, &in, HIPE_OP_CONTENT_RETURN);
    snprintf(v, sizeof v, "%.*s", (int)in.arg_length[3], in.arg[3]);
    hipe_instruction_clear(&in);
    CHECK("free: mode 4 snapshot dropped with the number", !strcmp(v, "full"));
}

static void directTags(void) {
    hipe_loc table = tag(s, 0, "table", 0, 0);
    hipe_loc tr = tag(s, table, "tr", 0, 0);
    CHECK("APPEND_TAG: tr goes straight into a table (no tbody)", lookup(s, HIPE_OP_GET_FIRST_CHILD, table, 0) == tr);
    hipe_loc div = tag(s, 0, "div", 0, 0);
    hipe_loc td = tag(s, div, "td", 0, "cell");
    content(s, td, "0"); CHECK("APPEND_TAG: td in a div is created, not dropped", !strcmp(v, "cell"));
    hipe_loc bad = tag(s, div, "not valid", 0, "x");
    content(s, bad, "0"); CHECK("APPEND_TAG: invalid name binds none", !strcmp(v, ""));
    hipe_loc after = tag(s, div, "span", 0, "after");
    content(s, after, "0"); CHECK("APPEND_TAG: numbering continues after a refused tag", !strcmp(v, "after"));
    hipe_loc svg = tag(s, 0, "svg", 0, 0);
    tag(s, svg, "lineargradient", 0, 0);
    content(s, svg, "1"); CHECK("APPEND_TAG: SVG names get the parser's case", strstr(v, "<linearGradient") != NULL);
    hipe_loc pre1 = tag(s, 0, "pre", 0, "\n\nabc\r\ndef");
    hipe_loc pre2 = tag(s, 0, "pre", 0, 0);
    hipe_send(s, HIPE_OP_SET_TEXT, 0, pre2, 2, "\n\nabc\r\ndef", "0");
    char a[sizeof v];
    content(s, pre2, "0"); memcpy(a, v, sizeof v);
    content(s, pre1, "0"); CHECK("APPEND_TAG text into a pre reads back as with SET_TEXT mode 0", !strcmp(v, a) && !strcmp(v, "\n\nabc\ndef"));
    hipe_send(s, HIPE_OP_INSERT_TAG, 0, after, 4, "em", "", "", "before");
    hipe_loc ins = hipe_newest_location();
    CHECK("INSERT_TAG: element created before the given one", lookup(s, HIPE_OP_GET_PREV_SIBLING, after, 0) == ins);
}

static void events(void) {
    /* clicks by screen position: the frame fills the screen, so page coordinates are screen coordinates */
    hipe_loc b = tag(s, 0, "button", 0, "button");
    hipe_send(s, HIPE_OP_EVENT_REQUEST, 0, b, 1, "click");
    hipe_instruction in; hipe_instruction_init(&in);
    hipe_send(s, HIPE_OP_GET_GEOMETRY, 0, b, 0);
    hipe_await_instruction(s, &in, HIPE_OP_GEOMETRY_RETURN);
    int x = atoi(in.arg[0]) + atoi(in.arg[2]) / 2, y = atoi(in.arg[1]) + atoi(in.arg[3]) / 2;
    hipe_instruction_clear(&in);
    char cmd[128];
    snprintf(cmd, sizeof cmd, "xdotool mousemove %d %d click 1", x, y);

    if(system(cmd) != 0) fprintf(stderr, "xdotool failed\n");
    usleep(400000);
    CHECK("events: a click reports the element's number", nextClick() == b);
    hipe_send(s, HIPE_OP_FREE_LOCATION, 0, b, 0);
    if(system(cmd) != 0) fprintf(stderr, "xdotool failed\n");
    usleep(400000);
    CHECK("events: after FREE_LOCATION the element's events stop", nextClick() == 0);
}

static void frames(void) {
    hipe_loc f1 = tag(s, 0, "iframe", 0, 0);
    hipe_loc f2 = tag(s, 0, "iframe", 0, 0);
    char key1[64], key2[64];
    hipe_instruction in; hipe_instruction_init(&in);
    hipe_send(s, HIPE_OP_GET_FRAME_KEY, 0, f1, 0);
    hipe_await_instruction(s, &in, HIPE_OP_KEY_RETURN);
    snprintf(key1, sizeof key1, "%.*s", (int)in.arg_length[0], in.arg[0]);
    hipe_instruction_clear(&in);
    hipe_send(s, HIPE_OP_GET_FRAME_KEY, 0, f2, 0);
    hipe_await_instruction(s, &in, HIPE_OP_KEY_RETURN);
    snprintf(key2, sizeof key2, "%.*s", (int)in.arg_length[0], in.arg[0]);
    hipe_instruction_clear(&in);

    /* numbered markup inside a framed app, as its very first instruction */
    hipe_session child1 = openSession(key1, "child1");
    hipe_loc n = hipe_reserve_location(child1);
    char m[128];
    snprintf(m, sizeof m, "<p hipe-loc=\"%llu\">framed</p>", (unsigned long long)n);
    hipe_send_markup(child1, 0, m, 0, &n, 1);
    content(child1, n, "0"); CHECK("frames: numbered markup in a framed app, first instruction on the body", !strcmp(v, "framed"));
    hipe_session child2 = openSession(key2, "child2");
    content(child2, 0, "0");

    /* delete then free, and free then delete: the children are disconnected either way */
    hipe_send(s, HIPE_OP_DELETE, 0, f1, 0);
    hipe_send(s, HIPE_OP_FREE_LOCATION, 0, f1, 0);
    hipe_send(s, HIPE_OP_FREE_LOCATION, 0, f2, 0);
    hipe_send(s, HIPE_OP_DELETE, 0, f2, 0); /* freed: names nothing, so nothing is deleted */
    content(s, 0, "0");
    content(child1, 0, "0"); CHECK("frames: deleting an iframe disconnects its client", !strcmp(v, "<disconnected>"));
    content(child2, 0, "0"); CHECK("frames: freeing an iframe's number leaves its client connected", strcmp(v, "<disconnected>") != 0);
    hipe_close_session(child1);
    hipe_close_session(child2);
}

static void fatalCases(void) {
    const char* lists[] = { "1", "9223372036854775809", "5,,6", ",1", "1,", "2-1", "1-3,2", "3,3", "a",
                            "18446744073709551616", "100-4194403" };
    const char* names[] = { "in use", "top bit", "double comma", "leading comma", "trailing comma", "reversed range",
                            "overlap", "repeat", "not a number", "overflow", "cap" };
    for(unsigned k = 0; k < sizeof lists / sizeof *lists; k++) {
        hipe_session f = openSession(0, "fatal");
        hipe_send(f, HIPE_OP_APPEND_TAG, 0, 0, 1, "div"); /* number 1 in use */
        hipe_send(f, HIPE_OP_SET_TEXT, 0, 0, 3, "<p>x</p>", "3", lists[k]);
        awaitFatal(f);
        char name[64];
        snprintf(name, sizeof name, "fatal: %s, reason received", names[k]);
        CHECK(name, strlen(v) > 0 && strcmp(v, "<still connected>") != 0);
        hipe_close_session(f);
    }
    hipe_session f = openSession(0, "fatal");
    hipe_send(f, HIPE_OP_FREE_LOCATION, 0, 0, 1, "3-1");
    awaitFatal(f); CHECK("fatal: malformed FREE_LOCATION list, reason received", strlen(v) > 0 && strcmp(v, "<still connected>") != 0);
    hipe_close_session(f);
}

static void bigMarkup(void) {
    size_t count = 20000;
    hipe_loc first = hipe_reserve_locations(s, count);
    hipe_loc* list = malloc(count * sizeof(hipe_loc));
    char* m = malloc(count * 160);
    size_t len = 0;
    for(size_t i = 0; i < count; i++) {
        list[i] = first + i;
        len += sprintf(m + len, "<div hipe-loc=\"%llu\">line %zu with some text to make the markup larger</div>",
                       (unsigned long long)(first + i), i);
    }
    hipe_loc box = tag(s, 0, "div", 0, 0);
    hipe_send_markup(s, box, m, 0, list, count);
    content(s, first + count - 1, "0");
    char expect[64]; snprintf(expect, sizeof expect, "line %zu with some text to make the markup larger", count - 1);
    snprintf(v + strlen(v), sizeof v - strlen(v), " (%zu bytes)", len);
    CHECK("big: multi-megabyte numbered markup", !strncmp(v, expect, strlen(expect)));
    char fl[64]; snprintf(fl, sizeof fl, "%llu-%llu", (unsigned long long)first, (unsigned long long)(first + count - 1));
    hipe_send(s, HIPE_OP_FREE_LOCATION, 0, 0, 1, fl);
    hipe_send(s, HIPE_OP_DELETE, 0, box, 0);
    free(list); free(m);
}

static void undo(void) {
    hipe_loc ed = tag(s, 0, "div", "ed", 0);
    hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, ed, 2, "contenteditable", "true");
    hipe_loc pn[2] = { hipe_reserve_location(s), hipe_reserve_location(s) };
    char m[256];
    snprintf(m, sizeof m, "<p id=\"e1\" hipe-loc=\"%llu\">first</p><p id=\"e2\" hipe-loc=\"%llu\">second</p>",
             (unsigned long long)pn[0], (unsigned long long)pn[1]);
    hipe_send_markup(s, ed, m, 0, pn, 2);
    hipe_send(s, HIPE_OP_SET_FOCUS, 0, ed, 0);
    hipe_send(s, HIPE_OP_CARAT_POSITION, 0, ed, 2, "5", "12"); /* "\nsecond": merges e2 away */
    hipe_send(s, HIPE_OP_EDIT_ACTION, 0, 0, 2, "t", "");
    CHECK("undo: editing removed the numbered paragraph", byId(s, "e2") == 0);
    hipe_send(s, HIPE_OP_EDIT_ACTION, 0, 0, 1, "z");
    CHECK("undo: the paragraph returns with its number", byId(s, "e2") == pn[1]);
    hipe_send(s, HIPE_OP_CARAT_POSITION, 0, ed, 2, "5", "12");
    hipe_send(s, HIPE_OP_EDIT_ACTION, 0, 0, 2, "t", "");
    hipe_send(s, HIPE_OP_FREE_LOCATION, 0, pn[1], 0);
    hipe_loc reuse = hipe_reserve_location(s);
    snprintf(m, sizeof m, "<span hipe-loc=\"%llu\">newcomer</span>", (unsigned long long)reuse);
    hipe_send_markup(s, ed, m, 1, &reuse, 1);
    hipe_send(s, HIPE_OP_EDIT_ACTION, 0, 0, 1, "z");
    content(s, 0, "0"); CHECK("undo: free, reuse, undo brings the paragraph back", strstr(v, "second") != NULL);
    CHECK("undo: the returned node has no number", byId(s, "e2") == 0);
    content(s, reuse, "0"); CHECK("undo: the number's new holder keeps it", !strcmp(v, "newcomer"));
}

int main(int argc, char** argv) {
    int rebuilds = argc > 1 ? atoi(argv[1]) : 0;
    s = openSession(0, "locations");
    hipe_loc box = tag(s, 0, "div", "box", 0);

    markupBinding(box);
    char* big = malloc(500 * 64);
    hipe_loc list[500], grp, first;
    rangesAndOrder(&grp, &first, big, list);
    freeing(grp, first, big, list);
    if(rebuilds) {
        char fl[64]; snprintf(fl, sizeof fl, "%llu-%llu", (unsigned long long)first, (unsigned long long)(first + 499));
        for(int k = 1; k <= rebuilds; k++) {
            hipe_send(s, HIPE_OP_FREE_LOCATION, 0, 0, 1, fl);
            hipe_reserve_locations(s, 500);
            hipe_send_markup(s, grp, big, 0, list, 500);
            if(k == rebuilds / 6 || k == rebuilds) { /* run.sh samples hiped's memory at each of these */
                content(s, first + 499, "0");
                printf("REBUILT %d\n", k); fflush(stdout);
                sleep(3);
            }
        }
        CHECK("free: repeated rebuilds", !strcmp(v, "line 499"));
    }
    directTags();
    pthread_t t1, t2;
    pthread_create(&t1, 0, appender, &box); pthread_create(&t2, 0, appender, &box);
    pthread_join(t1, 0); pthread_join(t2, 0);
    content(s, 0, "0"); CHECK("threads: two threads appending, still connected", strcmp(v, "<disconnected>") != 0);
    undo();
    bigMarkup();
    hipe_send(s, HIPE_OP_SET_TEXT, 0, 0, 2, "", "0"); /* clear the page for the click tests */
    events();
    frames();
    fatalCases();

    printf("DONE fails=%d\n", fails);
    hipe_close_session(s);
    return fails != 0;
}
