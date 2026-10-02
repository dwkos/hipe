/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2009 Michelangelo De Simone <micdesim@gmail.com>
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "EmailInputType.h"

#include "HTMLInputElement.h"
#include "HTMLParserIdioms.h"
#include "InputTypeNames.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

// hipecore: the RFC-ish email-format check (typeMismatch/typeMismatchFor/typeMismatchText,
// backed by a JSC/Yarr regex) is removed - too specific a case to embed a regex engine for.
// type=email still renders as a single-line text field (isEmailField() true for the keyboard
// hint) and still comma-strips for `multiple`; it just never reports a validity mismatch.

const AtomicString& EmailInputType::formControlType() const
{
    return InputTypeNames::email();
}

bool EmailInputType::isEmailField() const
{
    return true;
}

bool EmailInputType::supportsSelectionAPI() const
{
    return false;
}

String EmailInputType::sanitizeValue(const String& proposedValue) const
{
    String noLineBreakValue = proposedValue.removeCharacters(isHTMLLineBreak);
    if (!element().multiple())
        return stripLeadingAndTrailingHTMLSpaces(noLineBreakValue);
    Vector<String> addresses;
    noLineBreakValue.split(',', true, addresses);
    StringBuilder strippedValue;
    for (unsigned i = 0; i < addresses.size(); ++i) {
        if (i > 0)
            strippedValue.append(',');
        strippedValue.append(stripLeadingAndTrailingHTMLSpaces(addresses[i]));
    }
    return strippedValue.toString();
}

} // namespace WebCore
