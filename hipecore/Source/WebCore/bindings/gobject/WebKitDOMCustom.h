/*
 *  Copyright (C) 2011 Igalia S.L.
 *  Copyright (C) 2025-2026 General Development Systems
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

// hipecore note: trimmed to the one idea worth keeping from WebKitGTK's hand-written DOM
// binding extras (the rest wrapped UserMessageHandlers/WebKitNamespace, both deleted from this
// fork — see project_hipecore_bucket1_removal / project_hipecore_page_bucket_removal memory).
// Not part of any build: this is GObject codegen output glue, and the GTK port is gone.
//
// The idea: WebCore::Element::lastChangeWasUserEdit() (still live in WebCore, unrelated to the
// gobject bindings) distinguishes a user-typed edit from a script-set value. QWebElement doesn't
// expose this today. If hiped ever wants form dirty-tracking (e.g. "did the user actually type
// something here, or did we set this value ourselves"), wrapping
// HTMLInputElement/HTMLTextAreaElement::lastChangeWasUserEdit() the way
// webkit_dom_html_input_element_is_edited() does below is a cheap, ready-made shape to copy.

#ifndef WebKitDOMCustom_h
#define WebKitDOMCustom_h

#include <glib-object.h>
#include <glib.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

/**
 * webkit_dom_html_text_area_element_is_edited:
 * @input: A #WebKitDOMHTMLTextAreaElement
 *
 * Returns: A #gboolean
 */
WEBKIT_API gboolean webkit_dom_html_text_area_element_is_edited(WebKitDOMHTMLTextAreaElement* input);

/**
 * webkit_dom_html_input_element_is_edited:
 * @input: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gboolean
 */
WEBKIT_API gboolean webkit_dom_html_input_element_is_edited(WebKitDOMHTMLInputElement* input);

G_END_DECLS

#endif
