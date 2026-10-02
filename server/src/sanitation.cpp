/*  Copyright (c) 2016-2026 Daniel Kos, General Development Systems

    This file is part of Hipe.

    Hipe is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Hipe is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Hipe.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "sanitation.h"
#include <ctype.h>
#include <QByteArray>

std::set<std::string> Sanitation::tagWhitelist;
std::set<std::string> Sanitation::attrWhitelist;
std::map<char, QWebPage::WebAction> Sanitation::editCodeMap;

void Sanitation::init()
{
    //Populate the whitelist of allowed HTML elements.
    //List taken from https://www.w3.org/community/webed/wiki/HTML/Elements, with forbidden/nonapplicable tags deleted.
    tagWhitelist = {    "section",
                        "nav",
                        "article",
                        "aside",
                        "h1",
                        "h2",
                        "h3",
                        "h4",
                        "h5",
                        "h6",
                        "hgroup",
                        "header",
                        "footer",
                        "address",
                        "p",
                        "hr",
                        "pre",
                        "blockquote",
                        "ol",
                        "ul",
                        "li",
                        "dl",
                        "dt",
                        "dd",
                        "figure",
                        "figcaption",
                        "div",
                        "center",
                        "a",
                        "abbr",
                        "b",
                        "bdo",
                        "big",
                        "br",
                        "cite",
                        "code",
                        "dfn",
                        "em",
                        "i",
                        "kbd",
                        "mark",
                        "q",
                        "rp",
                        "rt",
                        "ruby",
                        "s",
                        "samp",
                        "small",
                        "spacer",
                        "span",
                        "strong",
                        "sub",
                        "sup",
                        "time",
                        "tt",
                        "u",
                        "var",
                        "wbr",
                        "ins",
                        "del",
                        "img",
                        "iframe",
                        "video",
                        "audio",
                        "source",
                        "track",
                        "canvas", //will need to make an instruction for manipulating canvas.
                        "object", //used as hipexwm's X11 embed-target element, see HIPE_OP_GET_X11_XID.
                        "map",
                        "area",
                        "svg",
                        "frame",
                        "frameset",
                        "table",
                        "caption",
                        "colgroup",
                        "col",
                        "tbody",
                        "thead",
                        "tfoot",
                        "tr",
                        "td",
                        "th",
                        "form",
                        "fieldset",
                        "legend",
                        "label",
                        "input",
                        "button",
                        "select",
                        "datalist",
                        "optgroup",
                        "option",
                        "textarea",
                        "output",
                        "progress",
                        "meter",
                        "details",
                        "summary",
                        "command",
                        "menu"
                   };

    //List of attributes obtained from https://www.w3.org/TR/html4/index/attributes.html
    //This is a whitelist of safe attributes that the user can use freely without special instructions.
    //Assigned before the HAVE_HIPECORE-gated SVG attributes below (mirroring tagWhitelist's own
    //order just above) so that block's .insert() calls add to this list instead of a later plain
    //assignment silently discarding them -- confirmed that was happening: this attrWhitelist used to
    //be (re)assigned here, after the SVG insert(), wiping every SVG-specific attribute it had just
    //added.
    //
    //Deliberately no "class" entry: HIPE_OP_TOGGLE_CLASS (a dedicated, whitelist-free opcode with
    //add/remove/toggle semantics) predates this whitelist entirely (present since hipe's very first
    //commit, well before attrWhitelist existed) and is the intended path for class manipulation --
    //confirmed absent even from the original pre-whitelist if-chain this list replaced, not just an
    //oversight. Whitelisting "class" here too would open a second, worse-fitting path (whole-string
    //overwrite) that could clobber classes TOGGLE_CLASS had already applied elsewhere.
    attrWhitelist = {   "abbr",
                        "accept-charset",
                        "accesskey",
                        "align",
                        "alt",
                        "autoplay",
                        "border",
                        "cellpadding",
                        "cellspacing",
                        "char",
                        "charoff",
                        "checked",
                        "cols",
                        "colspan",
                        "contenteditable",
                        "controls",
                        "coords",
                        "dir",
                        "disabled",
                        "for",
                        "frame",
                        "frameborder",
                        "headers",
                        "height",
                        "id",
                        "label",
                        "loop",
                        "maxlength",
                        "multiple",
                        "muted",
                        "name",
                        "noresize",
			"preload",
                        "readonly",
                        "rows",
                        "rowspan",
                        "rules",
                        "scope",
                        "scrolling",
                        "selected",
                        "shape",
                        "size",
                        "span",
                        "summary",
                        "tabindex",
                        "title",
                        "type",
                        "usemap",
                        "valign",
                        "value",
                        "width",

                        //HTML5 additions that take effect without scripting or form submission.
                        "hidden",
                        "max",          //range and number inputs
                        "min",
                        "placeholder",
                        "step"
                    };

#ifdef HAVE_HIPECORE
    //DOM-level SVG drawing primitives. "svg" itself is whitelisted above for both profiles,
    //but its children never were, making it useless: this list was originally kept
    //restrictive because stock Qt5WebKit still has a JS engine and URL navigation, and SVG
    //carries its own <script> element and xlink:href/href link-style attributes that could
    //reach either. hipecore has no JS engine at all and no URL navigation (its loader blocks
    //every request that isn't data:/about:), so that risk doesn't apply here - this is
    //additive to the list above, gated to hipecore builds only.
    //
    //Elements can be created and nested correctly (hipecore commit 882848e2 fixed the bug
    //that made this a no-op). A previously-suspected hipecore-side bug where SVG's own
    //geometry/animated attributes - cx/cy/r/d/x1/y1/x2/y2/points/viewBox/etc - wouldn't
    //round-trip through HIPE_OP_SET_ATTRIBUTE/HIPE_OP_GET_ATTRIBUTE was re-investigated
    //2026-09-12 and found not to reproduce (covered by a regression test in hipecore's
    //tst_qwebelement now); the original finding appears to have been a testing-methodology
    //artifact.
    //
    //Deliberately NOT whitelisted (kept out on both profiles, no gate needed): "script" (SVG
    //has its own, inert or not), "foreignObject" (can embed arbitrary HTML), "a"/"use"/"image"
    //and their href/xlink:href attributes (external-resource references - not needed for
    //drawing shapes; add later as its own scoped decision if wanted).
    tagWhitelist.insert({    "g",
                             "path",
                             "rect",
                             "circle",
                             "ellipse",
                             "line",
                             "polyline",
                             "polygon",
                             "defs",
                             "linearGradient",
                             "radialGradient",
                             "stop",
                             "text",
                             "tspan",
                             "clipPath",
                             "symbol"
                        });
    attrWhitelist.insert({   "d",
                             "cx",
                             "cy",
                             "r",
                             "rx",
                             "ry",
                             "x1",
                             "y1",
                             "x2",
                             "y2",
                             "points",
                             "viewBox",
                             "preserveAspectRatio",
                             "fill",
                             "fill-opacity",
                             "fill-rule",
                             "stroke",
                             "stroke-width",
                             "stroke-opacity",
                             "stroke-linecap",
                             "stroke-linejoin",
                             "stroke-dasharray",
                             "opacity",
                             "transform",
                             "gradientUnits",
                             "gradientTransform",
                             "offset",
                             "stop-color",
                             "stop-opacity",
                             "text-anchor",
                             "dx",
                             "dy",
                             "clip-path"
                        });
#endif

    //hipe's EDIT instructions (HIPE_OP_EDIT_ACTION and HIPE_OP_EDIT_STATUS)
    //use character codes such as 'x' (cut), 'z' (undo), etc. to specify
    //the actions required. These need to be converted into Qt's enumerated
    //constants as an intermediary step to looking up the relevant QAction object(s)
    //needed to trigger those actions.
    editCodeMap = {     {'z', QWebPage::Undo},
                        {'Z', QWebPage::Redo},
                        {'a', QWebPage::SelectAll},

                        {'x', QWebPage::Cut},
                        {'c', QWebPage::Copy},
                        {'v', QWebPage::PasteAndMatchStyle}, //match destination formatting
                        {'V', QWebPage::Paste}, //match source formatting

                        {'b', QWebPage::ToggleBold},
                        {'i', QWebPage::ToggleItalic},
                        {'u', QWebPage::ToggleUnderline},
                        {'k', QWebPage::ToggleStrikethrough},

                        {'l', QWebPage::AlignLeft},
                        {'C', QWebPage::AlignCenter},
                        {'r', QWebPage::AlignRight},
                        {'j', QWebPage::AlignJustified},

                        {'d', QWebPage::SetTextDirectionDefault},
                        {'>', QWebPage::SetTextDirectionLeftToRight},
                        {'<', QWebPage::SetTextDirectionRightToLeft}
                    };
}

Sanitation::TextMode Sanitation::textModeFromArg(const std::string& arg) {
    if(arg == "1") return LAYOUT_CONVERTED;
    if(arg == "2") return ENTITIES_DECODED;
    return SHOWN_AS_TYPED;
}

std::string Sanitation::sanitisePlainText(std::string input, TextMode mode)
//Processes input string, replaces special HTML characters like < with their
//equivalent nonfunctional representations, like &lt;. This is used to prevent
//HTML injection attacks.
//
//SHOWN_AS_TYPED escapes '&' too and converts nothing, so reading the element's
//text back returns the input (e.g. source code in a <pre>). The HTML parser itself
//still stores a carriage return, with or without a following newline, as one newline.
//
//LAYOUT_CONVERTED is for showing text with its layout in any element:
// - '&' is escaped too, so character entities are shown as typed.
// - '\n' and '\r' are replaced by <br/> and <p/> respectively, and '\t' by
//   a wider-than-usual space, since HTML would otherwise collapse them.
//
//ENTITIES_DECODED leaves '&' alone, so that clients can use character entities
//(e.g. "&times;") to insert symbols.
{
    std::string output;
    for(size_t i=0; i<input.size(); i++) {
        if(mode == LAYOUT_CONVERTED && input[i] == '\n')
            output += "<br/>";
        else if(mode == LAYOUT_CONVERTED && input[i] == '\r')
            output += "<p></p>"; //I'd use <p/>, but webkit doesn't parse it right when adding one element at a time.
        else if(mode == LAYOUT_CONVERTED && input[i] == '\t')
            output += "&emsp;";  //tab
        else if(mode != ENTITIES_DECODED && input[i] == '&')
            output += "&amp;";
        else if(input[i] == '<')
            output += "&lt;";
        else if(input[i] == '>')
            output += "&gt;";
        else if(input[i] == '"')
            output += "&quot;";
        else if(input[i] == '\'')
            output += "&#39;";
        else
            output += input[i];
    }
    return output;
}

std::string Sanitation::sanitiseCanvasInstruction(std::string input) {
//removes parentheses, braces and semicolons from strings intended as canvas instructions or other JS arguments.
//This prevents injections of arbitrary javascript code, which can degrade systemwide
//performance among other things.
//If sanitation fails for any reason, the whole string is rejected.

    for(size_t i=0; i<input.size(); i++) {
        if(input[i] == '(') return "";
        else if(input[i] == ')') return "";
        else if(input[i] == ';') return "";
    }
    return input;
}

std::string Sanitation::toBase64(const std::string& binaryData) {
    QByteArray b64qData = QByteArray(binaryData.data(), binaryData.size()).toBase64();
    //Qt provides a nice convenience function here.
    return std::string(b64qData.data(), b64qData.size());
}

std::string Sanitation::toBase64(const char* data, size_t size) {
//alternative function to avoid the step of needing to convert data to std::string first.
    QByteArray b64qData = QByteArray(data, size).toBase64();
    //Qt provides a nice convenience function here.
    return std::string(b64qData.data(), b64qData.size());
}

std::string Sanitation::toLower(const char* text, size_t length) {
    std::string result(text, length);
    for(size_t i=0; i<length; i++)
        if(result[i] >= 'A' && result[i] <= 'Z') result[i] -= ('A'-'a');
    return result;
}


bool Sanitation::isAllowedAttribute(std::string input)
//returns true iff use of the tag attribute is permitted by Hipe.
//List of attributes adapted from https://www.w3.org/TR/html4/index/attributes.html
//This is a whitelist of safe attributes that the user can use freely without special instructions.
{
    if(attrWhitelist.find(input) != attrWhitelist.end())
        return true;
    return false;
}

bool Sanitation::isAllowedTag(std::string input)
//returns true if the specified HTML tag is an allowed type. (e.g. "button" tags are allowed, "script" tags are not).
{
    if(tagWhitelist.find(input) != tagWhitelist.end())
        return true;
    return false;
}

bool Sanitation::isAllowedCSS(std::string input)
{
    for(size_t i=0; i<input.size(); i++) {
        if(input[i] == '<') return false; //don't let the user break out of the stylesheet to inject html code.
        if(input[i] == '>') return false;
        if(input[i] == '{') return false; //the user isn't allowed to fill in a whole stylesheet directly, so has no need of these.
        if(input[i] == '}') return false;
        if((input[i] == 'u' || input[i] == 'U') && i+2 < input.size()) { //screen for URLs, which are not allowed to be entered directly.
            if((input[i+1] == 'r' || input[i+1] == 'R')
                    && (input[i+2] == 'l' || input[i+2] == 'L')) {
                //we've detected an instance of the string "url".
                //at this point, we'll reject the input if there is a '(' or whitespace followed by a '('.
                size_t j = i+3;
                while(j<input.size() && isspace((unsigned char)input[j])) //skip any whitespace.
                    j++;
                if(j<input.size() && input[j] == '(') return false;
            }
        }
    }
    return true;
}

QWebPage::WebAction Sanitation::editCodeLookup(char code) {
    try {
        return editCodeMap.at(code);
    } catch(std::out_of_range&) {
        return QWebPage::NoWebAction;
    }

}


std::string Sanitation::mouseCursorFromUnicode(const std::string& symbol, const std::string& fgColor, const std::string& bgColor) {
    //Creates SVG graphic data for a mouse cursor based on the unicode character symbol
    //and the foreground and background colors specified.
    //The returned string can be used as the value of the "cursor" CSS property

    /*std::string svgData = 
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "height='40' width='40' style='font-size:36px;'>"
        "<text y='30' "
        "style='fill:";
    svgData += fgColor;
    svgData += ";stroke:";
    svgData += bgColor;
    svgData += ";stroke-width:2; '>"; // stroke-linejoin:round;'>";
    svgData += symbol;
    svgData += "</text></svg>";*/


    std::string svgData = 
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "height='40' width='40' style='font-size:35px;'>";
    svgData += "<text y='30' stroke='" + bgColor + "' stroke-width='5'>" + symbol + "</text>";
    svgData += "<text y='30' fill='" + fgColor + "'>" + symbol + "</text>";
    svgData += "</svg>";

    std::string origin = "0 7"; //cursor hotspot position, relative to the top-left corner of the cursor image.
    std::string cssValue = "url(\"data:image/svg;base64,";
    cssValue += Sanitation::toBase64(svgData);
    cssValue += "\") ";
    cssValue += origin;

    return cssValue;
}
