/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2014 Apple Inc.
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
#include "HTMLPlugInElement.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "CSSPropertyNames.h"
#include "Document.h"
#include "Event.h"
#include "EventHandler.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "HTMLNames.h"
#include "Logging.h"
#include "Page.h"
#include "PluginData.h"
#include "PluginViewBase.h"
#include "RenderEmbeddedObject.h"
#include "RenderSnapshottedPlugIn.h"
#include "RenderWidget.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "SubframeLoader.h"
#include "Widget.h"

namespace WebCore {

using namespace HTMLNames;

HTMLPlugInElement::HTMLPlugInElement(const QualifiedName& tagName, Document& document)
    : HTMLFrameOwnerElement(tagName, document)
    , m_inBeforeLoadEventHandler(false)
    , m_swapRendererTimer(*this, &HTMLPlugInElement::swapRendererTimerFired)
    , m_isCapturingMouseEvents(false)
    , m_displayState(Playing)
{
    setHasCustomStyleResolveCallbacks();
}

HTMLPlugInElement::~HTMLPlugInElement()
{
}

bool HTMLPlugInElement::canProcessDrag() const
{
    const PluginViewBase* plugin = is<PluginViewBase>(pluginWidget()) ? downcast<PluginViewBase>(pluginWidget()) : nullptr;
    return plugin ? plugin->canProcessDrag() : false;
}

bool HTMLPlugInElement::willRespondToMouseClickEvents()
{
    if (isDisabledFormControl())
        return false;
    auto renderer = this->renderer();
    return renderer && renderer->isWidget();
}

void HTMLPlugInElement::willDetachRenderers()
{
    if (m_isCapturingMouseEvents) {
        if (Frame* frame = document().frame())
            frame->eventHandler().setCapturingMouseEventsElement(nullptr);
        m_isCapturingMouseEvents = false;
    }
}

bool HTMLPlugInElement::guardedDispatchBeforeLoadEvent(const String& sourceURL)
{
    // FIXME: Our current plug-in loading design can't guarantee the following
    // assertion is true, since plug-in loading can be initiated during layout,
    // and synchronous layout can be initiated in a beforeload event handler!
    // See <http://webkit.org/b/71264>.
    // ASSERT(!m_inBeforeLoadEventHandler);
    m_inBeforeLoadEventHandler = true;
    // static_cast is used to avoid a compile error since dispatchBeforeLoadEvent
    // is intentionally undefined on this class.
    bool beforeLoadAllowedLoad = static_cast<HTMLFrameOwnerElement*>(this)->dispatchBeforeLoadEvent(sourceURL);
    m_inBeforeLoadEventHandler = false;
    return beforeLoadAllowedLoad;
}

Widget* HTMLPlugInElement::pluginWidget(PluginLoadingPolicy loadPolicy) const
{
    if (m_inBeforeLoadEventHandler) {
        // The plug-in hasn't loaded yet, and it makes no sense to try to load if beforeload handler happened to touch the plug-in element.
        // That would recursively call beforeload for the same element.
        return nullptr;
    }

    RenderWidget* renderWidget = loadPolicy == PluginLoadingPolicy::Load ? renderWidgetLoadingPlugin() : this->renderWidget();
    if (!renderWidget)
        return nullptr;

    return renderWidget->widget();
}

bool HTMLPlugInElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == widthAttr || name == heightAttr || name == vspaceAttr || name == hspaceAttr || name == alignAttr)
        return true;
    return HTMLFrameOwnerElement::isPresentationAttribute(name);
}

void HTMLPlugInElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStyleProperties& style)
{
    if (name == widthAttr)
        addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    else if (name == heightAttr)
        addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    else if (name == vspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
    } else if (name == hspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
    } else if (name == alignAttr)
        applyAlignmentAttributeToStyle(value, style);
    else
        HTMLFrameOwnerElement::collectStyleForPresentationAttribute(name, value, style);
}

