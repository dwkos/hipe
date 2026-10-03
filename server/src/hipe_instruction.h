/* Copyright (c) 2015-2026 Daniel Kos, General Development Systems

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of this Software library.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Hipe commons visible to library users.
 **/

#ifndef HIPECOM_H
#define HIPECOM_H

#include <sys/types.h>
#include <stdint.h>

#define HIPE_OP_CLEAR              1
/*clear the contents of the tag given by location (removing all child elements),
 *or clear body if location==0.*/

#define HIPE_OP_SET_TEXT           2
/* set a tag's contents to the text given in arg[0], overwriting previous contents.
 * if location==0 this applies to the entire body tag.
 * arg[1] is the text mode. In modes 0-2 < > " ' are shown as typed, never treated as markup.
 * arg[1] == 0 -- (default) '&' is shown as typed and whitespace is not converted: reading the
 *               element's text back returns arg[0] (e.g. source code in a <pre>).
 * arg[1] == 1 -- '&' is shown as typed; newlines, carriage returns and tabs are converted into line
 *               breaks, empty paragraphs and wide spaces, so the layout shows in any element.
 * arg[1] == 2 -- character entities are decoded (e.g. "&times;" shows as a multiplication sign);
 *               whitespace is not converted.
 * arg[1] == 3 -- arg[0] is HTML markup and is inserted as written. Elements with a hipe-loc="N" attribute get
 *               location N if N is listed in arg[2]: comma-separated numbers and ranges the client reserved for
 *               this markup, e.g. "57,1000-1499". The attribute is always removed. Other elements have no location.
 * In modes 0 and 2 a carriage return, with or without a following newline, is stored as one newline.
 */

#define HIPE_OP_APPEND_TEXT        3
/* append plain text inside the tag given by location, or inside body if location==0.
 * arg[0] is the text content to append.
 * arg[1] is the text mode, as for HIPE_OP_SET_TEXT.
 * arg[2] is the list of locations for markup (mode 3), as for HIPE_OP_SET_TEXT.
 */

#define HIPE_OP_APPEND_TAG         4
/* append a tag element inside the tag given by location, or inside body if location==0.
 * arg[0] is the tag type,
 * arg[1] is an optional identifier for the tag
 * arg[2] is an optional space-separated list of classes for the tag
 * arg[3] is optional initial text content, treated as by HIPE_OP_SET_TEXT in mode 0 (ignored for void tags e.g. br)
 */

#define HIPE_OP_ATTRIBUTE_RETURN   5
/* arg[0] is the name of the attribute and arg[1] is the retrieved value */

#define HIPE_OP_CONTAINER_GRANT    6
/* Server response to container request. Arg1 is "0" if the container request
 * was denied, or "1" if a new container has been granted. */

#define HIPE_OP_EVENT              7
/* Sent by the server to the client whenever a requested event occurs.
 * 'arg[0]' is the type of event, and 'location' is the tag for which events of 'arg[0]' were requested.
 * 'requestor' returns the same client-supplied value as was used for HIPE_OP_EVENT_REQUEST.
 * 'arg[1]' provides event-specific detail about the event -- e.g. for "click" events it returns the
 * number of clicks registered. For "wheel" events, 'arg[1]' is a comma-separated "deltaX,deltaY,deltaMode" triple:
 * deltaX/deltaY are positive when scrolling right/down respectively, and deltaMode is 0 (pixel),
 * 1 (line) or 2 (page), per the WheelEvent.deltaMode convention.
 * */

#define HIPE_OP_EVENT_CANCEL       8
/* Sent by the client to cancel notification of further events of type 'arg[0]' on location 'location'.
 * if arg[1]=="1", the server will reply with the same instruction to acknowledge that the event has been
 * cancelled. This may be useful to a client that needs to know when it is safe to clean up event listeners.
 * */

#define HIPE_OP_EVENT_REQUEST      9
/*Sent by the client to request notification of event 'arg[0]' on location 'location'.
 * arg[1] optionally cancels the event's default action (e.g. Tab moving the focus, or the wheel
 * scrolling), for events on the element or inside it that match any of a list of rules separated by ';'. Each
 * rule is "code,modifiers", written like the event's detail: code = keyCode (keydown/keyup), charCode (keypress)
 * or mouse button; modifiers = the mask (1 Shift, 2 Alt, 4 Ctrl, 8 Meta), matched exactly. "*" matches anything,
 * and ",modifiers" can be left out to match any. E.g. keydown "9,0;9,1" = Tab and Shift+Tab, wheel "*,4" =
 * Ctrl+wheel, "*" = every event. "-" cancels nothing. The rules replace any from an earlier request for the event
 * there. Empty cancels nothing, except for "contextmenu", whose default (the framing manager's edit menu) is
 * cancelled unless arg[1] is "-".
 * */

#define HIPE_OP_FREE_LOCATION      10
/* Sent by the client to the server to de-allocate a location index that is no longer required.
 * If arg[0] is not empty, it is a list of locations to free instead of the location field: comma-separated numbers
 * and ranges, e.g. "57,1000-1499" (see HIPE_OP_SET_TEXT mode 3).*/

#define HIPE_OP_GET_ATTRIBUTE      11
/* arg[0] is the name of the attribute*/

