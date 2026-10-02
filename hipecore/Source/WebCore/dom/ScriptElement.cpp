/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2025-2026 General Development Systems
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
#include "ScriptElement.h"

#include "Element.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLScriptElement.h"
#include "MIMETypeRegistry.h"
#include "SVGNames.h"
#include "SVGScriptElement.h"
#include "ScriptableDocumentParser.h"
#include "TextNodeTraversal.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

ScriptElement::ScriptElement(Element& element, bool parserInserted, bool alreadyStarted)
    : m_element(element)
    , m_parserInserted(parserInserted)
    , m_alreadyStarted(alreadyStarted)
    , m_haveFiredLoad(false)
    , m_forceAsync(!parserInserted)
{
}

bool ScriptElement::shouldCallFinishedInsertingSubtree(ContainerNode& insertionPoint)
{
    return insertionPoint.inDocument() && !m_parserInserted;
}

void ScriptElement::finishedInsertingSubtree()
{
    ASSERT(!m_parserInserted);
    prepareScript();
}

void ScriptElement::childrenChanged()
{
    if (!m_parserInserted && m_element.inDocument())
        prepareScript();
}

void ScriptElement::handleSourceAttribute(const String& sourceUrl)
{
    if (ignoresLoadRequest() || sourceUrl.isEmpty())
        return;
    prepareScript();
}

void ScriptElement::handleAsyncAttribute()
{
    m_forceAsync = false;
}

// Helper function
static bool isLegacySupportedJavaScriptLanguage(const String& language)
{
    // FIXME: This function is not HTML5 compliant. These belong in the MIME registry as "text/javascript<version>" entries.
    typedef HashSet<String, ASCIICaseInsensitiveHash> LanguageSet;
    static NeverDestroyed<LanguageSet> languages;
    if (languages.get().isEmpty()) {
        languages.get().add("javascript");
        languages.get().add("javascript1.0");
        languages.get().add("javascript1.1");
        languages.get().add("javascript1.2");
        languages.get().add("javascript1.3");
        languages.get().add("javascript1.4");
        languages.get().add("javascript1.5");
        languages.get().add("javascript1.6");
        languages.get().add("javascript1.7");
        languages.get().add("livescript");
        languages.get().add("ecmascript");
        languages.get().add("jscript");
    }

    return languages.get().contains(language);
}

void ScriptElement::dispatchErrorEvent()
{
    m_element.dispatchEvent(Event::create(eventNames().errorEvent, false, false));
}

bool ScriptElement::isScriptTypeSupported(LegacyTypeSupport supportLegacyTypes) const
{
    String type = typeAttributeValue();
    String language = languageAttributeValue();
    if (type.isEmpty()) {
        if (language.isEmpty())
            return true; // Assume text/javascript.
        if (MIMETypeRegistry::isSupportedJavaScriptMIMEType("text/" + language))
            return true;
        if (isLegacySupportedJavaScriptLanguage(language))
            return true;
        return false;
    }
    if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(type.stripWhiteSpace()))
        return true;
    if (supportLegacyTypes == AllowLegacyTypeInTypeAttribute && isLegacySupportedJavaScriptLanguage(type))
        return true;
    return false;
}

// http://dev.w3.org/html5/spec/Overview.html#prepare-a-script
//
// hipecore: reduced to the up-front eligibility checks + the alreadyStarted flip. There
// is no script engine, so the element never fetches a resource and never executes;
// prepareScript() therefore always returns false.
bool ScriptElement::prepareScript(const TextPosition&, LegacyTypeSupport supportLegacyTypes)
{
    if (m_alreadyStarted)
        return false;

    bool wasParserInserted = m_parserInserted;
    if (wasParserInserted)
        m_parserInserted = false;
    if (wasParserInserted && !asyncAttributeValue())
        m_forceAsync = true;

    if (!hasSourceAttribute() && !m_element.firstChild())
        return false;
    if (!m_element.inDocument())
        return false;
    if (!isScriptTypeSupported(supportLegacyTypes))
        return false;

    if (wasParserInserted) {
        m_parserInserted = true;
        m_forceAsync = false;
    }

    m_alreadyStarted = true;
    return false;
}

bool ScriptElement::ignoresLoadRequest() const
{
    return m_alreadyStarted || m_parserInserted || !m_element.inDocument();
}

String ScriptElement::scriptContent() const
{
    StringBuilder result;
    for (auto* text = TextNodeTraversal::firstChild(m_element); text; text = TextNodeTraversal::nextSibling(*text))
        result.append(text->data());
    return result.toString();
}

ScriptElement* toScriptElementIfPossible(Element* element)
{
    if (is<HTMLScriptElement>(*element))
        return downcast<HTMLScriptElement>(element);

    if (is<SVGScriptElement>(*element))
        return downcast<SVGScriptElement>(element);

    return nullptr;
}

}
