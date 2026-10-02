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

/* cbdemo - interactive checkbox / radio-button rendering playground.
   Native <input type=checkbox|radio> toggle themselves on click, so this
   just builds the DOM and sits in its event loop.  Compile:
       gcc cbdemo.c -o cbdemo -lhipe
   Run (once periscope is up):
       HIPE_KEYFILE=/tmp/periscope.key ./cbdemo                         */

#include <hipe.h>
#include <stdlib.h>
#include <stdio.h>

hipe_session s;

hipe_loc last(hipe_loc p) {
    hipe_send(s, HIPE_OP_GET_LAST_CHILD, 0, p, 0);
    hipe_instruction i; hipe_instruction_init(&i);
    hipe_await_instruction(s, &i, HIPE_OP_LOCATION_RETURN);
    return i.location;
}
hipe_loc add(hipe_loc p, const char* tag) {
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, p, 2, tag, "");
    return last(p);
}
void txt(hipe_loc l, const char* t) { hipe_send(s, HIPE_OP_SET_TEXT, 0, l, 2, t, ""); }
void att(hipe_loc l, const char* k, const char* v) { hipe_send(s, HIPE_OP_SET_ATTRIBUTE, 0, l, 2, k, v); }
void sty(hipe_loc l, const char* k, const char* v) { hipe_send(s, HIPE_OP_SET_STYLE, 0, l, 2, k, v); }

/* one labelled control */
hipe_loc ctl(hipe_loc parent, const char* type, const char* name, const char* label) {
    hipe_loc lab = add(parent, "label");
    sty(lab, "margin-right", "20px");
    sty(lab, "display", "inline-flex");
    sty(lab, "align-items", "center");
    hipe_loc in = add(lab, "input");
    att(in, "type", type);
    if (name) att(in, "name", name);
    sty(in, "margin-right", "7px");
    hipe_loc sp = add(lab, "span");
    txt(sp, label);
    return in;
}

hipe_loc section(hipe_loc parent, const char* heading) {
    hipe_loc d = add(parent, "div");
    sty(d, "margin", "0 0 6px 0");
    sty(d, "padding", "14px 18px");
    hipe_loc h = add(d, "div");
    txt(h, heading);
    sty(h, "font-weight", "bold");
    sty(h, "margin-bottom", "10px");
    sty(h, "opacity", "0.7");
    sty(h, "font-size", "13px");
    sty(h, "text-transform", "uppercase");
    sty(h, "letter-spacing", "1px");
    return d;
}
hipe_loc line(hipe_loc parent) {
    hipe_loc d = add(parent, "div");
    sty(d, "margin", "9px 0");
    return d;
}

int main(int argc, char** argv) {
    s = hipe_open_session(argc > 1 ? argv[1] : 0, 0, 0, "Checkbox / radio playground");
    if (!s) { fprintf(stderr, "no session\n"); return 1; }

    sty(0, "font", "16px sans-serif");
    sty(0, "padding", "8px");

    hipe_loc r, sec;

    /* --- default theme colours (dark primal / whatever the theme sets) --- */
    sec = section(0, "theme default");
    r = line(sec);
    ctl(r, "checkbox", 0, "unchecked");
    ctl(r, "checkbox", 0, "checked");
    { hipe_loc e = last(r); /* label */ }
    r = line(sec);
    ctl(r, "radio", "a", "option one");
    ctl(r, "radio", "a", "option two");
    ctl(r, "radio", "a", "option three");
    r = line(sec);
    { hipe_loc e = ctl(r, "checkbox", 0, "disabled"); att(e, "disabled", "disabled"); }
    { hipe_loc e = ctl(r, "radio", "b", "disabled radio"); att(e, "disabled", "disabled"); }

    /* --- on a light panel: strokes follow the (now dark) text colour --- */
    sec = section(0, "light panel  (color follows CSS)");
    sty(sec, "background", "#f4f2ee");
    sty(sec, "color", "#222");
    sty(sec, "border-radius", "6px");
    r = line(sec);
    ctl(r, "checkbox", 0, "on white");
    ctl(r, "checkbox", 0, "and checked");
    r = line(sec);
    ctl(r, "radio", "c", "pick a");
    ctl(r, "radio", "c", "pick b");

    /* --- explicit CSS color on the controls --- */
    sec = section(0, "css  color:  per row");
    r = line(sec); sty(r, "color", "#e0546c");
    ctl(r, "checkbox", 0, "crimson");
    ctl(r, "radio", "d", "crimson radio");
    r = line(sec); sty(r, "color", "#3bb273");
    ctl(r, "checkbox", 0, "green");
    ctl(r, "radio", "e", "green radio");
    r = line(sec); sty(r, "color", "#4a9de0");
    ctl(r, "checkbox", 0, "blue");
    ctl(r, "radio", "f", "blue radio");

    /* --- explicit CSS background-color fills the box --- */
    sec = section(0, "css  background-color  on the box");
    r = line(sec);
    { hipe_loc e = ctl(r, "checkbox", 0, "filled");      sty(e, "background", "#503060"); }
    { hipe_loc e = ctl(r, "checkbox", 0, "filled+check"); sty(e, "background", "#503060"); att(e, "checked", "checked"); }
    { hipe_loc e = ctl(r, "radio", "g", "filled radio");  sty(e, "background", "#295a7a"); }

    /* event loop - native controls toggle themselves */
    hipe_instruction hi; hipe_instruction_init(&hi);
    while (1) {
        hipe_next_instruction(s, &hi, 1);
        if (hi.opcode == HIPE_OP_SERVER_DENIED) return 0;
        if (hi.opcode == HIPE_OP_FRAME_CLOSE) return 0;
    }
}