#define HIPE_OP_GET_BY_ID          12
/* Request a location index for the HTML tag with id attribute == arg[0]*/

#define HIPE_OP_GET_FIRST_CHILD    13

#define HIPE_OP_GET_GEOMETRY       14
/* Request a HIPE_OP_GEOMETRY reply with the x,y,width,height coordinates of location.*/

#define HIPE_OP_GET_LAST_CHILD     15
#define HIPE_OP_GET_NEXT_SIBLING   16
#define HIPE_OP_GET_PREV_SIBLING   17
/* Sent by client to request a location index relative to the supplied location*/

#define HIPE_OP_LOCATION_RETURN    18
/* Sent from server to client in immediate response to a 'get child' request.
 * Args are undefined (0), requestor is copied from the request, location is the requested location.*/

#define HIPE_OP_GEOMETRY_RETURN    19
/* arg[0] is x position and arg[1] is y position relative to containing frame.
 * arg[2] is the width and arg[3] is the height of the element itself. */

#define HIPE_OP_REQUEST_CONTAINER  20
/* this must be the first instruction received. arg[0] is the key, arg[1] is a short client name, arg[2] is the
 * theme index and arg[3] is the protocol version (HIPE_PROTOCOL_VERSION).*/

#define HIPE_OP_SERVER_DENIED      21
/*Sent by the server when a request is received but cannot be acted on due to
 *some critical violation. Usually this means that the client has not followed
 *protocol, e.g. is trying to send instructions even though a container request
 *was previously denied. Or, the session has been terminated at the server end.
 *arg[0] is the reason, if any. The server sends it just before disconnecting a client for a protocol violation.
 */

#define HIPE_OP_SET_ATTRIBUTE      22
/* arg[0] is the property and arg[1] is the new value */

#define HIPE_OP_SET_STYLE          23
/* arg[0] is the property and arg[1] is the new value */

#define HIPE_OP_ADD_STYLE_RULE     24 //to replace SET_STYLESHEET
/* Used to apply CSS style rules before other content is displayed.
 * arg[0] is the CSS descriptor of element(s) that the rule will apply to, and
 * arg[1] is the styling to apply, e.g. "border:0; width:100%;"
 */

#define HIPE_OP_SET_SRC            25
/*
 * arg[0] is the binary contents of the media file to be applied to the element.
 * arg[1] is the mime type of the source file to be applied to the element, e.g. "image/jpeg"
 * arg[2] is optional: "1" means more chunks of the same file will follow in subsequent SET_SRC
 * calls to this same location (arg[1] only matters on the first chunk), anything else/omitted
 * means this is the final (or only) chunk. Chunking lets a large file be streamed in progressively
 * -- decoded and rendered incrementally as each chunk arrives, same as an image loading over a
 * slow network -- rather than needing the whole file assembled client-side first. Images only for
 * now.
 *
 * A client that doesn't know in advance which chunk will be the last one can keep sending arg[2]
 * "1" through every real chunk, then finalize with one extra, separate SET_SRC call carrying no
 * new data (arg_length[0] == 0) and arg[2] omitted -- the finalize decision is made purely from
 * that final call's own arg[2], independent of whether it carries any bytes.
 *
 * arg[3] is optional and only meaningful for a CHUNKED audio/video load (ignored for images, and
 * pointless -- though harmless -- on a single-shot, non-chunked call): the total size, in bytes,
 * of the complete file across all chunks, if known in advance -- e.g. a client reading a real file
 * off disk knows this immediately via stat(), before sending anything. Like arg[1], it only
 * matters on the very first chunk of a chunked sequence and is ignored on every later chunk of
 * that same sequence -- there's no need to resend it, and it can't be changed mid-load. Omit it
 * entirely if the real total isn't known upfront (e.g. a genuinely open-ended/growing source);
 * chunked delivery still works correctly without it, just without the early-availability benefit
 * below. A single-shot call has nothing to gain from it either way: the whole file is already
 * fully present and finalized before decoding ever begins, so none of the below applies to it.
 *
 * Without arg[3], a chunked audio/video load cannot report metadata (duration, playable state) or
 * begin playback until the *entire* file has arrived, even though the underlying decoder can
 * usually parse enough of the file's structure (its header/index) from a small fraction of the
 * data to know the duration and start decoding immediately. This isn't a hipecore choice -- it's a
 * consequence of how the GStreamer backend verifies what it parses: as part of validating the
 * header it just read, it checks a location very close to the file's true end, and without knowing
 * the real total size in advance, hipecore has no way to tell "hasn't arrived yet" apart from
 * "genuinely past the end of the file," so it must wait for the transfer to finish before it can
 * safely answer that check. Providing arg[3] lets it answer immediately and correctly instead,
 * which is what actually unlocks early metadata/playback for chunked audio/video -- confirmed live
 * to enable playback starting from as little as 10% of a file having arrived. Progressive image
 * loading (arg[2] on its own) already renders incrementally without needing arg[3] at all --
 * images don't have this same demuxer-verification behavior.
 */

#define HIPE_OP_SET_TITLE          26
/* arg[0] overwrites the container's title with a new one.*/

#define HIPE_OP_GET_FRAME_KEY      28
/* Sent by client to request the hostkey for connecting to a particular iframe
 * location is the handle corresponding to the particular iframe.
 */

