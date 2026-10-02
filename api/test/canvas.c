/*  Copyright (c) 2016-2026 Daniel Kos, General Development Systems

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


/* hipe-canvas: a small MS-Paint-style drawing demo, showing off hipecore's Canvas 2D
 * dispatch -- multiple tools (freehand, line, rectangle, filled rectangle, ellipse,
 * filled ellipse, eraser), a vertical tool palette on the left, a 16-colour
 * foreground/background palette along the top (classic Windows VGA colours, left-click
 * sets foreground, right-click sets background), a clear button, mouse resizing, and a
 * live rubber-band preview while dragging out a shape, all driven through plain
 * HIPE_OP_CANVAS_ACTION/HIPE_OP_CANVAS_SET_PROPERTY calls. */

#include <hipe.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

hipe_session session;

#define CANVAS_MIN 20
static int canvasWidth = 640;
static int canvasHeight = 420;

/* Builds a "0,0,w,h" clearRect argument string for the *current* canvas size --
 * needed as a function rather than a fixed constant now that the canvas is
 * resizable. buf must be at least 32 bytes. */
static const char* clearAllArgs(char* buf) {
    snprintf(buf, 32, "0,0,%d,%d", canvasWidth, canvasHeight);
    return buf;
}

hipe_loc getLoc(const char* id) {
    hipe_send(session, HIPE_OP_GET_BY_ID, 0, 0, 1, id);
    hipe_instruction instruction;
    hipe_instruction_init(&instruction);
    hipe_await_instruction(session, &instruction, HIPE_OP_LOCATION_RETURN);
    return instruction.location;
}

void setStyle(hipe_loc loc, const char* prop, const char* value) {
    hipe_send(session, HIPE_OP_SET_STYLE, 0, loc, 2, prop, value);
}

void doAction(const char* method, const char* args) {
    hipe_send(session, HIPE_OP_CANVAS_ACTION, 0, 0, 2, method, args ? args : "");
}

void setProperty(const char* prop, const char* value) {
    hipe_send(session, HIPE_OP_CANVAS_SET_PROPERTY, 0, 0, 2, prop, value);
}

/* Every canvas element has its own independent CanvasRenderingContext2D -- switching
 * which one HIPE_OP_CANVAS_ACTION/HIPE_OP_CANVAS_SET_PROPERTY apply to (content vs.
 * overlay, see below) means re-selecting it, same as any other HIPE_OP_USE_CANVAS. */
void useCanvas(hipe_loc loc) {
    hipe_send(session, HIPE_OP_USE_CANVAS, 0, loc, 1, "2d");
}

/* Appends a styled, clickable <div> to parent, with a text label -- used for both
 * tool buttons and the clear button. Returns its location and registers "click". */
hipe_loc makeButton(hipe_loc parent, const char* id, const char* label) {
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, parent, 2, "div", id);
    hipe_loc loc = getLoc(id);
    hipe_send(session, HIPE_OP_SET_TEXT, 0, loc, 1, label);
    setStyle(loc, "display", "inline-block");
    setStyle(loc, "padding", "4px 8px");
    setStyle(loc, "margin", "2px");
    setStyle(loc, "border", "2px outset #ccc");
    setStyle(loc, "background-color", "#e8e8e8");
    setStyle(loc, "font-family", "sans-serif");
    setStyle(loc, "font-size", "12px");
    setStyle(loc, "cursor", "pointer");
    setStyle(loc, "-webkit-user-select", "none"); /* this WebKit fork has no unprefixed "user-select" */
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 0, loc, 1, "click");
    return loc;
}

/* Appends a small coloured swatch <div> to parent. Registers "mousedown" rather than
 * "click" so the handler can see which button was pressed (the "which" field of the
 * mouse event detail) -- left sets the foreground colour, right sets the background
 * colour, and plain "click" doesn't reliably distinguish buttons in this old WebKit. */
