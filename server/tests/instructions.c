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

/* Instructions not covered by the other tests: tree navigation, CLEAR/DELETE, classes and attributes, caret and
 * selection, FIND_TEXT, MEASURE_TEXT, GET_RANGE_GEOMETRY, GET_SRC, scrolling, EDIT_STATUS/EDIT_ACTION and a click
 * event. Run by run.sh against a private hiped, as the top-level client; the click uses xdotool on $DISPLAY.
 * Prints PASS/FAIL lines and exits non-zero if anything failed. */

#include <hipe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static hipe_session s;
static char v[4000];
static int fails = 0;
#define CHECK(name, cond) do { int ok_ = (cond); if(!ok_) fails++; \
    printf("%s %s  [%.100s]\n", ok_ ? "PASS" : "FAIL", name, v); fflush(stdout); } while(0)

static hipe_instruction last; /* the latest reply, for checks on more than one argument */
static const char* argOf(int i) {
    snprintf(v, sizeof v, "%.*s", (int)last.arg_length[i], last.arg[i] ? last.arg[i] : "");
    return v;
}
static void ask(hipe_loc l, char op, char ret, int nargs, const char* a0, const char* a1) {
    hipe_instruction_clear(&last);
    hipe_send(s, op, 0, l, nargs, a0, a1);
    hipe_await_instruction(s, &last, ret);
}
static const char* content(hipe_loc l, const char* mode) {
    ask(l, HIPE_OP_GET_CONTENT, HIPE_OP_CONTENT_RETURN, 1, mode, ""); return argOf(0);
}
static const char* attribute(hipe_loc l, const char* name) {
    ask(l, HIPE_OP_GET_ATTRIBUTE, HIPE_OP_ATTRIBUTE_RETURN, 1, name, ""); return argOf(1);
}
static hipe_loc tag(hipe_loc parent, const char* t, const char* id, const char* cls, const char* text) {
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, parent, 4, t, id, cls, text);
    return hipe_newest_location();
}
static hipe_loc navigate(hipe_loc l, char op) {
    ask(l, op, HIPE_OP_LOCATION_RETURN, 0, "", ""); snprintf(v, sizeof v, "%llu", (unsigned long long)last.location);
    return last.location;
}

static const char png[] = "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52\x00\x00\x00\x01\x00\x00\x00\x01"
    "\x08\x06\x00\x00\x00\x1f\x15\xc4\x89\x00\x00\x00\x0d\x49\x44\x41\x54\x78\x9c\x63\xf8\xcf\xc0\xf0\x1f\x00\x05\x00\x01"
    "\xff\x89\x99\x3d\x1d\x00\x00\x00\x00\x49\x45\x4e\x44\xae\x42\x60\x82";
#define PNG_SIZE 70

