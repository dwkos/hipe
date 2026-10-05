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

/* Paragraph break (U+2029) tests: the break, -hipe-paragraph-spacing and -hipe-paragraph-indent, read-back and caret.
 * Run by run.sh against a private hiped. Prints PASS/FAIL lines and exits non-zero if anything failed. */

#include <hipe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PS "\xE2\x80\xA9" /* U+2029 PARAGRAPH SEPARATOR in UTF-8 */

static hipe_session s;
static char v[4000];
static int fails = 0;
#define CHECK(name, cond) do { int ok_ = (cond); if(!ok_) fails++; \
    printf("%s %s  [%.100s]\n", ok_ ? "PASS" : "FAIL", name, v); fflush(stdout); } while(0)

static const char* get(hipe_loc l, char op, char ret, int nargs, const char* a, const char* b, int idx) {
    hipe_instruction in; hipe_instruction_init(&in);
    hipe_send(s, op, 0, l, nargs, a, b);
    hipe_await_instruction(s, &in, ret);
    snprintf(v, sizeof v, "%.*s", (int)in.arg_length[idx], in.arg[idx]);
    hipe_instruction_clear(&in);
    return v;
}
#define content(l, m) get(l, HIPE_OP_GET_CONTENT, HIPE_OP_CONTENT_RETURN, 1, m, "", 0)
#define computed(l, p) get(l, HIPE_OP_GET_STYLE, HIPE_OP_STYLE_RETURN, 1, p, "", 1)

static int height(hipe_loc l) {
    return atoi(get(l, HIPE_OP_GET_GEOMETRY, HIPE_OP_GEOMETRY_RETURN, 0, "", "", 3));
}

/* x of the first rectangle covering characters [from, to) of l's laid-out text */
static int textX(hipe_loc l, const char* from, const char* to) {
    return atoi(get(l, HIPE_OP_GET_RANGE_GEOMETRY, HIPE_OP_RANGE_GEOMETRY, 2, from, to, 0));
}

/* a div with the given text (default text mode) and inline style */
static hipe_loc box(const char* text, const char* style) {
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, 0, 4, "div", "", "t", text);
    hipe_loc l = hipe_newest_location();
    if(style[0]) hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, l, 2, "style", style);
    return l;
}

int main() {
    s = hipe_open_session(0, 0, 0, "text");
    if(!s) return 2;
    /* 30px lines, so the default spacing (half a line) is 15px */
    hipe_send(s, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, ".t", "font: 20px sans-serif; line-height: 30px; width: 400px; margin: 0; padding: 0; border: 0");

    hipe_loc a;
    a = box("ab", "");                            height(a); CHECK("text without U+2029: one line", height(a) == 30);
    a = box("a" PS "b", "");                      height(a); CHECK("U+2029 breaks in white-space normal, with a half-line gap", height(a) == 75);
    a = box("a" PS "b", "white-space: pre-wrap"); height(a); CHECK("U+2029 breaks in pre-wrap, with a half-line gap", height(a) == 75);
    a = box("a" PS "b", "white-space: nowrap");   height(a); CHECK("U+2029 breaks in nowrap", height(a) == 75);
    a = box("a\nb", "");                          height(a); CHECK("\\n in white-space normal still doesn't break", height(a) == 30);
    a = box("a\nb", "white-space: pre-wrap");     height(a); CHECK("\\n in pre-wrap: a break, no gap", height(a) == 60);
    a = box("a" PS PS "b", "");                   height(a); CHECK("two U+2029: an empty line, a gap after each", height(a) == 120);
    a = box(PS "b", "");                          height(a); CHECK("leading U+2029: an empty first line, then a gap", height(a) == 75);
    a = box("a" PS, "");                          height(a); CHECK("trailing U+2029: no empty line, no gap", height(a) == 30);
    a = box("a " PS " b", "");                    height(a); CHECK("spaces around U+2029 collapse", height(a) == 75 && textX(a, "-2", "-1") == 0);

    a = box("a" PS "b", "-hipe-paragraph-spacing: 4px"); height(a); CHECK("spacing as a length", height(a) == 64);
    a = box("a" PS "b", "-hipe-paragraph-spacing: 1");   height(a); CHECK("spacing as a number of lines", height(a) == 90);
    a = box("a" PS "b", "-hipe-paragraph-spacing: 0");   height(a); CHECK("no spacing", height(a) == 60);
    computed(a, "-hipe-paragraph-spacing"); CHECK("GET_STYLE spacing 0", !strcmp(v, "0"));
    a = box("a", "");
    computed(a, "-hipe-paragraph-spacing"); CHECK("GET_STYLE default spacing", !strcmp(v, "0.5"));
    computed(a, "-hipe-paragraph-indent");  CHECK("GET_STYLE default indent", !strcmp(v, "0px"));

    a = box("a" PS "b", "-hipe-paragraph-indent: 2em; -hipe-paragraph-spacing: 0");
    textX(a, "2", "3"); CHECK("the line after U+2029 is indented", textX(a, "2", "3") == 40);
    textX(a, "0", "1"); CHECK("the first line is not", textX(a, "0", "1") == 0);
    computed(a, "-hipe-paragraph-indent"); CHECK("GET_STYLE indent", !strcmp(v, "40px"));
    a = box("a\nb", "white-space: pre-wrap; -hipe-paragraph-indent: 2em");
    textX(a, "2", "3"); CHECK("the line after \\n is not indented", textX(a, "2", "3") == 0);
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, 0, 4, "div", "", "", "");
    hipe_loc outer = hipe_newest_location();
    hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, outer, 2, "style", "width: 400px");
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, outer, 4, "div", "", "t", "a" PS "b");
    a = hipe_newest_location();
    hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, a, 2, "style", "-hipe-paragraph-indent: 10%");
    textX(a, "2", "3"); CHECK("indent as a percentage of the containing block's width", textX(a, "2", "3") == 40);
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, 0, 4, "div", "", "", "");
    outer = hipe_newest_location();
    hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, outer, 2, "style", "-hipe-paragraph-spacing: 4px");
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, outer, 4, "div", "", "t", "a" PS "b");
    a = hipe_newest_location(); height(a); CHECK("the properties are inherited", height(a) == 64);

    a = box("a" PS "b\nc", "white-space: pre-wrap");
    content(a, "0"); CHECK("GET_CONTENT mode 0 returns U+2029 as sent", !strcmp(v, "a" PS "b\nc"));
    content(a, "2"); CHECK("GET_CONTENT mode 2 returns U+2029 as sent", !strcmp(v, "a" PS "b\nc"));
    hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, a, 2, "contenteditable", "true");
    hipe_send(s, HIPE_OP_CARAT_POSITION, 0, a, 2, "2", "2");
    get(a, HIPE_OP_GET_CARAT_POSITION, HIPE_OP_CARAT_POSITION, 0, "", "", 0);
    CHECK("a caret offset after U+2029 round-trips", !strcmp(v, "2"));

    if(fails) printf("%d FAILED\n", fails);
    hipe_close_session(s);
    return fails ? 1 : 0;
}