#define HIPE_OP_KEY_RETURN         29
/* Sent by the server in response to a GET_FRAME_KEY request.
 * arg[0] is the returned host key and location is the frame on which the request was made.
 */

#define HIPE_OP_FRAME_EVENT        30
/* Sent by the server to indicate an event affecting a direct sub-frame of this one,
 * such as a client connecting to the sub-frame, or the title having been changed by
 * the client.
 * Frame events are sent automatically once HIPE_OP_GET_FRAME_KEY has been requested for that frame.
 * The framing client doesn't specify which events should be received.
 * requestor is the requestor value that was originally passed last time GET_FRAME_KEY was called.
 * location is the iframe element.
 * arg[0] is the event type
 * arg[1] is event detail (if applicable).
 */

#define HIPE_OP_FRAME_CLOSE        31
/* Sent by a client that manages a subframe to indicate a request for the client occupying that
 * frame to terminate.
 * Location: the frame element
 * arg[0]: non-null string - convey a user's request to close that client (e.g. user clicks close button).
 *       null - tells hipe server to terminate client connection forcibly.
 */

#define HIPE_OP_TOGGLE_CLASS       32
/* Applies or removes a CSS class to/from an element.
 * arg[0]: the class name to apply/remove.
 */

#define HIPE_OP_SET_FOCUS          33
/* Keyboard-focuses the element at location */

#define HIPE_OP_SET_STYLE_SRC 34
/* Applies a background image or other property to the element at location.
 * arg[0] is the CSS property (e.g. background-image)
 * arg[1] is the binary contents of the media file.
 * arg[2] is the mime type of the binary data.
 * arg[3] is an optional suffix value: some CSS properties take an additional parameter, e.g.
 *        background may take a source image, followed by "repeat-x repeat-y" or similar.
 */

#define HIPE_OP_TAKE_SNAPSHOT      35
/* Takes a screenshot of the client frame.
 * arg[0] is the file format: "pdf" or "svg" for vector screenshots, "png" for a raster
 * screenshot. Known "svg" limitation: an element with both a CSS gradient background
 * and a plain (non-rounded) border loses the gradient fill in the output (renders
 * transparent) - a Qt SVG paint-engine limitation, not present in "pdf"/"png". A
 * gradient alone, or a gradient with border-radius and no border, is unaffected.
 */

#define HIPE_OP_FILE_RETURN        36
/* Sent to the client when the client requests a file (e.g. a snapshot of the frame contents).
 * requestor carries the value of the instruction that requested the file.
 */

#define HIPE_OP_ADD_STYLE_RULE_SRC 37
/* Sets background image data for a particular CSS style rule.
 * arg[0] is the CSS designator and arg[1] is the image file data, which should be PNG format.
 */

#define HIPE_OP_USE_CANVAS         38
/* Sets a canvas object at location to be the active canvas for drawing.
 * arg[0] is the canvas drawing context to be used (e.g. "2d").
 */

#define HIPE_OP_CANVAS_ACTION      39
/* Carries out a drawing method on the canvas object selected with HIPE_OP_USE_CANVAS.
 * arg[0] is the method to use (e.g. "fillRect") and arg[1] is a string of comma-separated
 * parameters (e.g "0,0,150,75").
 */

#define HIPE_OP_CANVAS_SET_PROPERTY 40
/* Sets a property on the current canvas context.
 * arg[0] is the property, arg[1] is the desired value.
 */

#define HIPE_OP_SET_ICON           41
/* Sets the client application's icon, which will be passed to the client's
 * parent environment for the purpose of helping the user identify this application.
 * arg[0] is the image file in PNG format. arg[1] is unused.
 */

#define HIPE_OP_REMOVE_ATTRIBUTE   42
/* Removes (unsets) the attribute specified in arg[0]. arg[1] is unused.
 */

#define HIPE_OP_MESSAGE            43
/* Sends an arbitrary message to another Hipe client with a direct parent/child relationship.
 * Or receives a message from another client.
 * Location: * 0 (or body element) means the message is being passed to/from the direct parent
 *               (which manages the client frame)
 *           * a frame element means message is being passed to/from that child frame's client.
 * arg[0], arg[1], requestor: passed through to other client unmodified. User-defined message data can be passed through here.
 */

#define HIPE_OP_GET_SCROLL_GEOMETRY 44
/*Requests the scroll geometry of the specified element. No arguments. Return
 *instruction is a HIPE_OP_GEOMETRY_RETURN instruction, where arg[0] is the x
 *scroll position at the left hand side, arg[1] is the y scroll position at the
 *visible top of the element, arg[2] is the total scrollable width of the
 *element content, and arg[3] is the total scrollable height.
 */

#define HIPE_OP_SCROLL_TO          45
#define HIPE_OP_SCROLL_BY          46
/*Scrolls the contents of the element TO an absolute position or BY a relative
 *(_BY) amount. arg[0] is the horizontal pixel offset to scroll to (or by).
 *arg[1] is the vertical pixel offset to scroll to (or by) To scroll in one
 *direction only, leave the unwanted argument unspecified.
 *arg[2] can be specified as "%" in which case a percentage of the scroll track
 *is used. Otherwise scrolling is in pixels.
 */