void HTMLPlugInElement::defaultEventHandler(Event* event)
{
    // Firefox seems to use a fake event listener to dispatch events to plug-in (tested with mouse events only).
    // This is observable via different order of events - in Firefox, event listeners specified in HTML attributes fires first, then an event
    // gets dispatched to plug-in, and only then other event listeners fire. Hopefully, this difference does not matter in practice.

    // FIXME: Mouse down and scroll events are passed down to plug-in via custom code in EventHandler; these code paths should be united.

    auto renderer = this->renderer();
    if (!is<RenderWidget>(renderer))
        return;

    if (is<RenderEmbeddedObject>(*renderer)) {
        if (downcast<RenderEmbeddedObject>(*renderer).isPluginUnavailable()) {
            downcast<RenderEmbeddedObject>(*renderer).handleUnavailablePluginIndicatorEvent(event);
            return;
        }

        if (is<RenderSnapshottedPlugIn>(*renderer) && displayState() < Restarting) {
            downcast<RenderSnapshottedPlugIn>(*renderer).handleEvent(event);
            HTMLFrameOwnerElement::defaultEventHandler(event);
            return;
        }

        if (displayState() < Playing)
            return;
    }

    // Don't keep the widget alive over the defaultEventHandler call, since that can do things like navigate.
    {
        RefPtr<Widget> widget = downcast<RenderWidget>(*renderer).widget();
        if (!widget)
            return;
        widget->handleEvent(event);
        if (event->defaultHandled())
            return;
    }
    HTMLFrameOwnerElement::defaultEventHandler(event);
}

bool HTMLPlugInElement::isKeyboardFocusable(KeyboardEvent*) const
{
    // FIXME: Why is this check needed?
    if (!document().page())
        return false;

    Widget* widget = pluginWidget();
    if (!is<PluginViewBase>(widget))
        return false;

    return downcast<PluginViewBase>(*widget).supportsKeyboardFocus();
}

bool HTMLPlugInElement::isPluginElement() const
{
    return true;
}

bool HTMLPlugInElement::isUserObservable() const
{
    // No widget - can't be anything to see or hear here.
    Widget* widget = pluginWidget(PluginLoadingPolicy::DoNotLoad);
    if (!is<PluginViewBase>(widget))
        return false;

    PluginViewBase& pluginView = downcast<PluginViewBase>(*widget);

    // If audio is playing (or might be) then the plugin is detectable.
    if (pluginView.audioHardwareActivity() != AudioHardwareActivityType::IsInactive)
        return true;

    // If the plugin is visible and not vanishingly small in either dimension it is detectable.
    return pluginView.isVisible() && pluginView.width() > 2 && pluginView.height() > 2;
}

bool HTMLPlugInElement::supportsFocus() const
{
    if (HTMLFrameOwnerElement::supportsFocus())
        return true;

    if (useFallbackContent() || !is<RenderEmbeddedObject>(renderer()))
        return false;
    return !downcast<RenderEmbeddedObject>(*renderer()).isPluginUnavailable();
}

RenderPtr<RenderElement> HTMLPlugInElement::createElementRenderer(Ref<RenderStyle>&& style, const RenderTreePosition&)
{
    return createRenderer<RenderEmbeddedObject>(*this, WTFMove(style));
}

void HTMLPlugInElement::swapRendererTimerFired()
{
    ASSERT(displayState() == DisplayingSnapshot);
    if (userAgentShadowRoot())
        return;

    // Create a shadow root, which will trigger the code to add a snapshot container
    // and reattach, thus making a new Renderer.
    ensureUserAgentShadowRoot();
}

void HTMLPlugInElement::setDisplayState(DisplayState state)
{
    m_displayState = state;

    if (state == DisplayingSnapshot && !m_swapRendererTimer.isActive())
        m_swapRendererTimer.startOneShot(0);
}

void HTMLPlugInElement::didAddUserAgentShadowRoot(ShadowRoot*)
{
}

bool HTMLPlugInElement::requestObject(const String&, const String&, const Vector<String>&, const Vector<String>&)
{
    return false;
}

}
