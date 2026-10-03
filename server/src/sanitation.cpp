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
#include <cstdio>
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
    //Assigned before the SVG attributes below (mirroring tagWhitelist's own order just above):
    //that block's .insert() calls add to this list, so a plain assignment after them would
    //discard every SVG-specific attribute.
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

    //DOM-level SVG drawing primitives, added to the lists above. The engine has no JS engine
    //and no URL navigation (its loader blocks every request that isn't data:/about:), so
    //SVG's shapes are safe to allow.
    //
    //Deliberately NOT whitelisted: "script" (SVG
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
//Text added to a <style> element is raw text up to the next "</style", so "</" is the only sequence that could
//end the stylesheet early and start markup.
{
    return input.find("</") == std::string::npos;
}

QWebPage::WebAction Sanitation::editCodeLookup(char code) {
    try {
        return editCodeMap.at(code);
    } catch(std::out_of_range&) {
        return QWebPage::NoWebAction;
    }

}


std::string Sanitation::mouseCursorFromUnicode(const std::string& symbol, const std::string& fgColor, const std::string& bgColor,
                                               const std::string& hotspot) {
    //Creates SVG graphic data for a mouse cursor based on the unicode character symbol
    //and the foreground and background colors specified.
    //The returned string can be used as the value of the "cursor" CSS property

    //The symbol goes into SVG markup, so markup characters in it are escaped. '&' is left alone,
    //so a numeric character reference (e.g. "&#x2194;") still works as before.
    std::string text = sanitisePlainText(symbol, ENTITIES_DECODED);

    //hotspot: "x,y", each a fraction of the cursor's square from 0 (top/left) to 1 (bottom/right).
    double hotspotX, hotspotY;
    int consumed = 0;
    bool hasHotspot = sscanf(hotspot.c_str(), " %lf , %lf %n", &hotspotX, &hotspotY, &consumed) == 2
                      && consumed == (int) hotspot.size()
                      && hotspotX == hotspotX && hotspotY == hotspotY; //(not NaN)

    const int size = 40; //the cursor image is size x size pixels.
    std::string svgData =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" "
        "height='40' width='40' style='font-size:35px;'>";
    std::string origin; //cursor hotspot position in pixels, relative to the top-left corner of the cursor image.
    if(hasHotspot) {
        //the symbol is centred in the square, so that "0.5,0.5" is the middle of a symmetrical symbol.
        //The image is drawn by Qt's SVG image plugin, which ignores dominant-baseline and dy, so the
        //baseline is placed where it centres symbols of this font size vertically (measured: y=20 put
        //their middle 11-12 pixels too high; at y=32, six symbols tested were within 2.5 pixels).
        std::string placement = "x='20' y='32' text-anchor='middle'";
        svgData += "<text " + placement + " stroke='" + bgColor + "' stroke-width='5'>" + text + "</text>";
        svgData += "<text " + placement + " fill='" + fgColor + "'>" + text + "</text>";
        hotspotX = hotspotX < 0 ? 0 : hotspotX > 1 ? 1 : hotspotX;
        hotspotY = hotspotY < 0 ? 0 : hotspotY > 1 ? 1 : hotspotY;
        origin = std::to_string((int) (hotspotX * (size - 1) + 0.5)) + " " + std::to_string((int) (hotspotY * (size - 1) + 0.5));
    } else {
        svgData += "<text y='30' stroke='" + bgColor + "' stroke-width='5'>" + text + "</text>";
        svgData += "<text y='30' fill='" + fgColor + "'>" + text + "</text>";
        origin = "0 7";
    }
    svgData += "</svg>";

    std::string cssValue = "url(\"data:image/svg;base64,";
    cssValue += Sanitation::toBase64(svgData);
    cssValue += "\") ";
    cssValue += origin;

    return cssValue;
}
