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

/* Testbed for HIPE_OP_ADD_FONT: loads a font file from disk and applies it dynamically,
 * via C++ calls on the hipecore side rather than any JS. Shows the same sample text
 * rendered in the default font next to the newly-loaded font, so the effect is obvious.
 *
 * Usage: hipe-addfont <font-file> [family-name] [host-key]
 */

#include <hipe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

hipe_session session;

hipe_loc getLoc(char* id) {
    hipe_send(session, HIPE_OP_GET_BY_ID, 0, 0, 1, id);
    hipe_instruction instruction;
    hipe_instruction_init(&instruction);
    hipe_await_instruction(session, &instruction, HIPE_OP_LOCATION_RETURN);
    return instruction.location;
}

/* Crude extension-based MIME type guess -- good enough for a test client. */
const char* mimeTypeForFile(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot)
        return "application/octet-stream";
    if (strcasecmp(dot, ".ttf") == 0)
        return "font/ttf";
    if (strcasecmp(dot, ".otf") == 0)
        return "font/otf";
    if (strcasecmp(dot, ".woff") == 0)
        return "font/woff";
    if (strcasecmp(dot, ".woff2") == 0)
        return "font/woff2";
    return "application/octet-stream";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: %s <font-file> [family-name] [host-key]\n", argv[0]);
        printf("e.g.:  %s /usr/share/fonts/truetype/dejavu/DejaVuSerif-BoldItalic.ttf\n", argv[0]);
        return 1;
    }

    const char* fontPath = argv[1];
    const char* familyName = (argc > 2) ? argv[2] : "HipeAddFontTest";
    const char* hostKey = (argc > 3) ? argv[3] : 0;
    const char* mimeType = mimeTypeForFile(fontPath);

    FILE* file = fopen(fontPath, "rb");
    if (!file) {
        printf("Could not open font file: '%s' for reading.\n", fontPath);
        return 2;
    }

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    rewind(file);

    char* data = malloc(size);
    size_t result = fread(data, 1, size, file);
    fclose(file);
    if (result != size) {
        printf("Error reading font file (expected %zu bytes, got %zu).\n", size, result);
        return 2;
    }

    printf("Loaded '%s': %zu bytes, mime type '%s', family name '%s'.\n",
        fontPath, size, mimeType, familyName);

    session = hipe_open_session(hostKey, 0, 0, "Font Loading Test");
    if (!session) {
        printf("Could not connect to hipe.\n");
        return 3;
    }

    hipe_send(session, HIPE_OP_SET_STYLE, 0, 0, 2, "background-color", "white");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, 0, 2, "color", "black");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, 0, 2, "padding", "1em");

    /* HIPE_OP_ADD_FONT carries binary data (arg[2]), so this goes via
     * hipe_send_instruction() directly rather than the strlen()-based hipe_send()
     * convenience wrapper -- font files can contain embedded null bytes. */
    hipe_instruction addFont;
    hipe_instruction_init(&addFont);
    addFont.opcode = HIPE_OP_ADD_FONT;
    addFont.location = 0;
    addFont.arg[0] = (char*)familyName; addFont.arg_length[0] = strlen(familyName);
    addFont.arg[1] = (char*)mimeType;   addFont.arg_length[1] = strlen(mimeType);
    addFont.arg[2] = data;              addFont.arg_length[2] = size;
    hipe_send_instruction(session, addFont);
    free(data);

    /* Heading with the diagnostic info, in the default font. */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 2, "div", "heading");
    hipe_loc heading = getLoc("heading");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, heading, 2, "font-size", "80%");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, heading, 2, "margin-bottom", "1em");
    char headingText[512];
    snprintf(headingText, sizeof(headingText), "Loaded \"%s\" (%zu bytes, %s) as family \"%s\"",
        fontPath, size, mimeType, familyName);
    hipe_send(session, HIPE_OP_SET_TEXT, 0, heading, 1, headingText);

    /* Default-font sample. */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 1, "div");
    hipe_send(session, HIPE_OP_GET_LAST_CHILD, 0, 0, 0);
    hipe_instruction instr;
    hipe_instruction_init(&instr);
    hipe_await_instruction(session, &instr, HIPE_OP_LOCATION_RETURN);
    hipe_loc defaultBlock = instr.location;
    hipe_send(session, HIPE_OP_SET_STYLE, 0, defaultBlock, 2, "font-size", "250%");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, defaultBlock, 2, "margin-bottom", "0.5em");
    hipe_send(session, HIPE_OP_SET_TEXT, 0, defaultBlock, 1, "The quick brown fox (default font)");

    /* Custom-font sample, same text, styled with the newly-registered family. */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 1, "div");
    hipe_send(session, HIPE_OP_GET_LAST_CHILD, 0, 0, 0);
    hipe_instruction_clear(&instr);
    hipe_instruction_init(&instr);
    hipe_await_instruction(session, &instr, HIPE_OP_LOCATION_RETURN);
    hipe_loc customBlock = instr.location;
    hipe_send(session, HIPE_OP_SET_STYLE, 0, customBlock, 2, "font-size", "250%");
    hipe_send(session, HIPE_OP_SET_STYLE, 0, customBlock, 2, "font-family", familyName);
    char customText[128];
    snprintf(customText, sizeof(customText), "The quick brown fox (\"%s\")", familyName);
    hipe_send(session, HIPE_OP_SET_TEXT, 0, customBlock, 1, customText);

    printf("Displaying comparison. Close the window (or Ctrl+C here) to quit.\n");

    hipe_instruction event;
    hipe_instruction_init(&event);
    do {
        hipe_next_instruction(session, &event, 1);
    } while (event.opcode != HIPE_OP_FRAME_CLOSE);

    return 0;
}
