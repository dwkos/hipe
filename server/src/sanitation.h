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

/// Sanitation class.
/// PURPOSE: provides static utility functions and resources for sanitising user input.

#ifndef SANITATION_H
#define SANITATION_H

#include <string>
#include <set>
#include <map>
#include <HipeCore/QWebPage>
#include <HipeCore/QWebElement>

class Sanitation
{
private:
    //sets to store whitelists of allowed HTML tags and attributes.
    static std::set<std::string> tagWhitelist;
    static std::set<std::string> attrWhitelist;

    //converts edit action code characters into Qt WebAction constants.
    static std::map<char, QWebPage::WebAction> editCodeMap;
public:
    static void init();
    //initialises whitelists and related sanitation data.
    //Call this before using any of the sanitisation functions in this class.

    enum TextMode { //values are the text mode numbers clients pass (see HIPE_OP_SET_TEXT).
        SHOWN_AS_TYPED = 0,   //'&' is escaped and nothing is converted: reading the element's text back returns the input.
        LAYOUT_CONVERTED = 1, //'&' is escaped; newlines, carriage returns and tabs are converted to markup.
        ENTITIES_DECODED = 2  //'&' is left alone, so character entities (e.g. "&times;") become symbols.
    };
    static TextMode textModeFromArg(const std::string& arg);
    //the text mode requested by a client in an instruction argument ("1", "2"; anything else is mode 0).

    static std::string sanitisePlainText(std::string input, TextMode mode=ENTITIES_DECODED);
    //mode defaults to ENTITIES_DECODED for the callers that sanitise attribute values, tag names and ids;
    //text content should pass the client's text mode.
    //convert HTML syntactical characters in input into harmless escaped character entities.

    static std::string toBase64(const std::string& binaryData);
    static std::string toBase64(const char* data, size_t size);
    static std::string toLower(const char* text, size_t size); //convert to lowercase
    static bool isAllowedAttribute(std::string input);
    static bool isAllowedTag(std::string input);
    static bool isAllowedCSS(std::string input);
    //true if input can be added to a <style> element's text as it stands (it contains no "</").

    static QWebPage::WebAction editCodeLookup(char code);


    static std::string mouseCursorFromUnicode(const std::string& symbol, const std::string& fgColor, const std::string& bgColor,
                                              const std::string& hotspot = "");
    //Generates a CSS value for a mouse cursor image based on the unicode character
    //symbol and the foreground and background colours.
    //The returned string can be used as the value of the "cursor" CSS property.
    //hotspot is HIPE_OP_SET_CURSOR's optional "x,y" (fractions of the cursor's square). Without a
    //valid hotspot the cursor is drawn and placed exactly as before hotspots existed.

};

#endif // SANITATION_H