#define HIPE_OP_GET_CONTENT       47
/*Requests inner content of location; either flattened to unformatted plain
 *text or formatted to HTML (if arg[0] == "1" or "2"). Response is sent back from the
 *server as a HIPE_OP_CONTENT_RETURN instruction.
 * arg[0] == 0 -- unformatted plain text (default)
 * arg[0] == 1 -- contents formatted to HTML
 * arg[0] == 2 -- contents returned as formatted plain text (line breaks etc.)
 * arg[0] == 3 -- the element's value (input, textarea)
 * arg[0] == 4 -- the changes to the mode-0 text since this client's previous mode-4 request for the
 *               element (the first returns everything); arg[1] == "full" returns everything again.
 */

#define HIPE_OP_CONTENT_RETURN    48
/*Sent to client in response to a HIPE_OP_GET_CONTENT request. arg[0] contains
 *the requested content. requestor contains whatever was passed to the initial
 *request. Await this instruction after making a HIPE_OP_GET_CONTENT request.
 *For mode 4: arg[0] = new text of the changed span, arg[1] = its start and arg[2] = the length it
 *replaces in the previous text (characters), arg[3] = "delta"; or arg[0] = the whole text with
 *arg[3] = "full". Other modes leave arg[1..3] empty.
 */

#define HIPE_OP_DELETE 49
/*Request that the location itself be removed from the document.
 */

#define HIPE_OP_CARAT_POSITION 50
/*Sets the position of the text entry carat, or selects a range of text, in an input element such as a
 *<textarea> tag or an element with the contenteditable attribute.
 * arg[0] == character offset of the selection anchor (where the selection starts)
 * arg[1] == character offset of the selection focus (where the carat is), or same as arg[0]
 * if text is not selected. A focus before the anchor makes a backward selection.
 * arg[2] == "1" to scroll the selection into view (optional).
 * Offsets count characters (not bytes); negative offsets count from the end, -1 being after the
 * last character. Empty arguments leave the selection unchanged.
 * Also sent by the server in reply to HIPE_OP_GET_CARAT_POSITION, with arg[0] and arg[1] empty
 * if the element contains neither the carat nor selected text (a plain carat is reported as e.g. 3,3).
 */

#define HIPE_OP_GET_CARAT_POSITION 51
/*Requests the sever to send the client a HIPE_OP_CARAT_POSITION instruction
 *containing the *current* state of the input element.
 */

#define HIPE_OP_FIND_TEXT 52
/*Finds text in the style of a web browser's 'find on page', within the element given by location (0 = the
 *client's whole document; an <iframe> the client owns = that child client's frame), including frames nested
 *inside it. Highlights all matches and selects the next one after the current selection or text cursor there,
 *scrolling it into view and wrapping round at the end. Keyboard focus doesn't move.
 * arg[0] == the text to find. Empty removes the highlighting from the element.
 * arg[1] == options, any of: "b" find backwards (previous match), "c" match case, "w" whole words only.
 *The server replies with HIPE_OP_FIND_RESULT.
 */

#define HIPE_OP_ADD_FONT 53
/*Adds a @fontface style rule to the document
 *arg[0] == the name to be used to specify the font face in subsequent styling
 *arg[1] == the mime type for the font data (e.g. "application/x-font-woff")
 *arg[2] == the raw font file data itself.
 */

#define HIPE_OP_AUDIOVIDEO_STATE 54
/*If sent from the client to the server, modifies the state of an <audio> or
 *<video> tag. If received, this is in response to a ..._GET_AUDIOVIDEO_STATE
 *request and reflects the current status of the media element
 *arg[0] == the current playback position (in seconds) and the total duration (in seconds)
 *          separated by a ','. When setting the current playback position, it is
            sufficient to specify the desired position without the total duration.
 *arg[1] == the playback speed (1 == normal speed)
 *arg[2] == the playing/paused status. ("1" or "0")
 *arg[3] == the playback volume (fraction of 1.0)
 *--nonrequired arguments can be left empty.
 */

#define HIPE_OP_GET_AUDIOVIDEO_STATE 55
/*Requests a HIPE_OP_AUDIOVIDEO_STATE instruction containing the current state
 *of an <audio> or <video> tag.
 */

#define HIPE_OP_DIALOG 56
/*A dialog is a modal dialog display containing one or more forward options
 *and a built-in ability to close (cancel) the dialog without selecting an option.

 *"Modal" may be implemented with at least two possible behaviours: either
 *(a) the user cannot interact with the client until answering or dismissing
 *the dialog, or (b), refocusing the client (e.g. clicking outside the dialog)
 *automatically invokes dismissal of the dialog. The sending client cannot choose
 *this behaviour; it depends on how the message-receiver is designed.

 *If sent from the top level, Hipe handles rudimentary display of the dialog,
 *however the nominal case in a complete Hipe environment is for this to be
 *handled by a framing manager.
 *Otherwise, the next client up receives the instruction with the clientFrame
 *as the location. A client may
 *opt to retransmit the instruction up to the next level and then relay the
 *reply back to the client
 *
 *location == when sending: 0. When receiving: the clientFrame that sent it.
 *arg[0] == Dialog title text
 *arg[1] == Dialog prompt text; multiple lines allowed.
 *arg[2] == Choices, separated by newlines.
 *   (The cancel choice is not specifiable and is always provided)
 *   A separator may be specified by leaving a line blank.
 *
 *arg[3] == Character symbol (UTF8) for each option, separated by \n newlines.
 *   If no symbol is required, or a separator occupies that 'slot', a blank
 *   line should be supplied.
 *   - An additional symbol at the end of the list specifies a symbol for
 *     the dialog itself, e.g. "🛈"
 *   Symbols are the characters themselves; HTML character entities (e.g. "&#9888;")
 *   are not decoded.
 *
 */