hipe_loc makeSwatch(hipe_loc parent, const char* id, const char* color) {
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, parent, 2, "div", id);
    hipe_loc loc = getLoc(id);
    setStyle(loc, "display", "inline-block");
    setStyle(loc, "width", "20px");
    setStyle(loc, "height", "20px");
    setStyle(loc, "margin", "1px");
    setStyle(loc, "border", "2px inset #888");
    setStyle(loc, "background-color", color);
    setStyle(loc, "cursor", "pointer");
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 0, loc, 1, "mousedown");
    return loc;
}

/* Tools, in toolbar order. */
enum {
    TOOL_PENCIL, TOOL_LINE, TOOL_RECT, TOOL_FILLRECT, TOOL_ELLIPSE, TOOL_FILLELLIPSE, TOOL_ERASER,
    NUM_TOOLS
};
static const char* toolLabels[NUM_TOOLS] = {
    "Pencil", "Line", "Rect", "Fill Rect", "Ellipse", "Fill Ellipse", "Eraser"
};
static hipe_loc toolButtons[NUM_TOOLS];
static int currentTool = TOOL_PENCIL;

static int isShapeTool(int tool) {
    return tool == TOOL_LINE || tool == TOOL_RECT || tool == TOOL_FILLRECT
        || tool == TOOL_ELLIPSE || tool == TOOL_FILLELLIPSE;
}

/* Colour palette: the classic 16-colour Windows VGA palette used by MS Paint on
 * Windows 95, in its usual 2-row arrangement (row 1 = the 8 "dark"/pure colours, row 2
 * = the 8 "light"/grey-anchored colours). */
#define NUM_COLORS 16
static const char* colorNames[NUM_COLORS] = {
    "#000000", "#800000", "#008000", "#808000", "#000080", "#800080", "#008080", "#C0C0C0",
    "#808080", "#FF0000", "#00FF00", "#FFFF00", "#0000FF", "#FF00FF", "#00FFFF", "#FFFFFF"
};
static hipe_loc colorSwatches[NUM_COLORS];
static int fgColorIndex = 0;   /* black */
static int bgColorIndex = 15;  /* white */
static hipe_loc fgSwatch, bgSwatch; /* the two overlapping indicator squares */

static hipe_loc contentCanvas;  /* holds everything actually committed */
static hipe_loc overlayCanvas;  /* transparent, on top -- live shape preview only */
static hipe_loc clearButton;
static hipe_loc wrap;           /* the paper -- also the resize preview's positioning parent */
static hipe_loc resizeHandle;   /* small grip at the bottom-right corner */
static hipe_loc resizePreview;  /* dashed-outline box shown while dragging the handle */

/* Which mouse button started the drag currently being drawn -- 1 (left) draws with the
 * foreground colour, 3 (right) with the background colour. Set on mousedown, read by
 * prepareToolState() for the rest of that same drag. */
static int drawingButton = 1;

/* Sets a canvas element's width/height attributes to the current canvasWidth/
 * canvasHeight -- resizing either attribute clears that canvas's content, which is
 * why this is only ever called right after a resize actually completes, not during
 * the live drag (see the dashed resizePreview box for that instead). */
static void applyCanvasSize(hipe_loc loc) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", canvasWidth);
    hipe_send(session, HIPE_OP_SET_ATTRIBUTE, 0, loc, 2, "width", buf);
    snprintf(buf, sizeof(buf), "%d", canvasHeight);
    hipe_send(session, HIPE_OP_SET_ATTRIBUTE, 0, loc, 2, "height", buf);
}

static void selectTool(int tool) {
    setStyle(toolButtons[currentTool], "background-color", "#e8e8e8");
    setStyle(toolButtons[currentTool], "border", "2px outset #ccc");
    currentTool = tool;
    setStyle(toolButtons[currentTool], "background-color", "#ffd966");
    setStyle(toolButtons[currentTool], "border", "2px inset #ccc");
}

