/*  Copyright (c) 2025-2026 Daniel Kos, General Development Systems

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



#include <hipe.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

hipe_session session;

hipe_loc getLoc(char* id) {
    hipe_send(session, HIPE_OP_GET_BY_ID, 0, 0, 1, id); 
    hipe_instruction instruction;
    hipe_instruction_init(&instruction);
    hipe_await_instruction(session, &instruction, HIPE_OP_LOCATION_RETURN);
    return instruction.location;
}

int main(int argc, char** argv) {

    /*Request new application frame from Hipe display server.*/
    session = hipe_open_session(argc>1 ? argv[1] : 0,0,0,"wysiwyg");
    if(!session) exit(1);

    hipe_send(session, HIPE_OP_ADD_STYLE_RULE, 0,0, 2, "#document:focus", "outline:0");

    hipe_send(session, HIPE_OP_SET_STYLE, 0,0, 2, "margin", "0");
    hipe_send(session, HIPE_OP_SET_STYLE, 0,0, 2, "background-color", "rgb(223,222,221)");
    //hipe_send(session, HIPE_OP_SET_STYLE, 0,0, 2, "text-align", "center");
    hipe_send(session, HIPE_OP_SET_STYLE, 0,0, 2, "color", "black");
    hipe_send(session, HIPE_OP_APPEND_TAG, 0,0, 2, "div", "document");
    hipe_loc doc = getLoc("document");
    hipe_send(session, HIPE_OP_SET_STYLE, 0,doc, 2, "background-color", "white");
    hipe_send(session, HIPE_OP_SET_STYLE, 0,doc, 2, "border", "1px solid");
    hipe_send(session, HIPE_OP_SET_STYLE, 0,doc, 2, "padding", "1em");
    hipe_send(session, HIPE_OP_SET_STYLE, 0,doc, 2, "box-shadow", "5px 5px 3px grey");
    hipe_send(session, HIPE_OP_SET_STYLE, 0,doc, 2, "height", "100%");
    hipe_send(session, HIPE_OP_SET_STYLE, 0,doc, 2, "width", "75%");
    hipe_send(session, HIPE_OP_SET_STYLE, 0,doc, 2, "margin-top", "1em");
    hipe_send(session, HIPE_OP_SET_STYLE, 0,doc, 2, "margin-left", "auto");
    hipe_send(session, HIPE_OP_SET_STYLE, 0,doc, 2, "margin-right", "auto");
    hipe_send(session, HIPE_OP_SET_ATTRIBUTE, 0,doc, 2, "contenteditable", "true");
    hipe_send(session, HIPE_OP_SET_TEXT, 0,doc, 2, 
        "Edit this!"
    );

    /*Event loop*/
    hipe_instruction hi;
    hipe_instruction_init(&hi);
    while(1) {
        hipe_next_instruction(session, &hi, 1);
        if(hi.opcode == HIPE_OP_EVENT) {

        } else if(hi.opcode == HIPE_OP_SERVER_DENIED) { //client orphaned by server
            return 0;
        } else if(hi.opcode == HIPE_OP_FRAME_CLOSE) { //close button clicked.
            return 0;
        }
    }
}