#define HIPE_OP_DIALOG_RETURN 57
/* Location: always 0 when received. When sending, use the client-frame that
 *  awaits the response.
 *
 * Requestor: relays the value that was ued in requesting the dialog.
 * arg[0]: The text of the response chosen (or blank if cancelled)
 * arg[1]: The index (numbered from 1) or the response chosen, or 0 if cancelled.
 */


#define HIPE_OP_DIALOG_INPUT 58
/* An alternate version of HIPE_OP_DIALOG that allows free text input by the
 * User.
 * arg[0] == Dialog title text
 * arg[1] == Dialog prompt text. Multiple lines allowed, in which case the
 *   implementor may opt to style the final line differently as the main caption
 *   alongside the prompt itself, with prior lines preceding as explanatory text.
 * arg[2] == Suggested inputs, separated by newlines. The first line is the
 *   suggested default input. If the first line is blank, the input box should
 *   initially be empty. If the whole argument is blank, no suggestions are
 *   provided. Suggestions should be based on recent history (e.g. 5 recent commands)
 * arg[3] == Unicode character symbol (UTF8) for the question prompt.
 */


#define HIPE_OP_GET_SELECTION 59
/* Returns selected text in the current frame, or in all frames (top-level
 * frame only can request this). Selection is returned as plain text.
 * Response is sent back from the server as a HIPE_OP_CONTENT_RETURN instruction.
 * arg[0] == 0 -- get selection in current frame (default)
 * arg[0] == '1' -- get global selection (returns empty string if not the top-level element).
 */


#define HIPE_OP_EDIT_ACTION 60
/* Performs a cut/copy/paste or other edit operation on text the user has selected. This operation will
 * succeed or fail silently; there is no response from the server.
 * It applies to the frame with keyboard focus. location is 0 (current frame) or a child frame.
 * A top-level client may always act (focus is moved into a named child frame first); any other client
 * only while focus is inside its own/the named frame. Paste (v/V) from a non-top-level client also needs
 * recent user input in its frame, or a dialog answer relayed from the top level.
 * arg[0] == "x" -- attempts to perform a cut operation on selected text
 * arg[0] == "c" -- performs a copy operation on selected text
 * arg[0] == "v" -- performs a paste operation (matching destination formatting)
 * arg[0] == "V" -- performs a paste operation preserving source formatting.
 * arg[0] == "b" -- toggles bold if applicable
 * arg[0] == "i" -- italic
 * arg[0] == "u" -- underline
 * arg[0] == "k" -- strikethrough
 * arg[0] == "t" -- inserts the text in arg[1] at the caret, replacing any selected text, as if the user
 *   had typed it: nothing happens unless the caret is in editable content, and one undo ("z") removes
 *   the whole insertion (along with any typing immediately before it, as with ordinary typing).
 *   Newlines in the text become line breaks, as when the user presses Enter in plain text.
 *   The caret is left after the inserted text. If arg[1] is empty, the selected text is deleted
 *   (one undoable step) and nothing happens if no text is selected.
 */

#define HIPE_OP_EDIT_STATUS 61
/* Used for checking which edit actions are available (applicable to the
 * currently selected content or focused element). Takes a string of characters
 * (character codes used for HIPE_OP_EDIT_ACTION) in arg[0]; the reply's arg[1] holds one
 * status character per code: '0' available, '1' available and toggled on, 'e' unavailable.
 * Same frame rules as HIPE_OP_EDIT_ACTION; a frame that may not act gets 'e' for every code.
 */