/* Maps a clicked location back to a palette index, or -1 if it isn't one of the
 * swatches -- there are only 16 of these, so a linear scan is plenty. */
static int swatchIndexAt(hipe_loc loc) {
    int i;
    for (i = 0; i < NUM_COLORS; i++)
        if (colorSwatches[i] == loc) return i;
    return -1;
}

static void setFgColor(int index) {
    fgColorIndex = index;
    setStyle(fgSwatch, "background-color", colorNames[fgColorIndex]);
}

static void setBgColor(int index) {
    bgColorIndex = index;
    setStyle(bgSwatch, "background-color", colorNames[bgColorIndex]);
}

/* Prepares the *currently selected* canvas's fillStyle/strokeStyle/lineWidth for
 * whatever currentTool is about to draw. Each canvas element has its own independent
 * context state, so this needs calling again after every useCanvas() switch between
 * content and overlay -- it's cheap, and canvas state is otherwise sticky, so it's
 * never called more often than that. Eraser always uses white regardless of the
 * foreground/background colours or which button is drawing; every other tool picks
 * foreground or background based on drawingButton (set at the start of the drag). */
static void prepareToolState() {
    const char* color;
    if (currentTool == TOOL_ERASER) {
        color = "white";
    } else {
        color = colorNames[(drawingButton == 3) ? bgColorIndex : fgColorIndex];
    }
    setProperty("fillStyle", color);
    setProperty("strokeStyle", color);
    setProperty("lineWidth", (currentTool == TOOL_ERASER) ? "20" : "3");
}

/* Draws whatever currentTool's shape looks like between two points, onto whichever
 * canvas is currently selected -- shared by both the live preview (drawn on the
 * overlay on every mousemove) and the final commit (drawn on the content canvas on
 * mouseup), so the preview is always an exact match for what gets committed. Only
 * meaningful for the shape tools (isShapeTool()) -- pencil/eraser draw continuously
 * and never call this. */
static void drawShape(int x1, int y1, int x2, int y2) {
    char args[96];
    switch (currentTool) {
    case TOOL_LINE:
        doAction("beginPath", NULL);
        snprintf(args, sizeof(args), "%d,%d", x1, y1);
        doAction("moveTo", args);
        snprintf(args, sizeof(args), "%d,%d", x2, y2);
        doAction("lineTo", args);
        doAction("stroke", NULL);
        break;
    case TOOL_RECT:
    case TOOL_FILLRECT: {
        int rx = x1 < x2 ? x1 : x2;
        int ry = y1 < y2 ? y1 : y2;
        int rw = abs(x2 - x1);
        int rh = abs(y2 - y1);
        snprintf(args, sizeof(args), "%d,%d,%d,%d", rx, ry, rw, rh);
        doAction(currentTool == TOOL_RECT ? "strokeRect" : "fillRect", args);
        break;
    }
    case TOOL_ELLIPSE:
    case TOOL_FILLELLIPSE: {
        double cx = (x1 + x2) / 2.0;
        double cy = (y1 + y2) / 2.0;
        double rx = fabs(x2 - x1) / 2.0;
        double ry = fabs(y2 - y1) / 2.0;
        snprintf(args, sizeof(args), "%.1f,%.1f,%.1f,%.1f,0,0,%.5f,false", cx, cy, rx, ry, 2 * M_PI);
        doAction("beginPath", NULL);
        doAction("ellipse", args);
        doAction(currentTool == TOOL_ELLIPSE ? "stroke" : "fill", NULL);
        break;
    }
    }
}