int main() {
    s = hipe_open_session(0, 0, 0, "instructions");
    if(!s) return 2;
    hipe_instruction_init(&last);
    hipe_send(s, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, "div", "font: 20px monospace; line-height: 30px; margin: 0; padding: 0; border: 0");

    /* tree navigation */
    hipe_loc list = tag(0, "div", "", "", "");
    hipe_loc a = tag(list, "span", "", "", "A"), b = tag(list, "span", "", "", "B"), c = tag(list, "span", "", "", "C");
    CHECK("GET_FIRST_CHILD", navigate(list, HIPE_OP_GET_FIRST_CHILD) == a);
    CHECK("GET_LAST_CHILD", navigate(list, HIPE_OP_GET_LAST_CHILD) == c);
    CHECK("GET_NEXT_SIBLING", navigate(a, HIPE_OP_GET_NEXT_SIBLING) == b);
    CHECK("GET_PREV_SIBLING", navigate(c, HIPE_OP_GET_PREV_SIBLING) == b);
    CHECK("no next sibling: location 0", navigate(c, HIPE_OP_GET_NEXT_SIBLING) == 0);

    /* CLEAR, DELETE */
    hipe_send(s, HIPE_OP_DELETE, 0, b, 0);
    content(list, "0"); CHECK("DELETE removes the element", !strcmp(v, "AC"));
    hipe_send(s, HIPE_OP_CLEAR, 0, list, 0);
    content(list, "0"); CHECK("CLEAR removes the contents", !strcmp(v, ""));
    hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, list, 2, "title", "kept");
    attribute(list, "title"); CHECK("CLEAR keeps the element and its attributes", !strcmp(v, "kept"));

    /* classes and attributes */
    hipe_send(s, HIPE_OP_TOGGLE_CLASS, 0, list, 1, "on");
    attribute(list, "class"); CHECK("TOGGLE_CLASS adds", !strcmp(v, "on"));
    hipe_send(s, HIPE_OP_TOGGLE_CLASS, 0, list, 1, "on");
    attribute(list, "class"); CHECK("TOGGLE_CLASS removes", !strcmp(v, ""));
    hipe_send(s, HIPE_OP_REMOVE_ATTRIBUTE, 0, list, 1, "title");
    attribute(list, "title"); CHECK("REMOVE_ATTRIBUTE", !strcmp(v, ""));

    /* caret and selection */
    hipe_loc ed = tag(0, "div", "", "", "hello world");
    hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, ed, 2, "contenteditable", "true");
    hipe_send(s, HIPE_OP_CARAT_POSITION, 0, ed, 2, "6", "11");
    ask(ed, HIPE_OP_GET_CARAT_POSITION, HIPE_OP_CARAT_POSITION, 0, "", "");
    char anchor[16]; snprintf(anchor, sizeof anchor, "%s", argOf(0)); argOf(1);
    CHECK("CARAT_POSITION / GET_CARAT_POSITION round trip", !strcmp(anchor, "6") && !strcmp(v, "11"));
    ask(0, HIPE_OP_GET_SELECTION, HIPE_OP_CONTENT_RETURN, 1, "0", ""); argOf(0);
    CHECK("GET_SELECTION returns the selected text", !strcmp(v, "world"));

    /* EDIT_STATUS and EDIT_ACTION on the selection */
    ask(0, HIPE_OP_EDIT_STATUS, HIPE_OP_EDIT_STATUS, 1, "xct", ""); argOf(1);
    CHECK("EDIT_STATUS with a selection: cut, copy, insert available", !strcmp(v, "000"));
    hipe_send(s, HIPE_OP_EDIT_ACTION, 0, 0, 2, "t", "there");
    content(ed, "0"); CHECK("EDIT_ACTION t replaces the selection", !strcmp(v, "hello there"));
    hipe_send(s, HIPE_OP_EDIT_ACTION, 0, 0, 1, "z");
    content(ed, "0"); CHECK("EDIT_ACTION z undoes it", !strcmp(v, "hello world"));
    hipe_send(s, HIPE_OP_CARAT_POSITION, 0, ed, 2, "0", "0");
    ask(0, HIPE_OP_EDIT_STATUS, HIPE_OP_EDIT_STATUS, 1, "xc", ""); argOf(1);
    CHECK("EDIT_STATUS without a selection: no cut or copy", !strcmp(v, "ee"));
    hipe_send(s, HIPE_OP_EDIT_ACTION, 0, 0, 1, "a");
    ask(0, HIPE_OP_GET_SELECTION, HIPE_OP_CONTENT_RETURN, 1, "0", ""); argOf(0);
    CHECK("EDIT_ACTION a selects all", strstr(v, "hello world") != NULL);
    hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, ed, 2, "contenteditable", "false");

    /* FIND_TEXT */
    hipe_loc f = tag(0, "div", "", "", "One two one two ONE twofold");
    ask(f, HIPE_OP_FIND_TEXT, HIPE_OP_FIND_RESULT, 2, "one", "");
    char total[16]; snprintf(total, sizeof total, "%s", argOf(0)); argOf(1);
    CHECK("FIND_TEXT finds every match, case-insensitively, and selects the first", !strcmp(total, "3") && !strcmp(v, "1"));
    ask(f, HIPE_OP_FIND_TEXT, HIPE_OP_FIND_RESULT, 2, "one", ""); argOf(1);
    CHECK("FIND_TEXT again steps to the next match", !strcmp(v, "2"));
    ask(f, HIPE_OP_FIND_TEXT, HIPE_OP_FIND_RESULT, 2, "one", "b"); argOf(1);
    CHECK("FIND_TEXT b steps back", !strcmp(v, "1"));
    ask(f, HIPE_OP_FIND_TEXT, HIPE_OP_FIND_RESULT, 2, "one", "c"); argOf(0);
    CHECK("FIND_TEXT c matches case", !strcmp(v, "1"));
    ask(f, HIPE_OP_FIND_TEXT, HIPE_OP_FIND_RESULT, 2, "two", "w"); argOf(0);
    CHECK("FIND_TEXT w matches whole words only", !strcmp(v, "2"));
    hipe_send(s, HIPE_OP_SET_STYLE, 0, f, 2, "user-select", "none");
    ask(f, HIPE_OP_FIND_TEXT, HIPE_OP_FIND_RESULT, 2, "one", ""); argOf(0);
    CHECK("FIND_TEXT skips user-select: none text", !strcmp(v, "0"));
    ask(f, HIPE_OP_FIND_TEXT, HIPE_OP_FIND_RESULT, 2, "", ""); argOf(1);
    CHECK("FIND_TEXT with empty text selects nothing", !strcmp(v, "0"));

    /* MEASURE_TEXT */
    hipe_loc m = tag(0, "div", "", "", "");
    ask(m, HIPE_OP_MEASURE_TEXT, HIPE_OP_TEXT_METRICS, 1, "a", ""); double w1 = atof(argOf(0));
    ask(m, HIPE_OP_MEASURE_TEXT, HIPE_OP_TEXT_METRICS, 1, "abcd", ""); double w4 = atof(argOf(0));
    CHECK("MEASURE_TEXT: four monospace characters are four times one", w1 > 0 && w4 > 4 * w1 - 0.1 && w4 < 4 * w1 + 0.1);
    argOf(1); CHECK("MEASURE_TEXT height is the line height", atof(v) == 30);
    ask(m, HIPE_OP_MEASURE_TEXT, HIPE_OP_TEXT_METRICS, 2, "abcd", "40"); double w4big = atof(argOf(0));
    CHECK("MEASURE_TEXT at another font size", w4big > 2 * w4 - 0.5 && w4big < 2 * w4 + 0.5);

    /* GET_RANGE_GEOMETRY for a cursor */
    hipe_loc g = tag(0, "div", "", "", "abcd");
    ask(g, HIPE_OP_GET_RANGE_GEOMETRY, HIPE_OP_RANGE_GEOMETRY, 1, "2", "");
    int x, y, wd, ht; argOf(0);
    CHECK("GET_RANGE_GEOMETRY cursor: zero width, at two characters", sscanf(v, "%d,%d,%d,%d", &x, &y, &wd, &ht) == 4
          && wd == 0 && x >= (int)(2 * w1) - 1 && x <= (int)(2 * w1) + 1);

    /* GET_SRC */
    hipe_loc img = tag(0, "img", "", "", "");
    hipe_instruction set; hipe_instruction_init(&set);
    set.opcode = HIPE_OP_SET_SRC; set.location = img;
    set.arg[0] = (char*)png; set.arg_length[0] = PNG_SIZE;
    set.arg[1] = "image/png"; set.arg_length[1] = 9;
    hipe_send_instruction(s, set);
    ask(img, HIPE_OP_GET_SRC, HIPE_OP_SRC_RETURN, 0, "", "");
    snprintf(v, sizeof v, "%zu bytes, %.*s", (size_t)last.arg_length[0], (int)last.arg_length[1], last.arg[1]);
    CHECK("GET_SRC returns an img's bytes unchanged", last.arg_length[0] == PNG_SIZE && !memcmp(last.arg[0], png, PNG_SIZE)
          && !strcmp(v + strlen(v) - 9, "image/png"));
    hipe_loc canvas = tag(0, "canvas", "", "", "");
    ask(canvas, HIPE_OP_GET_SRC, HIPE_OP_SRC_RETURN, 1, "png", "");
    snprintf(v, sizeof v, "%zu bytes, %.*s", (size_t)last.arg_length[0], (int)last.arg_length[1], last.arg[1]);
    CHECK("GET_SRC of a canvas is a PNG", last.arg_length[0] > 8 && !memcmp(last.arg[0], "\x89PNG", 4));
    ask(list, HIPE_OP_GET_SRC, HIPE_OP_SRC_RETURN, 0, "", ""); argOf(2);
    CHECK("GET_SRC of a div is an error", last.arg_length[0] == 0 && strlen(v) > 0);

    /* scrolling */
    hipe_loc sc = tag(0, "div", "", "", "");
    hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, sc, 2, "style", "height: 100px; width: 200px; overflow: auto");
    hipe_loc tall = tag(sc, "div", "", "", "tall");
    hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, tall, 2, "style", "height: 1000px");
    hipe_send(s, HIPE_OP_SCROLL_TO, 0, sc, 2, "", "200");
    ask(sc, HIPE_OP_GET_SCROLL_GEOMETRY, HIPE_OP_GEOMETRY_RETURN, 0, "", ""); argOf(1);
    CHECK("SCROLL_TO", atoi(v) == 200);
    hipe_send(s, HIPE_OP_SCROLL_BY, 0, sc, 2, "", "50");
    ask(sc, HIPE_OP_GET_SCROLL_GEOMETRY, HIPE_OP_GEOMETRY_RETURN, 0, "", ""); argOf(1);
    CHECK("SCROLL_BY", atoi(v) == 250);
    argOf(3); CHECK("GET_SCROLL_GEOMETRY: scrollable height", atoi(v) == 1000);
    hipe_send(s, HIPE_OP_SCROLL_TO, 0, sc, 3, "", "100", "%");
    ask(sc, HIPE_OP_GET_SCROLL_GEOMETRY, HIPE_OP_GEOMETRY_RETURN, 0, "", ""); argOf(1);
    CHECK("SCROLL_TO 100% reaches the end", atoi(v) == 900);

    /* a click event */
    hipe_send(s, HIPE_OP_CLEAR, 0, 0, 0);
    hipe_loc btn = tag(0, "button", "", "", "click me");
    hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, btn, 2, "style", "width: 300px; height: 60px");
    hipe_send(s, HIPE_OP_EVENT_REQUEST, 77, btn, 1, "click");
    ask(btn, HIPE_OP_GET_GEOMETRY, HIPE_OP_GEOMETRY_RETURN, 0, "", "");
    char cmd[128];
    snprintf(cmd, sizeof cmd, "xdotool mousemove %d %d click 1", atoi(argOf(0)) + 150, atoi(argOf(1)) + 30);
    if(system(cmd) != 0) fprintf(stderr, "xdotool failed\n");
    ask(btn, HIPE_OP_GET_GEOMETRY, HIPE_OP_GEOMETRY_RETURN, 0, "", ""); /* any reply; the event arrives first */
    hipe_instruction ev; hipe_instruction_init(&ev);
    int got = 0;
    for(int i = 0; i < 20 && !got; i++) {
        if(hipe_next_instruction(s, &ev, 0) > 0) {
            if(ev.opcode == HIPE_OP_EVENT) got = 1;
            else hipe_instruction_clear(&ev);
        } else usleep(100000);
    }
    snprintf(v, sizeof v, "%.*s %.*s", got ? (int)ev.arg_length[0] : 0, got ? ev.arg[0] : "", got ? (int)ev.arg_length[1] : 0, got ? ev.arg[1] : "");
    CHECK("a click on the button arrives as a click event", got && ev.location == btn && ev.requestor == 77 && !strncmp(v, "click", 5));

    if(fails) printf("%d FAILED\n", fails);
    hipe_close_session(s);
    return fails ? 1 : 0;
}