#define HIPE_OP_FIFO_ADD_ABILITY 62
#define HIPE_OP_FIFO_REMOVE_ABILITY 63
#define HIPE_OP_FIFO_GET_PEER 64
#define HIPE_OP_FIFO_DROP_PEER 65
#define HIPE_OP_FIFO_OPEN 66
#define HIPE_OP_FIFO_CLOSE 67
#define HIPE_OP_FIFO_RESPONSE 68
/* Most of the FIFO operations have the same internal operation as the 
 * HIPE_OP_MESSAGE instruction, since the interpretation of these instructions
 * is left up to the framing manager rather than the hipe server itself.

 * The exception is HIPE_OP_FIFO_GET_PEER which, if the application is already
   a top-level frame, will display a system prompt for the user to select a real file
   to open/save rather than return a FIFO resource. (FIFOs are like 'virtual files')

   This allows native file open/save functionality on systems where a framing manager is
   not used.

 * In all of them, location 0 sends to the parent frame and a child frame's location sends to that
   frame; when received, location says which of those it came from. "Host" is the application that
   offers a resource, "client" the one that asked for it.
 * HIPE_OP_FIFO_ADD_ABILITY (host to framing manager): arg[0] == name of the ability offered, e.g.
   "Save"; arg[1] == access modes supported, e.g. "rw"; arg[2] == a description line, then one
   "ext:label" file type line per supported type ("*" any file, "/" any directory).
 * HIPE_OP_FIFO_REMOVE_ABILITY: arg[0] == name of the ability to withdraw.
 * HIPE_OP_FIFO_GET_PEER (client): arg[0] == suggested name for the resource, without extension;
   arg[1] == minimal access mode, optionally a space and every mode the client can use, e.g. "r rw";
   arg[2] == description and file types, as for an ability; arg[3] == blank from the client; the
   framing manager puts the chosen ability's name here when relaying to the host.
 * HIPE_OP_FIFO_RESPONSE (host, same requestor as the HIPE_OP_FIFO_GET_PEER): arg[0] == path of the
   FIFO resource, or blank if refused or cancelled; arg[1] == access modes granted; arg[2] == name
   the host gave the resource; arg[3] == its file type.
 * HIPE_OP_FIFO_OPEN (client, then relayed back by the host once its end is open): arg[0] == the
   resource path; arg[1] == one access mode; arg[2] == for R and W, "position" or "position,length";
   for m and M, the new name; arg[3] == for a directory resource, the entry's name.
 * HIPE_OP_FIFO_DROP_PEER: arg[0] == the resource path. The relationship is over; the host may
   discard the resource.

 * HIPE_OP_FIFO_CLOSE: arg[0] == the FIFO resource path; arg[1] == the outcome of the transfer at the
   sender's end: empty if it ended normally, otherwise the reason it failed (text for the user). Each end
   sends it once per transfer; the end that receives it first answers with its own. A host that cannot
   do what a HIPE_OP_FIFO_OPEN asked answers with HIPE_OP_FIFO_CLOSE and a reason in place of relaying
   the HIPE_OP_FIFO_OPEN back. A client that wrote data may treat it as saved once the host's
   HIPE_OP_FIFO_CLOSE arrives with arg[1] empty.

 * HIPE_OP_FIFO_OPEN access modes (arg[1]) that change a directory resource and transfer no data. Neither
   end opens the FIFO; the host does the operation and answers with HIPE_OP_FIFO_CLOSE (arg[1] empty if
   done, otherwise why not). arg[3] names the entry, as in the directory's list (directories end in '/'):
     "n" -- make a new empty directory. Fails if the name is taken or its parent doesn't exist.
     "m" -- move/rename the entry to the name in arg[2]. Fails if anything already has that name.
     "M" -- as "m", but a file that already has the new name is replaced, in a single step.
     "d" -- delete a file or an empty directory.
 */

#define HIPE_OP_OPEN_LINK 69
/* Used to communicate a hyperlink to the framing manager to be made available to the user
 * Same internal logic as HIPE_OP_MESSAGE.
 */

#define HIPE_OP_INSERT_TAG 70
/* Prepends a new tag before the tag given in location, with the same argument usage as
 * HIPE_OP_APPEND_TAG. */

#define HIPE_OP_SET_CURSOR 71
/* Sets the mouse cursor to a unicode character. The cursor is coloured to match the
   current foreground and background colours of the body element.
 * arg[0] is the unicode character to use as the cursor.
 * arg[1] is optional: the cursor's hotspot (the point that sits at the pointer position) as "x,y",
 * each a fraction of the cursor's square from 0 (left/top) to 1 (right/bottom), e.g. "0.5,0.5" for
 * the centre. With a hotspot the symbol is drawn centred in the square. Without one (or if arg[1] is
 * not two numbers) the symbol is drawn from the left edge and the hotspot is near its top-left.
 */

#define HIPE_OP_EDIT_CONTEXT_REQUEST 72
/* Indicates a child frame has reecieved an unhandled context menu request (e.g. the user
   has right-clicked on selected text and the app does not implement its own handler.)
   A framing manager that does not provide context menus may relay this instruction to
   its own parent.
*/

#define HIPE_OP_GET_X11_XID 73
/* Sent by the client to turn the <object> element given by location into an X11 embed
 * target. The server creates a native child X11 window at that element's render-box
 * position and keeps its geometry synced to CSS layout from then on; a HIPE_OP_X11_XID_RETURN
 * is sent back with the XID of that window. There is no reparent instruction - the client
 * is expected to XReparentWindow a foreign top-level window into the returned XID itself,
 * using its own Xlib connection.
 * location is the <object> element. No args.
 */

#define HIPE_OP_X11_XID_RETURN 74
/* Sent by the server in response to a GET_X11_XID request.
 * location is the <object> element the request was made on (echoed back).
 * arg[0] is the returned XID as a decimal string, or "0" if the element wasn't an
 * unreplaced <object> or the server has no X11 embed-target support.
 */

