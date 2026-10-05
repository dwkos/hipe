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

/* Standard CSS names: properties (with their old -webkit- names still accepted), keywords, :is() and image-set().
 * Run by run.sh against a private hiped. Prints PASS/FAIL lines and exits non-zero if anything failed. */

#include <hipe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static hipe_session s;
static char v[4000];
static int fails = 0;
#define CHECK(name, cond) do { int ok_ = (cond); if(!ok_) fails++; \
    printf("%s %s  [%.100s]\n", ok_ ? "PASS" : "FAIL", name, v); fflush(stdout); } while(0)

static const char* get(hipe_loc l, char op, char ret, int nargs, const char* a, int idx) {
    hipe_instruction in; hipe_instruction_init(&in);
    hipe_send(s, op, 0, l, nargs, a);
    hipe_await_instruction(s, &in, ret);
    snprintf(v, sizeof v, "%.*s", (int)in.arg_length[idx], in.arg[idx]);
    hipe_instruction_clear(&in);
    return v;
}
#define computed(l, p) get(l, HIPE_OP_GET_STYLE, HIPE_OP_STYLE_RETURN, 1, p, 1)
static int width(hipe_loc l) { return atoi(get(l, HIPE_OP_GET_GEOMETRY, HIPE_OP_GEOMETRY_RETURN, 0, "", 2)); }

static hipe_loc box(const char* text, const char* style) {
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, 0, 4, "div", "", "", text);
    hipe_loc l = hipe_newest_location();
    hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, l, 2, "style", style);
    return l;
}

int main() {
    s = hipe_open_session(0, 0, 0, "css");
    if(!s) return 2;
    hipe_loc a;

    a = box("x", "user-select: none");
    computed(a, "user-select");          CHECK("user-select", !strcmp(v, "none"));
    computed(a, "-webkit-user-select");  CHECK("the old name reads the same property", !strcmp(v, "none"));
    a = box("x", "-webkit-user-select: none");
    computed(a, "user-select");          CHECK("the old name still sets it", !strcmp(v, "none"));
    a = box("x", "margin-inline-start: 13px");
    computed(a, "margin-left");          CHECK("margin-inline-start", !strcmp(v, "13px"));
    a = box("x", "inline-size: 123px");
    width(a);                            CHECK("inline-size", width(a) == 123);
    a = box("x", "hyphens: none; text-decoration-line: underline");
    computed(a, "hyphens");              CHECK("hyphens", !strcmp(v, "none"));
    computed(a, "text-decoration-line"); CHECK("text-decoration-line", !strcmp(v, "underline"));

    a = box("aaaa bbbbbbbbbb cc", "width: min-content; font: 20px monospace");
    width(a);                            CHECK("width: min-content", width(a) > 0 && width(a) < 200);
    int minw = width(a);
    a = box("aaaa bbbbbbbbbb cc", "width: max-content; font: 20px monospace");
    width(a);                            CHECK("width: max-content", width(a) > minw && width(a) < 400);
    a = box("x", "position: sticky");
    computed(a, "position");             CHECK("position: sticky", !strcmp(v, "sticky"));
    a = box("x", "cursor: grab");
    computed(a, "cursor");               CHECK("cursor: grab", !strcmp(v, "grab"));

    hipe_send(s, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2, ":is(#is1, #is2)", "color: rgb(1, 2, 3)");
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, 0, 4, "p", "is2", "", "is");
    a = hipe_newest_location();
    computed(a, "color");                CHECK(":is()", !strcmp(v, "rgb(1, 2, 3)"));

    a = box("x", "background-image: image-set(url(\"data:image/png;base64,iVBORw0KGgo=\") 1x)");
    computed(a, "background-image");     CHECK("image-set()", !strncmp(v, "image-set(", 10));

    if(fails) printf("%d FAILED\n", fails);
    hipe_close_session(s);
    return fails ? 1 : 0;
}
