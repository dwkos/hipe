/**
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Inc.
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
 */

#include "config.h"
#include "CSSProperty.h"

#include "CSSValueList.h"
#include "RenderStyleConstants.h"
#include "StylePropertyShorthand.h"

#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

struct SameSizeAsCSSProperty {
    uint32_t bitfields;
    void* value;
};

COMPILE_ASSERT(sizeof(CSSProperty) == sizeof(SameSizeAsCSSProperty), CSSProperty_should_stay_small);

CSSPropertyID StylePropertyMetadata::shorthandID() const
{
    if (!m_isSetFromShorthand)
        return CSSPropertyInvalid;

    Vector<StylePropertyShorthand> shorthands = matchingShorthandsForLonghand(static_cast<CSSPropertyID>(m_propertyID));
    ASSERT(shorthands.size() && m_indexInShorthandsVector >= 0 && m_indexInShorthandsVector < shorthands.size());
    return shorthands[m_indexInShorthandsVector].id();
}

void CSSProperty::wrapValueInCommaSeparatedList()
{
    auto list = CSSValueList::createCommaSeparated();
    list.get().append(m_value.releaseNonNull());
    m_value = WTFMove(list);
}

static CSSPropertyID resolveToPhysicalProperty(TextDirection direction, WritingMode writingMode, LogicalBoxSide logicalSide, const StylePropertyShorthand& shorthand)
{
    return shorthand.properties()[mapLogicalSideToPhysicalSide(makeTextFlow(writingMode, direction), logicalSide)];
}

enum LogicalExtent { LogicalWidth, LogicalHeight };

static CSSPropertyID resolveToPhysicalProperty(WritingMode writingMode, LogicalExtent logicalSide, const CSSPropertyID* properties)
{
    if (writingMode == TopToBottomWritingMode || writingMode == BottomToTopWritingMode)
        return properties[logicalSide];
    return logicalSide == LogicalWidth ? properties[1] : properties[0];
}

static const StylePropertyShorthand& borderDirections()
{
    static const CSSPropertyID properties[4] = { CSSPropertyBorderTop, CSSPropertyBorderRight, CSSPropertyBorderBottom, CSSPropertyBorderLeft };
    static NeverDestroyed<StylePropertyShorthand> borderDirections(CSSPropertyBorder, properties);
    return borderDirections;
}

CSSPropertyID CSSProperty::resolveDirectionAwareProperty(CSSPropertyID propertyID, TextDirection direction, WritingMode writingMode)
{
    switch (propertyID) {
    case CSSPropertyMarginInlineEnd:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, marginShorthand());
    case CSSPropertyMarginInlineStart:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, marginShorthand());
    case CSSPropertyMarginBlockStart:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, marginShorthand());
    case CSSPropertyMarginBlockEnd:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, marginShorthand());
    case CSSPropertyPaddingInlineEnd:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, paddingShorthand());
    case CSSPropertyPaddingInlineStart:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, paddingShorthand());
    case CSSPropertyPaddingBlockStart:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, paddingShorthand());
    case CSSPropertyPaddingBlockEnd:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, paddingShorthand());
    case CSSPropertyBorderInlineEnd:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, borderDirections());
    case CSSPropertyBorderInlineStart:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, borderDirections());
    case CSSPropertyBorderBlockStart:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, borderDirections());
    case CSSPropertyBorderBlockEnd:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, borderDirections());
    case CSSPropertyBorderInlineEndColor:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, borderColorShorthand());
    case CSSPropertyBorderInlineStartColor:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, borderColorShorthand());
    case CSSPropertyBorderBlockStartColor:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, borderColorShorthand());
    case CSSPropertyBorderBlockEndColor:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, borderColorShorthand());
    case CSSPropertyBorderInlineEndStyle:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, borderStyleShorthand());
    case CSSPropertyBorderInlineStartStyle:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, borderStyleShorthand());
    case CSSPropertyBorderBlockStartStyle:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, borderStyleShorthand());
    case CSSPropertyBorderBlockEndStyle:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, borderStyleShorthand());
    case CSSPropertyBorderInlineEndWidth:
        return resolveToPhysicalProperty(direction, writingMode, EndSide, borderWidthShorthand());
    case CSSPropertyBorderInlineStartWidth:
        return resolveToPhysicalProperty(direction, writingMode, StartSide, borderWidthShorthand());
    case CSSPropertyBorderBlockStartWidth:
        return resolveToPhysicalProperty(direction, writingMode, BeforeSide, borderWidthShorthand());
    case CSSPropertyBorderBlockEndWidth:
        return resolveToPhysicalProperty(direction, writingMode, AfterSide, borderWidthShorthand());
    case CSSPropertyInlineSize: {
        const CSSPropertyID properties[2] = { CSSPropertyWidth, CSSPropertyHeight };
        return resolveToPhysicalProperty(writingMode, LogicalWidth, properties);
    }
    case CSSPropertyBlockSize: {
        const CSSPropertyID properties[2] = { CSSPropertyWidth, CSSPropertyHeight };
        return resolveToPhysicalProperty(writingMode, LogicalHeight, properties);
    }
    case CSSPropertyMinInlineSize: {
        const CSSPropertyID properties[2] = { CSSPropertyMinWidth, CSSPropertyMinHeight };
        return resolveToPhysicalProperty(writingMode, LogicalWidth, properties);
    }
    case CSSPropertyMinBlockSize: {
        const CSSPropertyID properties[2] = { CSSPropertyMinWidth, CSSPropertyMinHeight };
        return resolveToPhysicalProperty(writingMode, LogicalHeight, properties);
    }
    case CSSPropertyMaxInlineSize: {
        const CSSPropertyID properties[2] = { CSSPropertyMaxWidth, CSSPropertyMaxHeight };
        return resolveToPhysicalProperty(writingMode, LogicalWidth, properties);
    }
    case CSSPropertyMaxBlockSize: {
        const CSSPropertyID properties[2] = { CSSPropertyMaxWidth, CSSPropertyMaxHeight };
        return resolveToPhysicalProperty(writingMode, LogicalHeight, properties);
    }
    default:
        return propertyID;
    }
}

bool CSSProperty::isDirectionAwareProperty(CSSPropertyID propertyID)
{
    switch (propertyID) {
    case CSSPropertyBorderInlineEndColor:
    case CSSPropertyBorderInlineEndStyle:
    case CSSPropertyBorderInlineEndWidth:
    case CSSPropertyBorderInlineStartColor:
    case CSSPropertyBorderInlineStartStyle:
    case CSSPropertyBorderInlineStartWidth:
    case CSSPropertyBorderBlockStartColor:
    case CSSPropertyBorderBlockStartStyle:
    case CSSPropertyBorderBlockStartWidth:
    case CSSPropertyBorderBlockEndColor:
    case CSSPropertyBorderBlockEndStyle:
    case CSSPropertyBorderBlockEndWidth:
    case CSSPropertyMarginInlineEnd:
    case CSSPropertyMarginInlineStart:
    case CSSPropertyMarginBlockStart:
    case CSSPropertyMarginBlockEnd:
    case CSSPropertyPaddingInlineEnd:
    case CSSPropertyPaddingInlineStart:
    case CSSPropertyPaddingBlockStart:
    case CSSPropertyPaddingBlockEnd:
    case CSSPropertyInlineSize:
    case CSSPropertyBlockSize:
    case CSSPropertyMinInlineSize:
    case CSSPropertyMinBlockSize:
    case CSSPropertyMaxInlineSize:
    case CSSPropertyMaxBlockSize:
        return true;
    default:
        return false;
    }
}

} // namespace WebCore
