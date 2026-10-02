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

/* CSS Demo - demonstrates hipe's CSS styling API: defining rules with
   HIPE_OP_ADD_STYLE_RULE (including class and compound-class selectors),
   and toggling classes live with HIPE_OP_TOGGLE_CLASS - the correct way
   to change an element's class after it's already on screen. (Note:
   HIPE_OP_SET_ATTRIBUTE cannot be used to set "class" - it isn't on
   hiped's attribute whitelist. HIPE_OP_TOGGLE_CLASS is the real API for
   this and works both before and after the target element is visible.) */

#include <hipe.h>
#include <stdlib.h>
#include <stdio.h>

hipe_session session;

hipe_loc getById(char* id) {
    hipe_send(session, HIPE_OP_GET_BY_ID, 0, 0, 1, id);
    hipe_instruction instruction;
    hipe_instruction_init(&instruction);
    hipe_await_instruction(session, &instruction, HIPE_OP_LOCATION_RETURN);
    return instruction.location;
}

hipe_loc appendTag(hipe_loc parent, const char* tag, const char* id) {
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, parent, 2, tag, id);
    hipe_send(session, HIPE_OP_GET_LAST_CHILD, 0, parent, 0);
    hipe_instruction instruction;
    hipe_instruction_init(&instruction);
    hipe_await_instruction(session, &instruction, HIPE_OP_LOCATION_RETURN);
    return instruction.location;
}

int main(int argc, char** argv)
{
    session = hipe_open_session(argc > 1 ? argv[1] : 0, 0, 0, "CSS Demo");
    if (!session) exit(1);

    /* Base rules. Note the three ".box.<state>" rules below are compound
       class selectors - each one only applies once BOTH classes are
       present on the element at once, same as quadrant's ".frame.maximised". */
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2,
        "body", "background-color:#202020; color:white; font-family:arial,sans-serif; text-align:center;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2,
        "h1", "font-weight:normal; font-size:120%;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2,
        ".box", "margin:40px auto; width:200px; height:100px; line-height:100px; color:white; "
                "background-color:#404040; border:2px solid #606060; transition:all 0.2s;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2,
        ".box.highlighted", "background-color:#a03030; border-color:#ff6060;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2,
        ".box.rounded", "border-radius:20px;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2,
        ".box.shadow", "box-shadow:0 0 25px 5px #40c0ff;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2,
        ".btn", "display:inline-block; margin:8px; background-color:#404040; "
                "border:2px solid #606060; border-radius:4px; color:white;");
    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0, 0, 2,
        ".btn.active", "background-color:#306030; border-color:#60ff60;");

    /* Window contents. */
    hipe_loc heading = appendTag(0, "h1", "heading");
    hipe_send(session, HIPE_OP_SET_TEXT, 0, heading, 1, "CSS Demo");

    hipe_loc box = appendTag(0, "div", "box");
    hipe_send(session, HIPE_OP_TOGGLE_CLASS, 0, box, 1, "box");
    hipe_send(session, HIPE_OP_SET_TEXT, 0, box, 1, "sample box");

    hipe_loc btnHighlight = appendTag(0, "button", "btnHighlight");
    hipe_send(session, HIPE_OP_TOGGLE_CLASS, 0, btnHighlight, 1, "btn");
    hipe_send(session, HIPE_OP_SET_TEXT, 0, btnHighlight, 1, "Highlight");
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 1, btnHighlight, 1, "click");

    hipe_loc btnRounded = appendTag(0, "button", "btnRounded");
    hipe_send(session, HIPE_OP_TOGGLE_CLASS, 0, btnRounded, 1, "btn");
    hipe_send(session, HIPE_OP_SET_TEXT, 0, btnRounded, 1, "Round Corners");
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 2, btnRounded, 1, "click");

    hipe_loc btnShadow = appendTag(0, "button", "btnShadow");
    hipe_send(session, HIPE_OP_TOGGLE_CLASS, 0, btnShadow, 1, "btn");
    hipe_send(session, HIPE_OP_SET_TEXT, 0, btnShadow, 1, "Glow");
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 3, btnShadow, 1, "click");

    /* Main event loop: each button click toggles a class on the box AND
       a matching "active" class on the button itself, both live, both
       added well after the page's body already exists - exactly the
       scenario that used to be unsupported in older WebKit versions. */
    hipe_instruction hi;
    hipe_instruction_init(&hi);
    while (1) {
        hipe_next_instruction(session, &hi, 1);

        if (hi.opcode == HIPE_OP_SERVER_DENIED)
            return 0;

        if (hi.opcode == HIPE_OP_EVENT) {
            switch (hi.requestor) {
            case 1:
                hipe_send(session, HIPE_OP_TOGGLE_CLASS, 0, box, 1, "highlighted");
                hipe_send(session, HIPE_OP_TOGGLE_CLASS, 0, btnHighlight, 1, "active");
                break;
            case 2:
                hipe_send(session, HIPE_OP_TOGGLE_CLASS, 0, box, 1, "rounded");
                hipe_send(session, HIPE_OP_TOGGLE_CLASS, 0, btnRounded, 1, "active");
                break;
            case 3:
                hipe_send(session, HIPE_OP_TOGGLE_CLASS, 0, box, 1, "shadow");
                hipe_send(session, HIPE_OP_TOGGLE_CLASS, 0, btnShadow, 1, "active");
                break;
            }
        } else if (hi.opcode == HIPE_OP_FRAME_CLOSE) {
            return 0;
        }
    }
    return 0;
}