#define HIPE_OP_GET_STYLE 75
/* Sent by the client to request the *resolved* value of a CSS property on location (0 for the
 * client's own body) - i.e. QWebElement::ComputedStyle: cascaded and inherited rules applied, not
 * just a literal attribute. Use this instead of HIPE_OP_GET_ATTRIBUTE("style") to read a property
 * that's only set via an inherited/cascaded rule (e.g. a loaded theme stylesheet - see
 * Container::globalStyleSheets) rather than a literal inline style="..." on the element itself.
 * arg[0] is the CSS property name (e.g. "background-color", "color").
 * A HIPE_OP_STYLE_RETURN is sent back in response. Unlike HIPE_OP_SET_STYLE's own internal use of
 * ComputedStyle resolution (see handle_SET_STYLE), this does not retry/block waiting on layout - if
 * the element hasn't rendered yet, or the property name is invalid, the returned value may be an
 * empty string; the client should retry after a short delay if that matters to it.
 */

#define HIPE_OP_STYLE_RETURN 76
/* Sent by the server in response to a GET_STYLE request.
 * location is the element the request was made on (echoed back).
 * arg[0] is the property name (echoed back).
 * arg[1] is the resolved value as a string (e.g. "rgb(0, 0, 0)"), or an empty string if it couldn't
 * be resolved.
 */

#define HIPE_OP_GET_SRC 77
/* Sent by the client to request the source data of an <img> or <canvas> element -- the read-side
 * counterpart to HIPE_OP_SET_SRC. For an <img>, this returns the exact original bytes most recently
 * given via HIPE_OP_SET_SRC, verbatim, along with their original MIME type. For a <canvas>, there is
 * no "original bytes" to return (it never had a src), so this instead renders the canvas's current
 * pixel content on demand. Deliberately does not support <audio>/<video> -- a client that already
 * pushed a media file via HIPE_OP_SET_SRC has no legitimate reason to fetch the same, likely large,
 * payload back from the server.
 * location is the <img> or <canvas> element to read from.
 * arg[0] is a format hint, only consulted for <canvas>: "png" (default) or "pdf". Ignored for <img>,
 * which always returns its native format regardless of what (if anything) is passed here. "pdf" is
 * offered for <canvas> despite canvas content being fully rasterized either way -- not for vector
 * fidelity (there is none to preserve) but because a defined, print-ready page size is useful in its
 * own right, e.g. sending a signature pad or receipt canvas to a printer.
 * A HIPE_OP_SRC_RETURN is sent back in response.
 */

#define HIPE_OP_SRC_RETURN 78
/* Sent by the server in response to a GET_SRC request.
 * location is the element the request was made on (echoed back).
 * On success: arg[0] is the binary data, arg[1] is its MIME type (e.g. "image/png", "image/jpeg",
 * "application/pdf"), and arg[2] is absent/empty.
 * On failure (element isn't an <img>/<canvas>, or an <img> has no image data loaded yet): arg[0] and
 * arg[1] are absent/empty, and arg[2] is a human-readable error message.
 */

#define HIPE_OP_CANVAS_QUERY 79
/* Sent by the client to ask a question about the canvas object currently selected with
 * HIPE_OP_USE_CANVAS, for the handful of Canvas 2D methods that return a value rather than drawing
 * (HIPE_OP_CANVAS_ACTION has no reply channel, so these couldn't be served as ordinary actions).
 * location is not applicable -- same as HIPE_OP_CANVAS_ACTION, this applies to whichever canvas was
 * last selected via HIPE_OP_USE_CANVAS.
 * arg[0] is the query name: "isPointInPath", "isPointInStroke", or "measureText".
 * arg[1] is a comma-separated argument list, same splitting rules as HIPE_OP_CANVAS_ACTION's arg[1]:
 *   - "isPointInPath": pathId, x, y[, winding]. pathId is the id most recently given to a
 *     HIPE_OP_CANVAS_ACTION "createPath2D" call, or an empty string to test against this canvas's own
 *     current implicit path instead of a named one. winding is "nonzero" (default) or "evenodd".
 *     (pathId is always present, even when empty, specifically to avoid an arity ambiguity: the real
 *     Canvas 2D spec's isPointInPath(x, y, winding) and isPointInPath(path, x, y) overloads can both
 *     take exactly 3 arguments, which would be genuinely ambiguous to tell apart here -- always
 *     requiring pathId up front removes the ambiguity entirely, at the cost of a little verbosity for
 *     the no-path case.)
 *   - "isPointInStroke": pathId, x, y. Same pathId convention as above (empty string = implicit path).
 *   - "measureText": text. Measures using the canvas's current "font" property -- see also
 *     QWebElement::textWidth(), a similar measurement reached a different way (an element's own
 *     computed CSS font, rather than a canvas context's "font" property).
 * A HIPE_OP_CANVAS_QUERY_RETURN is sent back in response.
 */

#define HIPE_OP_CANVAS_QUERY_RETURN 80
/* Sent by the server in response to a CANVAS_QUERY request.
 * location is not applicable.
 * On success: arg[0] is the result as a string ("true"/"false" for isPointInPath/isPointInStroke, or
 * a decimal number for measureText's width), and arg[1] is absent/empty.
 * On failure (no context selected, unrecognised query name, wrong argument count, or an unknown
 * pathId): arg[0] is absent/empty, and arg[1] is a human-readable error message.
 */

#define HIPE_OP_MEASURE_TEXT 81
/* Measures text as if it were the content of the element given by location, in that element's font and
 * white-space/text-transform rules, without wrapping and without adding anything to the document.
 * arg[0] == the text; newlines start new lines if the element preserves them.
 * arg[1] == optional font size in CSS pixels to measure at instead of the element's own.
 * The server replies with HIPE_OP_TEXT_METRICS.
 */