int main(int argc, char** argv) {

    session = hipe_open_session(argc > 1 ? argv[1] : 0, 0, 0, "canvas");
    if (!session) exit(1);

    setStyle(0, "margin", "0");
    setStyle(0, "background-color", "grey");
    setStyle(0, "font-family", "sans-serif");
    setStyle(0, "display", "flex");
    setStyle(0, "flex-direction", "row");
    /* Without this, dragging the resize handle (or doing any mouse-drag gesture
     * anywhere outside the individual buttons/swatches, which already set this
     * locally) lets the browser's native text/content selection kick in alongside our
     * own custom drag tracking -- selection-range computation and highlight painting
     * during the drag is real, visible overhead that has nothing to do with our own
     * event handling, and looks like general sluggishness. This WebKit fork only
     * recognises the vendor-prefixed property (checked against
     * Source/WebCore/css/CSSPropertyNames.in) -- plain "user-select" parses as an
     * unrecognised property and is silently dropped, which is exactly what happened
     * the first time this was added here. */
    setStyle(0, "-webkit-user-select", "none");
    /* Requesting "contextmenu" gets preventDefault applied automatically server-side
     * (see hiped's handle_EVENT_REQUEST) -- registering it once on body suppresses the
     * native right-click menu page-wide via event bubbling, which right-click-for-
     * background-colour and right-drag-to-draw-in-background-colour both depend on. */
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 0, 0, 1, "contextmenu");

    /* Left toolbar: a vertical column of tool buttons plus the clear button. */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 2, "div", "leftToolbar");
    hipe_loc leftToolbar = getLoc("leftToolbar");
    setStyle(leftToolbar, "display", "flex");
    setStyle(leftToolbar, "flex-direction", "column");
    setStyle(leftToolbar, "flex", "0 0 auto");
    setStyle(leftToolbar, "background-color", "#d4d0c8");
    setStyle(leftToolbar, "border-right", "2px solid #888");
    setStyle(leftToolbar, "padding", "4px");

    for (int i = 0; i < NUM_TOOLS; i++) {
        char id[16];
        snprintf(id, sizeof(id), "tool%d", i);
        toolButtons[i] = makeButton(leftToolbar, id, toolLabels[i]);
        setStyle(toolButtons[i], "display", "block");
        setStyle(toolButtons[i], "width", "165px");
        setStyle(toolButtons[i], "white-space", "nowrap");
        setStyle(toolButtons[i], "box-sizing", "border-box");
        setStyle(toolButtons[i], "text-align", "center");
    }
    selectTool(TOOL_PENCIL);

    clearButton = makeButton(leftToolbar, "clearBtn", "Clear");
    setStyle(clearButton, "display", "block");
    setStyle(clearButton, "width", "165px");
    setStyle(clearButton, "white-space", "nowrap");
    setStyle(clearButton, "box-sizing", "border-box");
    setStyle(clearButton, "text-align", "center");
    setStyle(clearButton, "margin-top", "16px");

    /* Main area: the top palette bar above the canvas, both to the right of the
     * vertical tool column -- kept in their own flex-column container so the tool
     * column's own height doesn't get stretched by the canvas below it. */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, 0, 2, "div", "mainArea");
    hipe_loc mainArea = getLoc("mainArea");
    setStyle(mainArea, "display", "flex");
    setStyle(mainArea, "flex-direction", "column");
    setStyle(mainArea, "flex", "1 1 auto");

    /* Top palette bar: the foreground/background indicator, then 16 colour swatches
     * in 2 rows of 8 -- the classic Windows VGA palette, same layout MS Paint used. */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, mainArea, 2, "div", "topBar");
    hipe_loc topBar = getLoc("topBar");
    setStyle(topBar, "display", "flex");
    setStyle(topBar, "flex-direction", "row");
    setStyle(topBar, "align-items", "center");
    setStyle(topBar, "background-color", "#d4d0c8");
    setStyle(topBar, "border-bottom", "2px solid #888");
    setStyle(topBar, "padding", "4px");

    /* fg/bg indicator: two overlapping squares, background offset down-right behind,
     * foreground offset up-left in front -- same visual convention as MS Paint's. */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, topBar, 2, "div", "fgbgIndicator");
    hipe_loc fgbgIndicator = getLoc("fgbgIndicator");
    setStyle(fgbgIndicator, "position", "relative");
    setStyle(fgbgIndicator, "width", "36px");
    setStyle(fgbgIndicator, "height", "36px");
    setStyle(fgbgIndicator, "margin-right", "12px");
    setStyle(fgbgIndicator, "flex", "0 0 auto");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, fgbgIndicator, 2, "div", "bgSwatch");
    bgSwatch = getLoc("bgSwatch");
    setStyle(bgSwatch, "position", "absolute");
    setStyle(bgSwatch, "right", "0");
    setStyle(bgSwatch, "bottom", "0");
    setStyle(bgSwatch, "width", "20px");
    setStyle(bgSwatch, "height", "20px");
    setStyle(bgSwatch, "border", "2px inset #888");
    setStyle(bgSwatch, "background-color", colorNames[bgColorIndex]);

    /* Appended after bgSwatch so it paints on top in the shared stacking context. */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, fgbgIndicator, 2, "div", "fgSwatch");
    fgSwatch = getLoc("fgSwatch");
    setStyle(fgSwatch, "position", "absolute");
    setStyle(fgSwatch, "left", "0");
    setStyle(fgSwatch, "top", "0");
    setStyle(fgSwatch, "width", "20px");
    setStyle(fgSwatch, "height", "20px");
    setStyle(fgSwatch, "border", "2px outset #888");
    setStyle(fgSwatch, "background-color", colorNames[fgColorIndex]);

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, topBar, 2, "div", "paletteContainer");
    hipe_loc paletteContainer = getLoc("paletteContainer");
    setStyle(paletteContainer, "display", "flex");
    setStyle(paletteContainer, "flex-direction", "column");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, paletteContainer, 2, "div", "paletteRow1");
    hipe_loc paletteRow1 = getLoc("paletteRow1");
    setStyle(paletteRow1, "display", "flex");
    setStyle(paletteRow1, "flex-direction", "row");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, paletteContainer, 2, "div", "paletteRow2");
    hipe_loc paletteRow2 = getLoc("paletteRow2");
    setStyle(paletteRow2, "display", "flex");
    setStyle(paletteRow2, "flex-direction", "row");

    for (int i = 0; i < NUM_COLORS; i++) {
        char id[16];
        snprintf(id, sizeof(id), "color%d", i);
        hipe_loc rowParent = (i < 8) ? paletteRow1 : paletteRow2;
        colorSwatches[i] = makeSwatch(rowParent, id, colorNames[i]);
    }

    /* Canvas area: a wrapper div carries the paper's white background/border, and
     * holds two same-sized, same-positioned, borderless canvases stacked on top of
     * each other -- contentCanvas (everything actually committed) and overlayCanvas
     * (transparent, on top, used only for a live shape preview while dragging). Both
     * being borderless avoids a border-width misalignment between the two: if either
     * had its own border, its drawing surface would sit a couple of pixels off from
     * the other's. */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, mainArea, 2, "div", "canvasWrap");
    wrap = getLoc("canvasWrap");
    setStyle(wrap, "position", "relative");
    setStyle(wrap, "margin", "0");
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%dpx", canvasWidth);
        setStyle(wrap, "width", buf);
        snprintf(buf, sizeof(buf), "%dpx", canvasHeight);
        setStyle(wrap, "height", buf);
    }
    setStyle(wrap, "background-color", "white");
    setStyle(wrap, "border", "2px inset");

    /* wrap's own frame-relative position, queried once, now that the whole
     * surrounding layout (left toolbar + top bar) is in place and wrap's final
     * position on the page is settled -- needed because giving wrap "position:
     * relative" (to let the two canvases stack via absolute positioning) makes it the
     * CSS offsetParent for both of them. That breaks using the mouse event's own
     * offsetX/offsetY directly: those are computed server-side as pageX/pageY minus
     * the target element's offsetLeft/offsetTop, which are relative to the
     * *offsetParent* (now wrap), not the page -- so they'd end up close to raw
     * pageX/pageY again, off by wrap's own position on the page. Converting to
     * canvas-relative coordinates ourselves from pageX/pageY (frame-relative, like
     * HIPE_OP_GEOMETRY_RETURN's own x/y) sidesteps the CSS offsetParent quirk entirely. */
    hipe_send(session, HIPE_OP_GET_GEOMETRY, 0, wrap, 0);
    hipe_instruction geom;
    hipe_instruction_init(&geom);
    hipe_await_instruction(session, &geom, HIPE_OP_GEOMETRY_RETURN);
    int wrapX = atoi(geom.arg[0]);
    int wrapY = atoi(geom.arg[1]);

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, wrap, 2, "canvas", "content");
    contentCanvas = getLoc("content");
    setStyle(contentCanvas, "position", "absolute");
    setStyle(contentCanvas, "top", "0");
    setStyle(contentCanvas, "left", "0");
    applyCanvasSize(contentCanvas);
    useCanvas(contentCanvas);
    setProperty("lineCap", "round");
    setProperty("lineJoin", "round");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, wrap, 2, "canvas", "overlay");
    overlayCanvas = getLoc("overlay");
    setStyle(overlayCanvas, "position", "absolute");
    setStyle(overlayCanvas, "top", "0");
    setStyle(overlayCanvas, "left", "0");
    applyCanvasSize(overlayCanvas);
    useCanvas(overlayCanvas);
    setProperty("lineCap", "round");
    setProperty("lineJoin", "round");
    useCanvas(contentCanvas); /* leave content selected as the default */

    /* Resize handle: a small grip at the bottom-right corner of wrap. Dragging it
     * only updates a dashed-outline preview box (pure CSS, no canvas operations) --
     * the canvases themselves are only actually resized on mouseup, since resizing
     * either one's width/height attribute clears its content. */
    hipe_send(session, HIPE_OP_APPEND_TAG, 0, wrap, 2, "div", "resizeHandle");
    resizeHandle = getLoc("resizeHandle");
    setStyle(resizeHandle, "position", "absolute");
    setStyle(resizeHandle, "right", "-7px");
    setStyle(resizeHandle, "bottom", "-7px");
    setStyle(resizeHandle, "width", "12px");
    setStyle(resizeHandle, "height", "12px");
    setStyle(resizeHandle, "background-color", "#888");
    setStyle(resizeHandle, "border", "1px solid #444");
    setStyle(resizeHandle, "cursor", "nwse-resize");
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 0, resizeHandle, 1, "mousedown");

    hipe_send(session, HIPE_OP_APPEND_TAG, 0, wrap, 2, "div", "resizePreview");
    resizePreview = getLoc("resizePreview");
    setStyle(resizePreview, "position", "absolute");
    setStyle(resizePreview, "top", "0");
    setStyle(resizePreview, "left", "0");
    setStyle(resizePreview, "box-sizing", "border-box");
    setStyle(resizePreview, "border", "2px dashed red");
    setStyle(resizePreview, "pointer-events", "none");
    setStyle(resizePreview, "display", "none");

    /* Mouse events for drawing go on the overlay -- it's the topmost element, so it's
     * what the cursor actually hits regardless of which tool is active. mousemove/
     * mouseup are ALSO requested on the body: once a resize drag starts (on
     * resizeHandle's own mousedown), the cursor immediately leaves that tiny handle,
     * so continuing to track the drag needs a target wide enough to keep receiving
     * events for the rest of it. The event loop tells the two sources apart via
     * hi.location, and normal drawing is gated on "not currently resizing" so a
     * resize drag's body-level moves/ups can never be mistaken for a draw. */
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 0, overlayCanvas, 1, "mousedown");
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 0, overlayCanvas, 1, "mousemove");
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 0, overlayCanvas, 1, "mouseup");
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 0, 0, 1, "mousemove");
    hipe_send(session, HIPE_OP_EVENT_REQUEST, 0, 0, 1, "mouseup");

    /* Event loop. */
    hipe_instruction hi;
    hipe_instruction_init(&hi);
    short nowDrawing = 0;
    int startX = 0, startY = 0;
    short resizing = 0;
    int resizeStartMouseX = 0, resizeStartMouseY = 0;
    int resizeStartWidth = 0, resizeStartHeight = 0;

    while (1) {
        hipe_next_instruction(session, &hi, 1);

        if (hi.opcode == HIPE_OP_EVENT) {
            int isMouseEvt = strncmp("mousedown", hi.arg[0], hi.arg_length[0]) == 0
                          || strncmp("mousemove", hi.arg[0], hi.arg_length[0]) == 0
                          || strncmp("mouseup", hi.arg[0], hi.arg_length[0]) == 0;

            if (isMouseEvt) {
                /* Format: which,pageX,pageY,offsetX,offsetY. which is button+1 (1=left,
                 * 2=middle, 3=right). The offsetX/offsetY fields are NOT used here --
                 * see the wrapX/wrapY comment above for why they're unreliable now that
                 * wrap positions the two canvases via "position: relative"/"absolute".
                 * pageX/pageY (frame-relative, same frame of reference as
                 * HIPE_OP_GEOMETRY_RETURN's own x/y) minus wrap's own frame-relative
                 * position gives the correct canvas-relative coordinate instead. */
                int which, pageX, pageY, unusedOffsetX, unusedOffsetY;
                char detail[64];
                int len = hi.arg_length[1] < (int)sizeof(detail) - 1 ? hi.arg_length[1] : (int)sizeof(detail) - 1;
                memcpy(detail, hi.arg[1], len);
                detail[len] = '\0';
                sscanf(detail, "%d,%d,%d,%d,%d", &which, &pageX, &pageY, &unusedOffsetX, &unusedOffsetY);
                int x = pageX - wrapX;
                int y = pageY - wrapY;

                int isDown = strncmp("mousedown", hi.arg[0], hi.arg_length[0]) == 0;
                int isUp = strncmp("mouseup", hi.arg[0], hi.arg_length[0]) == 0;
                int swatchIdx = isDown ? swatchIndexAt(hi.location) : -1;

                if (isDown && hi.location == resizeHandle) {
                    /* Start a resize drag. Raw page coordinates, not canvas-relative
                     * ones -- only the *delta* matters here, so wrap's own position
                     * cancels out regardless of frame of reference. */
                    resizing = 1;
                    resizeStartMouseX = pageX; resizeStartMouseY = pageY;
                    resizeStartWidth = canvasWidth; resizeStartHeight = canvasHeight;
                    char buf[16];
                    snprintf(buf, sizeof(buf), "%dpx", canvasWidth);
                    setStyle(resizePreview, "width", buf);
                    snprintf(buf, sizeof(buf), "%dpx", canvasHeight);
                    setStyle(resizePreview, "height", buf);
                    setStyle(resizePreview, "display", "block");
                } else if (isDown && swatchIdx >= 0) {
                    /* Left (and, as a fallback, middle) sets the foreground colour;
                     * right sets the background colour -- matching MS Paint. */
                    if (which == 3) setBgColor(swatchIdx);
                    else setFgColor(swatchIdx);
                } else if (resizing && hi.location == 0 && (isUp || !isDown)) {
                    /* Continuing (mousemove) or finishing (mouseup) a resize drag --
                     * these arrive via body's own listeners, not the handle's, since
                     * the cursor is long gone from that tiny element by now. */
                    int newWidth = resizeStartWidth + (pageX - resizeStartMouseX);
                    int newHeight = resizeStartHeight + (pageY - resizeStartMouseY);
                    if (newWidth < CANVAS_MIN) newWidth = CANVAS_MIN;
                    if (newHeight < CANVAS_MIN) newHeight = CANVAS_MIN;

                    if (isUp) {
                        resizing = 0;
                        setStyle(resizePreview, "display", "none");
                        canvasWidth = newWidth;
                        canvasHeight = newHeight;
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%dpx", canvasWidth);
                        setStyle(wrap, "width", buf);
                        snprintf(buf, sizeof(buf), "%dpx", canvasHeight);
                        setStyle(wrap, "height", buf);
                        /* Resizing either attribute clears that canvas -- there's no
                         * cheap way to preserve existing content across a resize with
                         * just the plain hipe API, so this demo accepts the loss. */
                        applyCanvasSize(contentCanvas);
                        applyCanvasSize(overlayCanvas);
                        useCanvas(contentCanvas);
                        useCanvas(overlayCanvas);
                        useCanvas(contentCanvas);
                    } else {
                        char buf[16];
                        snprintf(buf, sizeof(buf), "%dpx", newWidth);
                        setStyle(resizePreview, "width", buf);
                        snprintf(buf, sizeof(buf), "%dpx", newHeight);
                        setStyle(resizePreview, "height", buf);
                    }
                } else if (!resizing && hi.location == overlayCanvas) {
                    if (isDown) {
                        if (which != 1 && which != 3) {
                            /* Middle button (or anything else): not a drawing gesture. */
                        } else {
                            drawingButton = which;
                            nowDrawing = 1;
                            startX = x; startY = y;
                            if (currentTool == TOOL_PENCIL || currentTool == TOOL_ERASER) {
                                useCanvas(contentCanvas);
                                prepareToolState();
                                doAction("beginPath", NULL);
                                char coords[32];
                                snprintf(coords, sizeof(coords), "%d,%d", x, y);
                                doAction("moveTo", coords);
                            }
                            /* Shape tools: nothing to draw yet, just remember the start point. */
                        }
                    } else if (isUp && nowDrawing) {
                        nowDrawing = 0;
                        char clearBuf[32];
                        if (currentTool == TOOL_PENCIL || currentTool == TOOL_ERASER) {
                            char coords[32];
                            snprintf(coords, sizeof(coords), "%d,%d", x, y);
                            doAction("lineTo", coords);
                            doAction("stroke", NULL);
                        } else if (isShapeTool(currentTool)) {
                            useCanvas(overlayCanvas);
                            doAction("clearRect", clearAllArgs(clearBuf)); /* remove the last preview */
                            useCanvas(contentCanvas);
                            prepareToolState();
                            drawShape(startX, startY, x, y); /* final commit */
                        }
                    } else if (!isDown && !isUp && nowDrawing) {
                        char clearBuf[32];
                        if (currentTool == TOOL_PENCIL || currentTool == TOOL_ERASER) {
                            char coords[32];
                            snprintf(coords, sizeof(coords), "%d,%d", x, y);
                            doAction("lineTo", coords);
                            doAction("stroke", NULL);
                            doAction("beginPath", NULL);
                            doAction("moveTo", coords);
                        } else if (isShapeTool(currentTool)) {
                            useCanvas(overlayCanvas);
                            doAction("clearRect", clearAllArgs(clearBuf));
                            prepareToolState();
                            drawShape(startX, startY, x, y); /* live preview only */
                        }
                    }
                }
            } else if (strncmp("click", hi.arg[0], hi.arg_length[0]) == 0) {
                int i;
                for (i = 0; i < NUM_TOOLS; i++) {
                    if (hi.location == toolButtons[i]) {
                        selectTool(i);
                        break;
                    }
                }
                if (hi.location == clearButton) {
                    char clearBuf[32];
                    useCanvas(contentCanvas);
                    doAction("clearRect", clearAllArgs(clearBuf));
                }
            }
        } else if (hi.opcode == HIPE_OP_SERVER_DENIED) { /* client orphaned by server */
            return 0;
        } else if (hi.opcode == HIPE_OP_FRAME_CLOSE) { /* close button clicked. */
            return 0;
        }
    }
}
