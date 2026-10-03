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

/* Markup text mode (SET_TEXT / APPEND_TEXT mode 3) tests. Run by run.sh against a private hiped; the form test clicks
 * with xdotool on $DISPLAY. Prints PASS/FAIL lines and exits non-zero if anything failed. */

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

static const char* get(hipe_loc l, char op, char ret, const char* a, int idx) {
    hipe_instruction in; hipe_instruction_init(&in);
    hipe_send(s, op, 0, l, 1, a);
    hipe_await_instruction(s, &in, ret);
    snprintf(v, sizeof v, "%.*s", (int)in.arg_length[idx], in.arg[idx]);
    hipe_instruction_clear(&in);
    return v;
}
#define content(l, m) get(l, HIPE_OP_GET_CONTENT, HIPE_OP_CONTENT_RETURN, m, 0)

static hipe_loc tag(hipe_loc parent, const char* t, const char* id, const char* cls, const char* text) {
    hipe_send(s, HIPE_OP_APPEND_TAG, 0, parent, 4, t, id, cls, text);
    return hipe_newest_location();
}

int main() {
    s = hipe_open_session(0, 0, 0, "markup");
    if(!s) return 2;
    hipe_loc formBox = tag(0, "div", "fb", "", "");
    hipe_loc a = tag(0, "div", "A", "", "");
    hipe_loc b = tag(0, "div", "B", "", "B-content");
    hipe_send(s, HIPE_OP_SET_TEXT, 0, a, 2, "<span class=\"kw\">if</span> <b>x</b> &lt;y&gt; &amp;", "3");
    content(a, "0"); CHECK("styled spans, entities decoded", !strcmp(v, "if x <y> &"));
    content(a, "1"); CHECK("markup kept as elements", strstr(v, "<span class=\"kw\">if</span>") && strstr(v, "<b>x</b>"));
    hipe_send(s, HIPE_OP_APPEND_TEXT, 0, a, 2, "</div><p id=out>ESCAPED</p><div>", "3");
    content(a, "1"); CHECK("a closing tag can't end the target element", strstr(v, "ESCAPED") != NULL);
    content(b, "0"); CHECK("the sibling is untouched", !strcmp(v, "B-content"));
    hipe_send(s, HIPE_OP_APPEND_TEXT, 0, a, 2, "<b>unclosed", "3");
    hipe_send(s, HIPE_OP_APPEND_TEXT, 0, a, 2, " after", "0");
    content(a, "1"); CHECK("an unclosed tag is closed within its markup", strstr(v, "<b>unclosed</b> after") != NULL);
    hipe_send(s, HIPE_OP_APPEND_TEXT, 0, b, 2, "<style>.kw2 { color: rgb(1, 2, 3) }</style>", "3");
    hipe_loc k = tag(0, "p", "", "kw2", "styled");
    get(k, HIPE_OP_GET_STYLE, HIPE_OP_STYLE_RETURN, "color", 1); CHECK("a <style> in markup applies", !strcmp(v, "rgb(1, 2, 3)"));
    hipe_send(s, HIPE_OP_APPEND_TEXT, 0, b, 2, "<script>document.getElementById('B').textContent='HACKED'</script>", "3");
    content(b, "1"); CHECK("a <script> in markup never runs", strstr(v, "B-content") != NULL);

    /* a form submitted by a real click doesn't navigate */
    hipe_send(s, HIPE_OP_SET_TEXT, 0, formBox, 2,
        "<form action=\"data:text/html,NAVIGATED\"><button type=submit style=\"width:300px;height:60px\">go</button></form>"
        "<iframe></iframe><input autofocus>", "3");
    hipe_instruction in; hipe_instruction_init(&in);
    hipe_send(s, HIPE_OP_GET_GEOMETRY, 0, formBox, 0);
    hipe_await_instruction(s, &in, HIPE_OP_GEOMETRY_RETURN);
    char cmd[128];
    snprintf(cmd, sizeof cmd, "xdotool mousemove %d %d click 1", atoi(in.arg[0]) + 150, atoi(in.arg[1]) + 30);
    hipe_instruction_clear(&in);
    if(system(cmd) != 0) fprintf(stderr, "xdotool failed\n");
    sleep(2);
    content(0, "0"); CHECK("a form click doesn't navigate", strstr(v, "B-content") && !strstr(v, "NAVIGATED"));

    hipe_loc last = tag(0, "p", "", "", "LAST");
    content(last, "0"); CHECK("numbering in step after markup", !strcmp(v, "LAST"));
    hipe_send(s, HIPE_OP_SET_TEXT, 0, 0, 2, "<h1>Body <i>markup</i></h1>", "3");
    content(0, "0"); CHECK("mode 3 on the body (location 0)", !strcmp(v, "Body markup"));
    hipe_loc last2 = tag(0, "p", "", "", "LAST2");
    content(last2, "0"); CHECK("numbering in step after body markup", !strcmp(v, "LAST2"));
    hipe_send(s, HIPE_OP_SET_TEXT, 0, last2, 2, "<b>not markup</b>", "0");
    content(last2, "0"); CHECK("mode 0 still shows tags as text", !strcmp(v, "<b>not markup</b>"));

    printf("DONE fails=%d\n", fails);
    hipe_close_session(s);
    return fails != 0;
}