#define HIPE_OP_TEXT_METRICS 82
/* Reply to HIPE_OP_MEASURE_TEXT, in CSS pixels:
 * arg[0] == width of the widest line, arg[1] == height of all lines (for one line, the line height),
 * arg[2] == font ascent, arg[3] == font descent. All empty if the element can't be measured.
 */

#define HIPE_OP_GET_RANGE_GEOMETRY 83
/* Requests the on-screen rectangles of a range of text in the element given by location.
 * arg[0], arg[1] == start and end character offsets, in HIPE_OP_CARAT_POSITION's units (negative counts
 * from the end). If arg[1] is empty, the range is a caret at arg[0]; if both are empty, the element's
 * current caret (the selection's focus). The server replies with HIPE_OP_RANGE_GEOMETRY.
 */

#define HIPE_OP_RANGE_GEOMETRY 84
/* Reply to HIPE_OP_GET_RANGE_GEOMETRY. arg[0] == one rectangle per line, "x,y,w,h" separated by ';',
 * in CSS pixels relative to the element's top-left corner as displayed. A caret is a zero-width
 * rectangle as tall as its line. Empty if there is nothing to report (e.g. no caret in the element).
 */

#define HIPE_OP_FIND_RESULT 85
/* Reply to HIPE_OP_FIND_TEXT. arg[0] == the number of matches found, arg[1] == the position of the selected
 * match among them (from 1; 0 if there are none), arg[2] == "1" if the search wrapped round to find it,
 * otherwise "0", arg[3] == "+" if the search stopped at its limit of 1000 matches (so there may be more),
 * otherwise empty.
 */

#define HIPE_OP_SERVER_NOTICE 86
/* Sent by the server to report a client bug that isn't fatal, e.g. a location number listed for markup but used on
 * no element in it. arg[0] is the message.
 */

#define HIPE_PROTOCOL_VERSION "3"
/* Sent by the client library in arg[3] of HIPE_OP_REQUEST_CONTAINER. The server refuses a missing or older version.
 */


/*--------------*/



#define HIPE_FRAME_EVENT_CLIENT_CONNECTED    1 //arg[1] is client name
#define HIPE_FRAME_EVENT_CLIENT_DISCONNECTED 2
#define HIPE_FRAME_EVENT_TITLE_CHANGED       3 //arg[1] is new title
#define HIPE_FRAME_EVENT_ICON_CHANGED        4 //arg[1] is binary PNG image data.
#define HIPE_FRAME_EVENT_COLOR_CHANGED       5 //arg[1] is the new foreground color
#define HIPE_FRAME_EVENT_BACKGROUND_CHANGED  6 //arg[1] is the new background color

#define HIPE_NARGS 4 //fixed maximum number of arguments per hipe instruction.

typedef uint64_t hipe_loc;

struct _hipe_instruction { /*for storing the (decoded) values of an instruction*/
    char opcode;
    uint64_t requestor;
    hipe_loc location;
    char* arg[HIPE_NARGS]; //up to 4 arguments may be transmitted per instruction.
    uint64_t arg_length[HIPE_NARGS]; //each argument's length is transmitted as a 64 bit value.

    struct _hipe_instruction* next; /*for implementing a linked list.*/
};
typedef struct _hipe_instruction hipe_instruction;

void hipe_instruction_init(hipe_instruction* obj);
/*Must be called on any new hipe_instruction instance (excepting shallow copies
 *of an existing instance). The same applies if a struct previously modified by
 *the user (e.g. assigning arg strings to it) is going to be reused for
 *collecting new input from hipe. This function doesn't allocate or free memory,
 *but it initialises the struct's values into a consistent state so that other
 *hipe functions can do so safely.
 */

void hipe_instruction_alloc(hipe_instruction* obj);
/*Allocates memory to store each argument in obj, according to arg_length[]
 *values already assigned to obj. Arguments allocated in this way should later
 *be freed with hipe_instruction_clear().
 */

void hipe_instruction_clear(hipe_instruction* obj);
/*Frees memory previously allocated within the instruction, and leaves the
 *struct in a clean state ready for reuse. Do not use this function to clear a
 *hipe_instruction for which you have allocated values yourself. Instead,
 *de-allocate the argument strings in a way consistent with how you allocated
 *them, then call hipe_instruction_init() to re-initialise the values into a
 *clean state.
 */

void hipe_instruction_copy(hipe_instruction* dest, hipe_instruction* src);
/*Deeply copies src values into dest, allocating memory for args.
 *Does not do any initialisation or deallocation of any previous values that may
 *have been present in dest previously.
 *When done with the instruction in dest, free the memory of its internal
 *variables with hipe_instruction_clear().
 */

void hipe_instruction_move(hipe_instruction* dest, hipe_instruction* src);
/*Moves allocations from src to dest. Similar to hipe_instruction_copy except no
 *new memory is allocated; the memory allocated to src is reassigned to dest,
 *and src is then re-initialised. The memory in dest should later be freed in a
 *manner consistent with how src was previously allocated. */

#endif
#ifdef __cplusplus
}
#endif
