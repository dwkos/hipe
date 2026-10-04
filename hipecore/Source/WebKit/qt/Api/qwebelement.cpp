/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2021 Harrison Schaefer, Thomas Templeton,
                       Amesh Fernando, Scott Guiney
    Copyright (C) 2025 Daniel Kos
    Copyright (C) 2025-2026 General Development Systems

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include "qwebelement.h"

#include "qwebelement_p.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSParser.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleRule.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DOMSelection.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLElement.h"
#include "FocusController.h"
#include "QWebPageAdapter.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLNames.h"
#include "HTMLInputElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFormControlElement.h"
#include "TextControlInnerElements.h"
#include "HTMLMediaElement.h"
#include "HTMLImageElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLTemplateElement.h"
#include "MathMLNames.h"
#include "SVGNames.h"
#include "TypedElementDescendantIterator.h"
#include "CanvasRenderingContext2D.h"
#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "DOMPath.h"
#include "ImageData.h"
#include "TextMetrics.h"
#include <wtf/typedarrays/TypedArrayInlines.h>
#include "Color.h"
#if ENABLE(VIDEO)
#include "HTMLVideoElement.h"
#endif
#include "StyleProperties.h"
#include "StyleRule.h"
#include <QWebFrameAdapter.h>
#include "NodeList.h"
#include "RenderImage.h"
#include "RenderStyle.h"
#include "RenderWidget.h"
#include "StaticNodeList.h"
#include "StyleResolver.h"
#include "Text.h"
#include "VisibleSelection.h"
#include "FindOptions.h"
#include "Editor.h"
#include "DocumentMarkerController.h"
#include "NodeTraversal.h"
#include "LengthFunctions.h"
#include "RenderText.h"
#include "VisiblePosition.h"
#include "FontCascade.h"
#include "TextIterator.h"
#include "TextRun.h"
#include "markup.h"
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

#include "hipecoreeventlistener.h"
#if PLATFORM(X11)
#include "X11EmbedWidgetQt.h"
#endif
#include <wtf/HashMap.h>
#include <map>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h> 
#include <WebCore/Event.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/WheelEvent.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/Range.h>

#include <iostream>

#include <QPainter>
#include <QBuffer>
#include <QPdfWriter>
#include <QPageSize>

using namespace WebCore;

static HashMap<Element*, quint64>& hipeNumbers(); // element -> Hipe location number; see QWebLocationRegistry

class QWebElementPrivate {
public:
};

/*!
    \class QWebElement
    \since 4.6
    \brief The QWebElement class provides convenient access to DOM elements in
    a QWebFrame.
    \inmodule QtWebKit

    A QWebElement object allows easy access to the document model, represented
    by a tree-like structure of DOM elements. The root of the tree is called
    the document element and can be accessed using
    QWebFrame::documentElement().

    Specific elements can be accessed using findAll() and findFirst(). These
    elements are identified using CSS selectors. The code snippet below
    demonstrates the use of findAll().

    \snippet webkitsnippets/webelement/main.cpp FindAll

    The first list contains all \c span elements in the document. The second
    list contains \c span elements that are children of \c p, classified with
    \c intro.

    Using findFirst() is more efficient than calling findAll(), and extracting
    the first element only in the list returned.

    Alternatively you can traverse the document manually using firstChild() and
    nextSibling():

    \snippet webkitsnippets/webelement/main.cpp Traversing with QWebElement

    Individual elements can be inspected or changed using methods such as attribute()
    or setAttribute(). For examle, to capture the user's input in a text field for later
    use (auto-completion), a browser could do something like this:

    \snippet webkitsnippets/webelement/main.cpp autocomplete1

    When the same page is later revisited, the browser can fill in the text field automatically
    by modifying the value attribute of the input element:

    \snippet webkitsnippets/webelement/main.cpp autocomplete2

    Another use case is to emulate a click event on an element. The following
    code snippet demonstrates how to call the JavaScript DOM method click() of
    a submit button:

    \snippet webkitsnippets/webelement/main.cpp Calling a DOM element method

    The underlying content of QWebElement is explicitly shared. Creating a copy
    of a QWebElement does not create a copy of the content. Instead, both
    instances point to the same element.

    The contents of child elements can be converted to plain text with
    toPlainText(); to XHTML using toInnerXml(). To include the element's tag in
    the output, use toOuterXml().

    It is possible to replace the contents of child elements using
    setPlainText() and setInnerXml(). To replace the element itself and its
    contents, use setOuterXml().

    \section1 Examples

    The \l{DOM Traversal Example} shows one way to traverse documents in a running
    example.

    The \l{Simple Selector Example} can be used to experiment with the searching
    features of this class and provides sample code you can start working with.
*/

/*!
    Constructs a null web element.
*/
QWebElement::QWebElement()
    : d(0)
    , m_element(0)
{
}

/*!
    \internal
*/
QWebElement::QWebElement(WebCore::Element* domElement)
    : d(0)
    , m_element(domElement)
{
    if (m_element)
        m_element->ref();
}

/*!
    Constructs a copy of \a other.
*/
QWebElement::QWebElement(const QWebElement &other)
    : d(0)
    , m_element(other.m_element)
{
    if (m_element)
        m_element->ref();
}

/*!
    Assigns \a other to this element and returns a reference to this element.
*/
QWebElement &QWebElement::operator=(const QWebElement &other)
{
    // ### handle "d" assignment
    if (this != &other) {
        Element *otherElement = other.m_element;
        if (otherElement)
            otherElement->ref();
        if (m_element)
            m_element->deref();
        m_element = otherElement;
    }
    return *this;
}

/*!
    Destroys the element. However, the underlying DOM element is not destroyed.
*/
QWebElement::~QWebElement()
{
    delete d;
    if (m_element) {
        m_element->deref();
    }
}

bool QWebElement::operator==(const QWebElement& o) const
{
    return m_element == o.m_element;
}

bool QWebElement::operator!=(const QWebElement& o) const
{
    return m_element != o.m_element;
}

/*!
    Returns true if the element is a null element; otherwise returns false.
*/
bool QWebElement::isNull() const
{
    return !m_element;
}

/*!
    Returns a new list of child elements matching the given CSS selector
    \a selectorQuery. If there are no matching elements, an empty list is
    returned.

    \l{Standard CSS selector} syntax is used for the query.

    This method is equivalent to Element::querySelectorAll in the \l{DOM Selectors API}.

    \note This search is performed recursively.

    \sa findFirst()
*/
QWebElementCollection QWebElement::findAll(const QString &selectorQuery) const
{
    return QWebElementCollection(*this, selectorQuery);
}

/*!
    Returns the first child element that matches the given CSS selector
    \a selectorQuery.

    \l{Standard CSS selector} syntax is used for the query.

    This method is equivalent to Element::querySelector in the \l{DOM Selectors API}.

    \note This search is performed recursively.

    \sa findAll()
*/
QWebElement QWebElement::findFirst(const QString &selectorQuery) const
{
    if (!m_element)
        return QWebElement();
    ExceptionCode exception = 0; // ###
    return QWebElement(m_element->querySelector(selectorQuery, exception));
}

/*!
    Replaces the existing content of this element with \a text.

    This is equivalent to setting the HTML innerText property.

    \sa toPlainText()
*/
void QWebElement::setPlainText(const QString &text)
{
    if (!m_element || !m_element->isHTMLElement())
        return;
    ExceptionCode exception = 0;
    static_cast<HTMLElement*>(m_element)->setInnerText(text, exception);
}

/*!
    Returns the text between the start and the end tag of this
    element.

    This is equivalent to reading the HTML innerText property.

    The text is extracted from the rendered layout, so text that isn't
    displayed is left out: for example text with visibility:hidden, or text
    inside a zero-sized scroll container, such as one in a frame that has
    never been shown. Use toTextContent() to read the text regardless of
    rendering.

    \sa setPlainText(), toTextContent()
*/
QString QWebElement::toPlainText() const
{
    if (!m_element || !m_element->isHTMLElement())
        return QString();
    return static_cast<HTMLElement*>(m_element)->innerText();
}

/*!
    Returns the concatenated text of all text nodes inside this element.

    This is equivalent to reading the DOM textContent property. Unlike
    toPlainText(), it reads the document directly and doesn't depend on
    rendering, so it returns the same text whether or not the element is
    displayed. Line breaks are only those present in the text itself;
    elements such as <br> contribute nothing.

    \sa toPlainText()
*/
QString QWebElement::toTextContent() const
{
    if (!m_element)
        return QString();
    return m_element->textContent();
}

/*!
    Returns the text of this element as laid out, like toPlainText(), but
    including text that isn't currently visible.

    As with toPlainText(), block boundaries and <br> elements become line
    breaks, and elements with display:none (which aren't laid out) are left
    out. Unlike toPlainText(), text is included even when it is clipped away
    entirely (for example inside a zero-sized scroll container, such as one
    in a frame that has never been shown) or styled visibility:hidden.

    \sa toPlainText(), toTextContent()
*/
QString QWebElement::toLayoutText() const
{
    if (!m_element || !m_element->isHTMLElement())
        return QString();
    // Same as Element::innerText(), but with TextIteratorIgnoresStyleVisibility, which makes
    // TextIterator skip neither fully clipped nor visibility:hidden content.
    m_element->document().updateLayoutIgnorePendingStylesheets();
    if (!m_element->renderer())
        return m_element->textContent(true);
    return plainText(rangeOfContents(*m_element).ptr(), TextIteratorIgnoresStyleVisibility);
}

/*!
    Replaces the contents of this element as well as its own tag with
    \a markup. The string may contain HTML or XML tags, which is parsed and
    formatted before insertion into the document.

    \note This element's parent must be an (X)HTML element (a restriction of
    outerHTML itself, not of this method); use setInnerXml() on the parent
    instead if that isn't the case.

    \sa toOuterXml(), toInnerXml(), setInnerXml()
*/
void QWebElement::setOuterXml(const QString &markup)
{
    if (!m_element)
        return;

    ExceptionCode exception = 0;

    //Element::setOuterHTML() (generic, unlike HTMLElement's inherited version this used to
    //reach via an unnecessary static_cast gated by isHTMLElement()) still requires this
    //element's *parent* to be an HTMLElement - that's a WebCore-level restriction on
    //outerHTML itself, not something this method adds.
    m_element->setOuterHTML(markup, exception);
}

/*!
    Returns this element converted to XML, including the start and the end
    tags as well as its attributes.

    \note The format of the markup returned will obey the namespace of the
    document containing the element. This means the return value will obey XML
    formatting rules, such as self-closing tags, only if the document is
    'text/xhtml+xml'.

    \sa setOuterXml(), setInnerXml(), toInnerXml()
*/
QString QWebElement::toOuterXml() const
{
    if (!m_element)
        return QString();

    return m_element->outerHTML();
}

/*!
    Replaces the contents of this element with \a markup. The string may
    contain HTML or XML tags, which is parsed and formatted before insertion
    into the document.

    \sa toInnerXml(), toOuterXml(), setOuterXml()
*/
// Removes the hipe-loc attribute from every element in a parsed fragment, returning each element that had it with the
// attribute's value. Done before the fragment is inserted, so the attribute never reaches the document.
static Vector<std::pair<Ref<Element>, String>> takeHipeLocations(DocumentFragment& fragment)
{
    static NeverDestroyed<AtomicString> hipeLoc("hipe-loc", AtomicString::ConstructFromLiteral);
    Vector<std::pair<Ref<Element>, String>> carriers;
    for (auto& element : descendantsOfType<Element>(fragment)) {
        if (element.hasAttributes() && element.hasAttribute(hipeLoc))
            carriers.append(std::make_pair(Ref<Element>(element), element.getAttribute(hipeLoc).string()));
    }
    for (auto& carrier : carriers)
        carrier.first->removeAttribute(hipeLoc);
    return carriers;
}

void QWebElement::setInnerXml(const QString &markup)
{
    setInnerXml(markup, nullptr);
}

void QWebElement::setInnerXml(const QString &markup, QList<QPair<QWebElement, QString>>* hipeLocations)
{
    if (!m_element)
        return;

    // As Element::setInnerHTML(), which is generic and namespace-aware (see appendInside()'s comment): unlike
    // setOuterHTML() it has no HTMLElement-parent requirement, so this works uniformly for HTML and SVG (and any
    // other Element-derived) targets. The fragment is handled here so hipe-loc can be taken off before insertion.
    ExceptionCode exception = 0;
    RefPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(markup, m_element, AllowScriptingContent, exception);
    if (!fragment)
        return;
    for (auto& carrier : takeHipeLocations(*fragment)) {
        if (hipeLocations)
            hipeLocations->append(qMakePair(QWebElement(carrier.first.ptr()), QString(carrier.second)));
    }

    ContainerNode* container = m_element;
#if ENABLE(TEMPLATE_ELEMENT)
    if (is<HTMLTemplateElement>(*m_element))
        container = downcast<HTMLTemplateElement>(*m_element).content();
#endif
    replaceChildrenWithFragment(*container, fragment.releaseNonNull(), exception);
}

/*!
    Returns the XML content between the element's start and end tags.

    \note The format of the markup returned will obey the namespace of the
    document containing the element. This means the return value will obey XML
    formatting rules, such as self-closing tags, only if the document is
    'text/xhtml+xml'.

    \sa setInnerXml(), setOuterXml(), toOuterXml()
*/
QString QWebElement::toInnerXml() const
{
    if (!m_element)
        return QString();

    return m_element->innerHTML();
}

/*!
    Adds an attribute with the given \a name and \a value. If an attribute with
    the same name exists, its value is replaced by \a value.

    \sa attribute(), attributeNS(), setAttributeNS()
*/
void QWebElement::setAttribute(const QString &name, const QString &value)
{
    if (!m_element)
        return;

    const AtomicString& attributeName = name.toLower();

    //input elements need special handling to make them behave themselves
    if (m_element->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);

        if (attributeName == HTMLNames::valueAttr.localName()) {
            inputElement->setValue(value, DispatchInputAndChangeEvent);
            return;
        } else if (attributeName == HTMLNames::checkedAttr.localName()) {
            //Any non-empty value sets checked to true, except for "false".
            //Conventionally, a value of "checked" is used for true.
            bool checked = !value.isEmpty() && value.compare("false", Qt::CaseInsensitive);
            inputElement->setChecked(checked, DispatchInputAndChangeEvent);
            return;
        }
    }
    //textarea's value lives in the element itself, not in a value attribute.
    //Unlike input above, no input/change events are fired: a value set by the
    //client is not a user edit, and the client already knows about it.
    if (m_element->hasTagName(HTMLNames::textareaTag)
            && attributeName == HTMLNames::valueAttr.localName()) {
        static_cast<HTMLTextAreaElement*>(m_element)->setValue(value);
        return;
    }
    ExceptionCode exception = 0;
    m_element->setAttribute(attributeName, value, exception);
    if (exception)
    qWarning("QWebElement::setAttribute: Failed to set attribute '%s' to '%s'", 
             qPrintable(name), qPrintable(value));
}

void QWebElement::setAttributeBinaryData(const QString& name, const QString& mimeType, 
                                        const char* data, size_t length) 
{
    if (!m_element)
        return;
    
    // Check if this is an image element and name is "src"
    bool isImgSrc = (m_element->hasTagName(WebCore::HTMLNames::imgTag) && 
                     name == QLatin1String("src"));
    bool isMediaSrc = ((m_element->hasTagName(WebCore::HTMLNames::audioTag) || 
                       m_element->hasTagName(WebCore::HTMLNames::videoTag)) &&
                       name == QLatin1String("src"));
    
    if (isImgSrc) {
        // Direct binary data injection for image elements
        WebCore::HTMLImageElement* imgElement = static_cast<WebCore::HTMLImageElement*>(m_element);
        WTF::AtomicString webCoreMimeType(mimeType);

        // Use the specialized method to set image data directly
        imgElement->setBinaryImageData(
            reinterpret_cast<const uint8_t*>(data),
            length,
            webCoreMimeType
        );
        return;
    }
    // Raw pixel upload for a <canvas> -- the putImageData()-equivalent path (see
    // HTMLCanvasElement::setSrcRegion()'s doc comment for the full design rationale: a
    // hipe-specific sentinel mimetype rather than a real image format, since there's no
    // file here to decode, just a tightly-packed RGBA8888 buffer). Deliberately single-
    // shot only -- chunking this would need new state-machinery in the same class as the
    // img/media begin/append/finish bookkeeping for a use case (streaming a single
    // real-time frame across multiple calls) that doesn't actually exist.
    bool isCanvasRawPixels = (m_element->hasTagName(WebCore::HTMLNames::canvasTag) &&
                              name == QLatin1String("src") &&
                              mimeType == QLatin1String("image/x-raw-rgba"));
    if (isCanvasRawPixels) {
        auto& canvasElement = static_cast<WebCore::HTMLCanvasElement&>(*m_element);
        WebCore::CanvasRenderingContext* context = canvasElement.renderingContext();
        if (!context || !context->is2d())
            return; // no 2d context selected via useCanvasContext() yet: no-op
        auto* ctx = static_cast<WebCore::CanvasRenderingContext2D*>(context);

        WebCore::IntRect region = canvasElement.srcRegion();
        if (region.width() <= 0 || region.height() <= 0
            || length != static_cast<size_t>(region.width()) * region.height() * 4)
            return; // byte count doesn't match the primed (or default full-canvas) region: no-op

        // Uint8ClampedArray is aliased at global scope (see wtf/typedarrays/Uint8ClampedArray.h),
        // not inside namespace WebCore -- matches how CanvasRenderingContext2D.cpp itself
        // references it unqualified.
        RefPtr<Uint8ClampedArray> pixels = Uint8ClampedArray::create(
            reinterpret_cast<const unsigned char*>(data), static_cast<unsigned>(length));
        RefPtr<WebCore::ImageData> imageData = WebCore::ImageData::create(
            WebCore::IntSize(region.width(), region.height()), pixels);
        WebCore::ExceptionCode ec = 0;
        ctx->putImageData(imageData.get(), region.x(), region.y(), ec);
        return;
    }

    if (isMediaSrc) {
        // Direct binary data injection for audio/video elements, same shape as isImgSrc above.
        // Now that chunked loading is backed by a plain MediaSampleBuffer instead of
        // MediaSource/SourceBuffer (see MediaSampleBuffer.h), there is no fragmented-container
        // requirement forcing single-shot loads through base64 any more -- a single-shot load is
        // just begin+append+finish back to back, exactly like setBinaryImageData() composes the
        // three image calls above.
        beginBinaryMediaData(mimeType, static_cast<qint64>(length));
        appendBinaryMediaData(data, length);
        finishBinaryMediaData();
        return;
    }

    //For general cases, convert the binary data to base64 and set the attribute.
    //Webkit requires a "safe" encoding method like base64, due to lots of internal
    //string handling quirks.

    // Convert to base64 and create "data:" URL
    QByteArray binaryData(data, length);
    QByteArray base64Data = binaryData.toBase64();
    QString dataUrl = QString("data:%1;base64,%2").arg(mimeType, QString::fromLatin1(base64Data));
    
    // Set the attribute using the data URL
    setAttribute(name, dataUrl);
}

void QWebElement::beginBinaryImageData(const QString& mimeType)
{
    if (!m_element || !m_element->hasTagName(WebCore::HTMLNames::imgTag))
        return;
    WebCore::HTMLImageElement* imgElement = static_cast<WebCore::HTMLImageElement*>(m_element);
    imgElement->beginBinaryImageData(WTF::AtomicString(mimeType));
}

void QWebElement::appendBinaryImageData(const char* data, size_t length)
{
    if (!m_element || !m_element->hasTagName(WebCore::HTMLNames::imgTag))
        return;
    WebCore::HTMLImageElement* imgElement = static_cast<WebCore::HTMLImageElement*>(m_element);
    imgElement->appendBinaryImageData(reinterpret_cast<const uint8_t*>(data), length);
}

void QWebElement::finishBinaryImageData()
{
    if (!m_element || !m_element->hasTagName(WebCore::HTMLNames::imgTag))
        return;
    WebCore::HTMLImageElement* imgElement = static_cast<WebCore::HTMLImageElement*>(m_element);
    imgElement->finishBinaryImageData();
}

#if ENABLE(VIDEO) && USE(GSTREAMER)
static WebCore::HTMLMediaElement* toBinaryMediaElement(WebCore::Element* element)
{
    if (!element || (!element->hasTagName(WebCore::HTMLNames::audioTag) && !element->hasTagName(WebCore::HTMLNames::videoTag)))
        return 0;
    return static_cast<WebCore::HTMLMediaElement*>(element);
}
#endif

void QWebElement::beginBinaryMediaData(const QString& mimeType, qint64 expectedTotalSize)
{
#if ENABLE(VIDEO) && USE(GSTREAMER)
    if (WebCore::HTMLMediaElement* mediaElement = toBinaryMediaElement(m_element))
        mediaElement->beginBinaryMediaData(WTF::AtomicString(mimeType), expectedTotalSize);
#else
    Q_UNUSED(mimeType);
    Q_UNUSED(expectedTotalSize);
#endif
}

void QWebElement::appendBinaryMediaData(const char* data, size_t length)
{
#if ENABLE(VIDEO) && USE(GSTREAMER)
    if (WebCore::HTMLMediaElement* mediaElement = toBinaryMediaElement(m_element))
        mediaElement->appendBinaryMediaData(reinterpret_cast<const uint8_t*>(data), length);
#else
    Q_UNUSED(data);
    Q_UNUSED(length);
#endif
}

void QWebElement::finishBinaryMediaData()
{
#if ENABLE(VIDEO) && USE(GSTREAMER)
    if (WebCore::HTMLMediaElement* mediaElement = toBinaryMediaElement(m_element))
        mediaElement->finishBinaryMediaData();
#endif
}

/*!
    Adds an attribute with the given \a name in \a namespaceUri with \a value.
    If an attribute with the same name exists, its value is replaced by
    \a value.

    \sa attributeNS(), attribute(), setAttribute()
*/
void QWebElement::setAttributeNS(const QString &namespaceUri, const QString &name, const QString &value)
{
    if (!m_element)
        return;
    WebCore::ExceptionCode exception = 0;
    m_element->setAttributeNS(namespaceUri, name, value, exception);
}

/*!
    Returns the attribute with the given \a name. If the attribute does not
    exist, \a defaultValue is returned.

    \sa setAttribute(), setAttributeNS(), attributeNS()
*/
QString QWebElement::attribute(const QString &name, const QString &defaultValue) const
{
    if (!m_element)
        return QString();

    const AtomicString& attributeName = name.toLower();

    if (m_element->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* htmlElement = static_cast<HTMLInputElement*>(m_element);
        if (attributeName == HTMLNames::valueAttr.localName())
            return htmlElement->value();
        if (attributeName == HTMLNames::checkedAttr.localName())
            return htmlElement->checked() ? QString::fromLatin1("checked") : QString();        
    }    
    if (m_element->hasTagName(HTMLNames::textareaTag)
            && attributeName == HTMLNames::valueAttr.localName())
        return static_cast<HTMLTextAreaElement*>(m_element)->value();

    if (m_element->hasAttribute(attributeName))
        return m_element->getAttribute(attributeName);
    else
        return defaultValue;
}

/*!
    Returns the attribute with the given \a name in \a namespaceUri. If the
    attribute does not exist, \a defaultValue is returned.

    \sa setAttributeNS(), setAttribute(), attribute()
*/
QString QWebElement::attributeNS(const QString &namespaceUri, const QString &name, const QString &defaultValue) const
{
    if (!m_element)
        return QString();
    if (m_element->hasAttributeNS(namespaceUri, name))
        return m_element->getAttributeNS(namespaceUri, name);
    else
        return defaultValue;
}

/*!
    Returns true if this element has an attribute with the given \a name;
    otherwise returns false.

    \sa attribute(), setAttribute()
*/
bool QWebElement::hasAttribute(const QString &name) const
{
    if (!m_element)
        return false;
    return m_element->hasAttribute(name);
}

/*!
    Returns true if this element has an attribute with the given \a name, in
    \a namespaceUri; otherwise returns false.

    \sa attributeNS(), setAttributeNS()
*/
bool QWebElement::hasAttributeNS(const QString &namespaceUri, const QString &name) const
{
    if (!m_element)
        return false;
    return m_element->hasAttributeNS(namespaceUri, name);
}

/*!
    Removes the attribute with the given \a name from this element.

    \sa attribute(), setAttribute(), hasAttribute()
*/
void QWebElement::removeAttribute(const QString &name)
{
    if (!m_element)
        return;

    const AtomicString& attributeName = name.toLower();

    if (m_element->hasTagName(HTMLNames::inputTag)) {
        HTMLInputElement* htmlElement = static_cast<HTMLInputElement*>(m_element);
        if (attributeName == HTMLNames::valueAttr.localName()) {
            htmlElement->setValue(QString(), DispatchInputAndChangeEvent);
            return;
        } else if (attributeName == HTMLNames::checkedAttr.localName()) {
            htmlElement->setChecked(false, DispatchInputAndChangeEvent);
            return;
        }
    }
    //as with input above, removing value reverts to the default (the textarea's
    //own text content), like a form reset.
    if (m_element->hasTagName(HTMLNames::textareaTag)
            && attributeName == HTMLNames::valueAttr.localName()) {
        static_cast<HTMLFormControlElement*>(m_element)->reset();
        return;
    }
    m_element->removeAttribute(attributeName);
}

/*!
    Removes the attribute with the given \a name, in \a namespaceUri, from this
    element.

    \sa attributeNS(), setAttributeNS(), hasAttributeNS()
*/
void QWebElement::removeAttributeNS(const QString &namespaceUri, const QString &name)
{
    if (!m_element)
        return;
    m_element->removeAttributeNS(namespaceUri, name);
}

/*!
    Returns true if the element has any attributes defined; otherwise returns
    false;

    \sa attribute(), setAttribute()
*/
bool QWebElement::hasAttributes() const
{
    if (!m_element)
        return false;
    return m_element->hasAttributes();
}

/*!
    Return the list of attributes for the namespace given as \a namespaceUri.

    \sa attribute(), setAttribute()
*/
QStringList QWebElement::attributeNames(const QString& namespaceUri) const
{
    if (!m_element)
        return QStringList();

    QStringList attributeNameList;
    if (m_element->hasAttributes()) {
        const String namespaceUriString(namespaceUri); // convert QString -> String once
        const unsigned attrsCount = m_element->attributeCount();
        for (unsigned i = 0; i < attrsCount; ++i) {
            const Attribute& attribute = m_element->attributeAt(i);
            if (namespaceUriString == attribute.namespaceURI())
                attributeNameList.append(attribute.localName());
        }
    }
    return attributeNameList;
}

/*!
    Returns true if the element has keyboard input focus; otherwise, returns false

    \sa setFocus()
*/
bool QWebElement::hasFocus() const
{
    if (!m_element)
        return false;
    return m_element == m_element->document().focusedElement();
}

/*!
    Gives keyboard input focus to this element - original Qt function

    \sa hasFocus()
*/
void QWebElement::setFocus()
{
    if (!m_element) return;
    
    //Original (broken) Qt code:
    /*if (m_element->isFocusable())
        m_element->document().setFocusedElement(m_element);*/

    // Focusing an <iframe> gives focus to its content frame. If focus was last in a frame nested deeper
    // inside it (e.g. a client hosting its own child client), put it back there, so keys reach the
    // element that had focus. Look it up first: focusing the <iframe> updates the record.
    Frame* restoreFrame = nullptr;
    if (is<HTMLFrameOwnerElement>(*m_element) && m_element->document().page()) {
        Frame* contentFrame = downcast<HTMLFrameOwnerElement>(*m_element).contentFrame();
        if (QWebPageAdapter* adapter = QWebPageAdapter::kit(m_element->document().page()))
            restoreFrame = adapter->lastFocusedFrameWithin(contentFrame);
        if (restoreFrame == contentFrame)
            restoreFrame = nullptr;
    }

    //This produces correct behaviour for Hipe:
    m_element->focus(false, FocusDirectionNone);

    if (restoreFrame && m_element->document().page())
        m_element->document().page()->focusController().setFocusedFrame(restoreFrame);
}


// Caret/selection offsets are counted in characters (Unicode code points), not UTF-16 units: Hipe
// strings are UTF-8, so a client counts characters, never UTF-16 units. These convert between the two.
static int codePointCount(const String& text, unsigned utf16Length)
{
    int count = 0;
    for (unsigned i = 0; i < utf16Length && i < text.length(); ++i, ++count) {
        if (U16_IS_LEAD(text[i]) && i + 1 < utf16Length && i + 1 < text.length() && U16_IS_TRAIL(text[i + 1]))
            ++i;
    }
    return count;
}

static unsigned utf16IndexForCodePoints(const String& text, int codePoints)
{
    unsigned i = 0;
    for (int count = 0; count < codePoints && i < text.length(); ++count) {
        if (U16_IS_LEAD(text[i]) && i + 1 < text.length() && U16_IS_TRAIL(text[i + 1]))
            ++i;
        ++i;
    }
    return i;
}

// A negative offset counts from the end: -1 is after the last character, -2 before it, and so on.
// The result is clamped to 0..length.
static int resolveOffset(int offset, int length)
{
    if (offset < 0)
        offset = length + 1 + offset;
    return std::max(0, std::min(offset, length));
}

// For elements other than <input>/<textarea>, offsets index the element's text as laid out: the same text
// GET_CONTENT mode 2 returns (toLayoutText()), in which <br> and block boundaries are line breaks. Text that
// is clipped away or visibility:hidden still counts, so this also works in frames that aren't displayed.
static const TextIteratorBehavior caretTextBehavior = TextIteratorIgnoresStyleVisibility;

// Maps a UTF-16 offset into the element's layout text to a DOM position.
static Position positionForLayoutOffset(Element& element, int offset)
{
    Ref<Range> contents = rangeOfContents(element);
    CharacterIterator it(contents.get(), caretTextBehavior);
    it.advance(offset);
    if (it.atEnd())
        return Position(&element, element.countChildNodes(), Position::PositionIsOffsetInAnchor);
    Ref<Range> range = it.range();
    return Position(&range->startContainer(), range->startOffset(), Position::PositionIsOffsetInAnchor);
}

// Maps a DOM position inside the element to a character offset into the element's layout text.
static int layoutOffsetForPosition(Element& element, Node& container, int offset)
{
    ExceptionCode exception = 0;
    Ref<Range> prefix = Range::create(element.document());
    prefix->setStart(&element, 0, exception);
    if (!exception)
        prefix->setEnd(&container, offset, exception);
    if (exception)
        return 0;
    String text = plainText(prefix.ptr(), caretTextBehavior);
    return codePointCount(text, text.length());
}

// Fast path for reading offsets without layout. When an element holds only text nodes and its whitespace is
// preserved (a <pre>-style editor), its layout text is exactly its raw text, so offsets can be counted in the DOM.
// The layout path must first bring layout up to date, and after every edit it is out of date: for a large
// document that costs far more than the count itself (about 1 s at 50k lines on a Raspberry Pi).
static bool layoutTextIsRawText(Element& element)
{
    for (Node* child = element.firstChild(); child; child = child->nextSibling()) {
        if (!child->isTextNode())
            return false;
    }
    element.document().updateStyleIfNeeded();
    RenderStyle* style = element.computedStyle();
    return style && style->preserveNewline() && !style->collapseWhiteSpace() && style->textTransform() == TTNONE;
}

// Maps a DOM position (a text child of element, or element itself with a child index) to a character offset,
// for an element that satisfies layoutTextIsRawText().
static int rawOffsetForPosition(Element& element, Node& container, int offset)
{
    int count = 0;
    int index = 0;
    for (Node* child = element.firstChild(); child; child = child->nextSibling(), ++index) {
        const String& data = downcast<Text>(*child).data();
        if (child == &container)
            return count + codePointCount(data, std::max(offset, 0));
        if (&container == &element && index >= offset)
            break;
        count += codePointCount(data, data.length());
    }
    return count;
}

static bool isInsideElement(Node* node, Element& element)
{
    return node && (node == &element || node->isDescendantOf(&element));
}

static HTMLTextFormControlElement* textFormControl(Element* element)
{
    if (!is<HTMLTextFormControlElement>(*element))
        return nullptr;
    if (is<HTMLInputElement>(*element) && !downcast<HTMLInputElement>(*element).canHaveSelection())
        return nullptr;
    return downcast<HTMLTextFormControlElement>(element);
}

/*!
    Selects the text between character offsets \a anchor and \a focus of this
    element, or places the caret there if they are equal.

    Offsets count characters (Unicode code points). For <input> and <textarea>
    they index the element's value; for other elements they index the text as
    laid out, as returned by toLayoutText(), in which <br> and the boundaries
    of block elements count as line breaks. A negative offset counts from the
    end: -1 is after the last character. Offsets are clamped to the text.

    The selection runs from \a anchor to \a focus, so a \a focus before
    \a anchor makes a backward selection. If \a scrollIntoView is true, the
    selection is scrolled into view.

    \sa getSelectionRange()
*/
void QWebElement::setSelectionRange(int anchor, int focus, bool scrollIntoView)
{
    if (!m_element)
        return;
    Document& document = m_element->document();
    document.updateLayoutIgnorePendingStylesheets();

    if (HTMLTextFormControlElement* control = textFormControl(m_element)) {
        String value = control->value();
        int length = codePointCount(value, value.length());
        anchor = utf16IndexForCodePoints(value, resolveOffset(anchor, length));
        focus = utf16IndexForCodePoints(value, resolveOffset(focus, length));
        control->setSelectionRange(std::min(anchor, focus), std::max(anchor, focus),
            focus < anchor ? SelectionHasBackwardDirection : SelectionHasForwardDirection);
        if (scrollIntoView) {
            if (document.focusedElement() == m_element && document.frame())
                document.frame()->selection().revealSelection(ScrollAlignment::alignToEdgeIfNeeded, RevealExtent);
            else
                m_element->scrollIntoViewIfNeeded(false);
        }
        return;
    }

    RefPtr<DOMSelection> selection = document.getSelection();
    if (!selection)
        return;

    Ref<Range> contents = rangeOfContents(*m_element);
    String text = plainText(contents.ptr(), caretTextBehavior);
    int length = codePointCount(text, text.length());
    Position anchorPosition = positionForLayoutOffset(*m_element, utf16IndexForCodePoints(text, resolveOffset(anchor, length)));
    Position focusPosition = positionForLayoutOffset(*m_element, utf16IndexForCodePoints(text, resolveOffset(focus, length)));

    ExceptionCode exception = 0;
    selection->setBaseAndExtent(anchorPosition.containerNode(), anchorPosition.offsetInContainerNode(),
        focusPosition.containerNode(), focusPosition.offsetInContainerNode(), exception);
    if (exception)
        return;

    if (scrollIntoView && document.frame())
        document.frame()->selection().revealSelection(ScrollAlignment::alignToEdgeIfNeeded, RevealExtent);
}

/*!
    Gets the current selection within this element as character offsets, in
    the same units as setSelectionRange(): \a anchor is where the selection
    started and \a focus where it ends (where the caret is), so \a focus is
    less than \a anchor for a backward selection, and equal to it for a plain
    caret.

    A caret with no text selected is reported with \a anchor equal to
    \a focus. Returns false, leaving \a anchor and \a focus unchanged, if the
    element contains neither the caret nor selected text: for elements other
    than <input> and <textarea>, that is when the document's selection (which
    includes its caret) is elsewhere, or extends beyond this element. <input>
    and <textarea> keep their own selection, so they always report one.

    \sa setSelectionRange()
*/
bool QWebElement::getSelectionRange(int* anchor, int* focus)
{
    if (!m_element)
        return false;
    Document& document = m_element->document();

    if (HTMLTextFormControlElement* control = textFormControl(m_element)) {
        document.updateLayoutIgnorePendingStylesheets();
        String value = control->value();
        int start = codePointCount(value, control->selectionStart());
        int end = codePointCount(value, control->selectionEnd());
        bool backward = control->selectionDirection() == "backward";
        if (anchor)
            *anchor = backward ? end : start;
        if (focus)
            *focus = backward ? start : end;
        return true;
    }

    RefPtr<DOMSelection> selection = document.getSelection();
    if (!selection || !selection->rangeCount())
        return false;
    Node* anchorNode = selection->anchorNode();
    Node* focusNode = selection->focusNode();
    if (!isInsideElement(anchorNode, *m_element) || !isInsideElement(focusNode, *m_element))
        return false;

    if (layoutTextIsRawText(*m_element)) {
        if (anchor)
            *anchor = rawOffsetForPosition(*m_element, *anchorNode, selection->anchorOffset());
        if (focus)
            *focus = rawOffsetForPosition(*m_element, *focusNode, selection->focusOffset());
        return true;
    }

    document.updateLayoutIgnorePendingStylesheets();
    if (anchor)
        *anchor = layoutOffsetForPosition(*m_element, *anchorNode, selection->anchorOffset());
    if (focus)
        *focus = layoutOffsetForPosition(*m_element, *focusNode, selection->focusOffset());
    return true;
}

/*!
    Measures \a text as if it were this element's content: laid out in the
    element's computed font (including letter and word spacing and tab stops),
    with its white-space and text-transform rules applied, but without wrapping.
    Each newline in \a text starts a new line when the element preserves
    newlines. Nothing is added to the document.

    If \a fontSize is greater than 0, the text is measured at that font size
    (in CSS pixels) instead of the element's own, with the line height scaled
    accordingly.

    Returns { width, height, ascent, descent } in CSS pixels: the width of the
    widest line, the height of all the lines (each one line-height tall), and
    the font's ascent and descent. Returns an empty vector for a null element.

    \sa rangeGeometry()
*/
QVector<qreal> QWebElement::measureText(const QString& text, qreal fontSize) const
{
    if (!m_element)
        return QVector<qreal>();
    m_element->document().updateStyleIfNeeded();
    RenderStyle* style = m_element->computedStyle();
    if (!style)
        return QVector<qreal>();

    FontCascade font = style->fontCascade();
    float size = style->computedFontSize();
    if (fontSize > 0) {
        FontCascadeDescription description = font.fontDescription();
        description.setSpecifiedSize(fontSize);
        description.setComputedSize(fontSize);
        font = FontCascade(description, font.letterSpacing(), font.wordSpacing());
        font.update(style->fontCascade().fontSelector());
        size = fontSize;
    }
    const FontMetrics& metrics = font.fontMetrics();
    const Length& lineHeightLength = style->lineHeight();
    float lineHeight = lineHeightLength.isNegative() ? metrics.floatLineSpacing()
        : lineHeightLength.isPercentOrCalculated() ? floatValueForLength(lineHeightLength, size)
        : lineHeightLength.value();

    String content = text;
    if (style->textTransform() != TTNONE)
        applyTextTransform(*style, content, ' ');

    // Apply the element's white-space rules: newlines are kept only if it preserves them, and with
    // collapsing, runs of spaces become one and spaces at the start and end of a line are dropped.
    bool preserveNewlines = style->preserveNewline();
    bool collapse = style->collapseWhiteSpace();
    Vector<String> lines;
    StringBuilder line;
    bool pendingSpace = false;
    for (unsigned i = 0; i <= content.length(); ++i) {
        UChar c = i < content.length() ? content[i] : '\n';
        bool atEnd = i == content.length();
        if (c == '\r')
            continue;
        if (c == '\n' && (preserveNewlines || atEnd)) {
            lines.append(line.toString());
            line.clear();
            pendingSpace = false;
            continue;
        }
        if (collapse && (c == ' ' || c == '\t' || c == '\n')) {
            pendingSpace = !line.isEmpty();
            continue;
        }
        if (pendingSpace) {
            line.append(' ');
            pendingSpace = false;
        }
        line.append(c == '\n' ? ' ' : c);
    }

    float width = 0;
    for (const String& l : lines) {
        TextRun run(l);
        run.setTabSize(!collapse, style->tabSize());
        width = std::max(width, font.width(run));
    }
    return QVector<qreal>() << width << lineHeight * lines.size() << metrics.floatAscent() << metrics.floatDescent();
}

/*!
    Returns the rectangles occupied by the text between character offsets
    \a start and \a end of this element, one per line, relative to the
    element's top-left corner as currently displayed (so they move when the
    element or a container scrolls). Offsets are in the units of
    setSelectionRange(), and negative ones count from the end.

    If \a start equals \a end, returns the single rectangle of a caret at that
    offset: zero width, as tall as its line.

    Lines the range covers without any text on them (e.g. empty lines) have no
    rectangle. Returns an empty list if the element isn't displayed.

    \sa measureText(), setSelectionRange()
*/
QList<QRectF> QWebElement::rangeGeometry(int start, int end) const
{
    QList<QRectF> result;
    if (!m_element)
        return result;
    Document& document = m_element->document();
    document.updateLayoutIgnorePendingStylesheets();
    RenderObject* renderer = m_element->renderer();
    if (!renderer)
        return result;

    Position startPosition, endPosition;
    if (HTMLTextFormControlElement* control = textFormControl(m_element)) {
        String value = control->value();
        int length = codePointCount(value, value.length());
        startPosition = control->visiblePositionForIndex(utf16IndexForCodePoints(value, resolveOffset(start, length))).deepEquivalent();
        endPosition = control->visiblePositionForIndex(utf16IndexForCodePoints(value, resolveOffset(end, length))).deepEquivalent();
    } else {
        Ref<Range> contents = rangeOfContents(*m_element);
        String text = plainText(contents.ptr(), caretTextBehavior);
        int length = codePointCount(text, text.length());
        int a = resolveOffset(start, length), b = resolveOffset(end, length);
        startPosition = positionForLayoutOffset(*m_element, utf16IndexForCodePoints(text, std::min(a, b)));
        endPosition = positionForLayoutOffset(*m_element, utf16IndexForCodePoints(text, std::max(a, b)));
    }

    FloatPoint origin = renderer->localToAbsolute(FloatPoint());
    Vector<IntRect> rects;
    if (startPosition == endPosition) {
        IntRect caret = VisiblePosition(startPosition).absoluteCaretBounds();
        if (!caret.isEmpty() || caret.height())
            result.append(QRectF(caret.x() - origin.x(), caret.y() - origin.y(), 0, caret.height()));
        return result;
    }

    Ref<Range> range = Range::create(document, startPosition, endPosition);
    range->absoluteTextRects(rects, false);

    // One rectangle per line: merge rectangles that overlap vertically by at least half the smaller one
    // (text in different spans or fonts on the same line).
    std::sort(rects.begin(), rects.end(), [](const IntRect& a, const IntRect& b) { return a.y() < b.y() || (a.y() == b.y() && a.x() < b.x()); });
    Vector<IntRect> lines;
    for (const IntRect& r : rects) {
        if (r.isEmpty())
            continue;
        if (!lines.isEmpty()) {
            IntRect& last = lines.last();
            int overlap = std::min(last.maxY(), r.maxY()) - std::max(last.y(), r.y());
            if (overlap * 2 >= std::min(last.height(), r.height())) {
                last.unite(r);
                continue;
            }
        }
        lines.append(r);
    }
    for (const IntRect& r : lines)
        result.append(QRectF(r.x() - origin.x(), r.y() - origin.y(), r.width(), r.height()));
    return result;
}

// For findText(): the element's content as text ranges in document order. A frame owned by an <iframe> in the
// element is searched in place: the range stops before the <iframe>, the frame's document follows (recursively),
// and the next range starts after it. Text that can't be selected (-webkit-user-select: none, e.g. line numbers or
// a layer duplicating other text) is left out the same way, since a match there couldn't be selected.
static void collectFindSegments(Element& root, Vector<RefPtr<Range>>& segments)
{
    Document& document = root.document();
    Position segmentStart = firstPositionInNode(&root);
    for (Node* node = NodeTraversal::next(root, &root); node; ) {
        if (is<Text>(*node) && node->renderer() && node->renderer()->style().userSelect() == SELECT_NONE) {
            if (segmentStart != positionBeforeNode(node))
                segments.append(Range::create(document, segmentStart, positionBeforeNode(node)));
            segmentStart = positionAfterNode(node);
            node = NodeTraversal::nextSkippingChildren(*node, &root);
            continue;
        }
        if (is<HTMLFrameOwnerElement>(*node)) {
            Document* content = downcast<HTMLFrameOwnerElement>(*node).contentDocument();
            if (Element* contentRoot = content ? content->documentElement() : nullptr) {
                segments.append(Range::create(document, segmentStart, positionBeforeNode(node)));
                collectFindSegments(*contentRoot, segments);
                segmentStart = positionAfterNode(node);
            }
            node = NodeTraversal::nextSkippingChildren(*node, &root);
            continue;
        }
        node = NodeTraversal::next(*node, &root);
    }
    segments.append(Range::create(document, segmentStart, lastPositionInNode(&root)));
}

/*!
    Searches for \a text in this element's content (or, for an <iframe>, the
    content of its frame), including frames nested inside it, in the style of
    a browser's "find in page".

    All matches are highlighted, and the next match after the current selection
    or text cursor within the element (the previous one if \a backwards) is
    selected and scrolled into view, wrapping round at the end. Keyboard focus
    doesn't move. At most \a limit matches are found (0 for no limit). An empty
    \a text removes the highlighting.

    Returns { count, index, wrapped, capped }: the number of matches found, the
    position of the selected match among them (from 1; 0 if none), whether the
    search wrapped round, and whether it stopped at \a limit.
*/
QVector<int> QWebElement::findText(const QString& text, bool backwards, bool caseSensitive, bool wholeWords, int limit)
{
    QVector<int> result(4, 0);
    if (!m_element)
        return result;

    Element* root = m_element;
    if (is<HTMLFrameOwnerElement>(*m_element)) {
        Document* content = downcast<HTMLFrameOwnerElement>(*m_element).contentDocument();
        if (!content || !content->documentElement())
            return result;
        root = content->documentElement();
    }
    root->document().updateLayoutIgnorePendingStylesheets();

    Vector<RefPtr<Range>> segments;
    collectFindSegments(*root, segments);

    // Replace any previous highlighting in the element (only there: other parts of the page keep theirs). The text
    // inside form controls (which the search enters) isn't part of the ranges, so it's cleared control by control.
    for (auto& segment : segments) {
        DocumentMarkerController& markers = segment->ownerDocument().markers();
        markers.removeMarkers(segment.get(), DocumentMarker::TextMatch, DocumentMarkerController::RemovePartiallyOverlappingMarker);
        for (Node* node = segment->firstNode(); node && node != segment->pastLastNode(); node = NodeTraversal::next(*node)) {
            if (!is<HTMLTextFormControlElement>(*node))
                continue;
            if (TextControlInnerTextElement* innerText = downcast<HTMLTextFormControlElement>(*node).innerTextElement()) {
                for (Node* inner = innerText; inner; inner = NodeTraversal::next(*inner, innerText))
                    markers.removeMarkers(inner, DocumentMarker::TextMatch);
            }
        }
    }
    if (text.isEmpty())
        return result;

    FindOptions options = (caseSensitive ? 0 : CaseInsensitive) | (wholeWords ? AtWordStarts | AtWordEnds : 0);
    Vector<RefPtr<Range>> matches;
    Vector<size_t> matchSegment;
    for (size_t i = 0; i < segments.size(); ++i) {
        Document& document = segments[i]->ownerDocument();
        Frame* frame = document.frame();
        if (!frame)
            continue;
        document.updateLayoutIgnorePendingStylesheets();
        Vector<RefPtr<Range>> found;
        unsigned remaining = limit > 0 ? limit - matches.size() : 0;
        Ref<Range> searchRange = segments[i]->cloneRange(); // countMatchesForText() moves its start along
        frame->editor().countMatchesForText(text, searchRange.ptr(), options, remaining, true, &found);
        frame->editor().setMarkedTextMatchesAreHighlighted(true);
        for (auto& match : found) {
            matches.append(match);
            matchSegment.append(i);
        }
        if (limit > 0 && (int)matches.size() >= limit) {
            result[3] = 1;
            break;
        }
    }
    result[0] = matches.size();
    if (matches.isEmpty())
        return result;

    // Where the user is: the selection (or text cursor) inside the element, preferably the focused frame's.
    RefPtr<Range> reference;
    size_t referenceSegment = 0;
    auto locate = [&](Frame* frame) {
        if (!frame || frame->selection().isNone())
            return false;
        RefPtr<Range> selected = frame->selection().selection().firstRange();
        if (!selected)
            return false;
        for (size_t i = 0; i < segments.size(); ++i) {
            if (&segments[i]->ownerDocument() != frame->document())
                continue;
            ExceptionCode ec = 0;
            if (segments[i]->isPointInRange(&selected->startContainer(), selected->startOffset(), ec) && !ec) {
                reference = selected;
                referenceSegment = i;
                return true;
            }
        }
        return false;
    };
    if (Page* page = root->document().page()) {
        if (!locate(page->focusController().focusedFrame())) {
            for (auto& segment : segments) {
                if (locate(segment->ownerDocument().frame()))
                    break;
            }
        }
    }

    // The next match after the reference (or the previous one before it), wrapping round.
    auto comesAfter = [&](size_t k) { // match k starts at or after the reference's end
        if (matchSegment[k] != referenceSegment)
            return matchSegment[k] > referenceSegment;
        ExceptionCode ec = 0;
        return Range::compareBoundaryPoints(&matches[k]->startContainer(), matches[k]->startOffset(),
            &reference->endContainer(), reference->endOffset(), ec) >= 0;
    };
    auto comesBefore = [&](size_t k) { // match k ends at or before the reference's start
        if (matchSegment[k] != referenceSegment)
            return matchSegment[k] < referenceSegment;
        ExceptionCode ec = 0;
        return Range::compareBoundaryPoints(&matches[k]->endContainer(), matches[k]->endOffset(),
            &reference->startContainer(), reference->startOffset(), ec) <= 0;
    };
    long chosen = -1;
    if (reference) {
        if (!backwards) {
            for (size_t k = 0; k < matches.size() && chosen < 0; ++k)
                if (comesAfter(k)) chosen = k;
        } else {
            for (size_t k = matches.size(); k-- > 0 && chosen < 0; )
                if (comesBefore(k)) chosen = k;
        }
        if (chosen < 0)
            result[2] = 1;
    }
    if (chosen < 0)
        chosen = backwards ? (long)matches.size() - 1 : 0;

    // One selection in the element, so the next search carries on from this match (other frames don't keep theirs).
    Frame* matchFrame = matches[chosen]->ownerDocument().frame();
    for (auto& segment : segments) {
        Frame* frame = segment->ownerDocument().frame();
        if (frame && frame != matchFrame && !frame->selection().isNone()) {
            RefPtr<Range> selected = frame->selection().selection().firstRange();
            ExceptionCode ec = 0;
            if (selected && segment->isPointInRange(&selected->startContainer(), selected->startOffset(), ec) && !ec)
                frame->selection().clear();
        }
    }
    matchFrame->selection().setSelection(VisibleSelection(*matches[chosen]), FrameSelection::defaultSetSelectionOptions() | FrameSelection::DoNotSetFocus);
    matchFrame->selection().revealSelection(ScrollAlignment::alignCenterIfNeeded);
    result[1] = chosen + 1;
    return result;
}

/*!
    Returns the start of the selection within this element (the lower of its
    anchor and focus offsets), or -1 if the element contains neither the
    caret nor selected text.

    \sa getSelectionRange(), selectionEnd()
*/
int QWebElement::selectionStart()
{
    int anchor, focus;
    if (!getSelectionRange(&anchor, &focus))
        return -1;
    return std::min(anchor, focus);
}

/*!
    Returns the end of the selection within this element (the higher of its
    anchor and focus offsets), or -1 if the element contains neither the
    caret nor selected text.

    \sa getSelectionRange(), selectionStart()
*/
int QWebElement::selectionEnd()
{
    int anchor, focus;
    if (!getSelectionRange(&anchor, &focus))
        return -1;
    return std::max(anchor, focus);
}

QString QWebElement::getSelection() {
    if (!m_element) return QString();

    return m_element->document().getSelection()->toString();
}

// Select this whole element (DOM Range::selectNode semantics: the selection
// boundary points sit outside the element, in its parent). Contrast with
// setSelectionRange(), which selects a character span of the element's text.
void QWebElement::select()
{
    if (!m_element)
        return;

    Document& document = m_element->document();
    RefPtr<DOMSelection> selection = document.getSelection();
    if (!selection)
        return;

    RefPtr<Range> range = document.createRange();
    if (!range)
        return;

    ExceptionCode exception = 0;
    range->selectNode(m_element, exception);
    if (exception)
        return;

    selection->removeAllRanges();
    selection->addRange(range.get());
}

/*!
    Gets the left offset of this element.

    \sa offsetLeft()
*/
double QWebElement::offsetLeft() const
{
    if (!m_element)
        return 0;
    return m_element->offsetLeft();
}

/*!
    Gets the top offset of this element.

    \sa offsetTop()
*/
double QWebElement::offsetTop() const
{
    if (!m_element)
        return 0;
    return m_element->offsetTop();
}

/*!
    Gets the width of this element.

    \sa offsetWidth()
*/
double QWebElement::offsetWidth() const
{
    if (!m_element)
        return 0;
    return m_element->offsetWidth();
}

/*!
    Gets the height of this element.

    \sa offsetHeight()
*/
double QWebElement::offsetHeight() const
{
    if (!m_element)
        return 0;
    return m_element->offsetHeight();
}

double QWebElement::clientWidth() const
{
    if (!m_element)
        return 0;
    return m_element->clientWidth();
}

double QWebElement::clientHeight() const
{
    if (!m_element)
        return 0;
    return m_element->clientHeight();
}

/*!
    Gets the left scroll position of this element.

    \sa setScrollLeft()
*/
int QWebElement::scrollLeft() const
{
    if (!m_element)
        return 0;
    return m_element->scrollLeft();
}

/*!
    Sets the left scroll position of this element.

    \sa scrollLeft()
*/
void QWebElement::setScrollLeft(int newLeft)
{
    if (!m_element)
        return;
    m_element->setScrollLeft(newLeft);
}

/*!
    Sets the top scroll position of this element.

    \sa scrollTop()
*/
void QWebElement::setScrollTop(int newTop)
{
    if (!m_element)
        return;
    m_element->setScrollTop(newTop);
}

/*!
    Gets the top scroll position of this element.

    \sa setScrollTop()
*/
int QWebElement::scrollTop() const
{
    if (!m_element)
        return 0;
    return m_element->scrollTop();
}

/*
    Gets the scroll width of this element.
*/
int QWebElement::scrollWidth() const
{
    if (!m_element)
        return 0;
    return m_element->scrollWidth();
}

/*
    Gets the scroll height of this element.
*/
int QWebElement::scrollHeight() const
{
    if (!m_element)
        return 0;
    return m_element->scrollHeight();
}

/*!
    Checks whether or not the underlying WebCore element is a HTMLMediaElement
*/
WebCore::HTMLMediaElement* QWebElement::isMediaElement()    ///is exposing webcore the best approach?
{
    if (!m_element) return nullptr;

    // Check if this element is an audio or video element before casting
    if (m_element->hasTagName(HTMLNames::audioTag) || 
        m_element->hasTagName(HTMLNames::videoTag)) {
        return static_cast<HTMLMediaElement*>(m_element);
    }
    
    return nullptr;
}

/*
    Returns the current position we are at in the video
*/
std::string QWebElement::getMediaPositionString(WebCore::HTMLMediaElement* mediaElement)
{
    if (!mediaElement) return std::string();

    return std::to_string(mediaElement->currentTime()) + "," + std::to_string(mediaElement->duration());
}
/*
    sets the current position in the video to time
*/
void QWebElement::setCurrentTime(WebCore::HTMLMediaElement* mediaElement, double time)
{
    if (!mediaElement) return;

    mediaElement->setCurrentTime(time);
}
/*
    Returns the playback rate of the video
*/
std::string QWebElement::getPlaybackRate(WebCore::HTMLMediaElement* mediaElement)
{
    if (!mediaElement) return std::string();

    return std::to_string(mediaElement->playbackRate());
}
/*
    sets the playback rate of the video
*/
void QWebElement::setPlaybackRate(WebCore::HTMLMediaElement* mediaElement, double rate)
{
    if (!mediaElement) return;

    mediaElement->setPlaybackRate(rate);
}
/*
    Returns true if the video is playing, false if it is paused or otherwise.
*/
bool QWebElement::isMediaPlaying(WebCore::HTMLMediaElement* mediaElement)
{
    if (!mediaElement) return false;
    
    return !mediaElement->paused();
}

/*
   sets the video playing state, playing if flag is 1 and paused if flag is 0
*/
void QWebElement::setMediaPlaying(WebCore::HTMLMediaElement* mediaElement, std::string flag)
{
    if (!mediaElement) return;

    if (flag == "1")
    {
        mediaElement->play();
    }
    else
    {
        mediaElement->pause();
    }
}

/*
    Returns the volume of the video/audio, from 0-0.99
*/
std::string QWebElement::getVolume(WebCore::HTMLMediaElement* mediaElement)
{
    if (!mediaElement) return std::string();

    return std::to_string(mediaElement->volume());
}

/*
    sets the volume, volume must be in range 0-0.99
*/
void QWebElement::setVolume(WebCore::HTMLMediaElement* mediaElement, double volume)
{
    if (!mediaElement) return;

    WebCore::ExceptionCode ec = NULL;
    mediaElement->setVolume(volume, ec);
}


// Modifier keys held during an event, as reported in event details:
// 1=Shift, 2=Alt, 4=Ctrl, 8=Meta.
static int modifierMask(const UIEventWithKeyState& event)
{
    return (event.shiftKey() ? 1 : 0) | (event.altKey() ? 2 : 0)
        | (event.ctrlKey() ? 4 : 0) | (event.metaKey() ? 8 : 0);
}

void QWebElement::requestEvent(const QString& eventName, void* usrPtr, uint64_t usrVal1, uint64_t usrVal2,
                                HipeCoreEventCallback callbackFn, bool preventDefault, bool usrVal1IsHipeLocation) {
    if (!m_element) return;

    // Create a callback that bridges between WebCore::Event and our API
    auto callback = [callbackFn, eventName, usrPtr, usrVal1, usrVal2, preventDefault, usrVal1IsHipeLocation]
                                            (WebCore::Event* event) {

        if(preventDefault) event->preventDefault();
        //override default webkit behaviour if requested.

        uint64_t location = usrVal1;
        if (usrVal1IsHipeLocation) {
            // The listener's element's number now, not when the event was requested: a freed number reports nothing,
            // and a reused one only its own element's events.
            Node* node = event->currentTarget() ? event->currentTarget()->toNode() : nullptr;
            auto entry = (node && is<Element>(*node)) ? hipeNumbers().find(&downcast<Element>(*node)) : hipeNumbers().end();
            if (entry == hipeNumbers().end())
                return;
            location = entry->value;
        }

        QString details;

        if (event->isWheelEvent()) {
            // WheelEvent is itself a MouseEvent subclass, so this must be
            // checked before isMouseEvent() below or it never gets here.
            auto* wheelEvent = static_cast<WheelEvent*>(event);

            // Format: deltaX,deltaY,deltaMode,modifiers (deltaMode: 0=pixel, 1=line, 2=page)
            details = QString("%1,%2,%3,%4")
                .arg(wheelEvent->deltaX())
                .arg(wheelEvent->deltaY())
                .arg(wheelEvent->deltaMode())
                .arg(modifierMask(*wheelEvent));
        } else if (event->isMouseEvent()) {
            auto* mouseEvent = static_cast<MouseEvent*>(event);
            auto* targetElement = static_cast<Element*>(mouseEvent->target());

            //click only returns button value and modifiers, other mouse events produce a
            //comma-separated list of button, absolute and relative coordinates, and modifiers.
            //contextmenu is the DOM event for a right-click.
            if(eventName == "click") {
                // Format: button,modifiers
                details = QString("%1,%2")
                    .arg(mouseEvent->button()+1)
                    .arg(modifierMask(*mouseEvent));
            } else {
                // Format: which,pageX,pageY,offsetX,offsetY,modifiers
                details = QString("%1,%2,%3,%4,%5,%6")
                    .arg(mouseEvent->button()+1)
                    .arg(mouseEvent->pageX())
                    .arg(mouseEvent->pageY())
                    .arg(mouseEvent->pageX() - targetElement->offsetLeft())
                    .arg(mouseEvent->pageY() - targetElement->offsetTop())
                    .arg(modifierMask(*mouseEvent));
            }
        } else if (event->isKeyboardEvent()) {
            auto* keyEvent = static_cast<KeyboardEvent*>(event);
            if (eventName == "keypress") // character only, deliberately no modifiers
                details = QString::number(keyEvent->charCode());
            else  // keydown or keyup. Format: keyCode,modifiers
                details = QString("%1,%2")
                    .arg(keyEvent->keyCode())
                    .arg(modifierMask(*keyEvent));
        } else {
            // For other events, typically return 0 for 'which' detail.
            details = QString::number(0);
        }

        // Convert WebCore event data to our API format
        callbackFn(eventName, usrPtr, location, usrVal2, details);
    };

    // For resize events, add listener to document rather than element
    //and IF THERE ARE ISSUES WITH SCROLL AND FOCUS/BLUR EVENTS:
    //(*Note: other special case events when applied to the body element
    //might be scroll (may be triggered at window level). also focus & blur
    // -- may bubble up to the document level.*)
    EventTarget* target;
    if (eventName == "resize") {
        target = static_cast<EventTarget*>(m_element->document().domWindow());
    } else {
        target = static_cast<EventTarget*>(m_element);
    }

    // Create and store listener - WebKit's EventListenerMap takes care of actual storage
    RefPtr<HipeCoreEventListener> listener = WebCore::HipeCoreEventListener::create(callback).ptr();
    target->addEventListener(AtomicString(eventName), WTFMove(listener), false);
}

bool QWebElement::handlesEvent(const QString& eventName) {
    if (!m_element) return false;

    // Choose right target for getting listeners
    EventTarget* target = (eventName == "resize")
            ? static_cast<EventTarget*>(m_element->document().domWindow())
            : static_cast<EventTarget*>(m_element);

    // Only listeners that report events count: a preventer isn't a request for them.
    for (const auto& registeredListener : target->getEventListeners(AtomicString(eventName))) {
        const EventListener* listener = registeredListener.listener.get();
        if (listener->type() != EventListener::CPPEventListenerType
            || !static_cast<const HipeCoreEventListener*>(listener)->isPreventer())
            return true;
    }
    return false;
}

void QWebElement::cancelEvent(const QString& eventName) {
    if (!m_element) return;

    // Choose right target for getting listeners
    EventTarget* target = (eventName == "resize")
            ? static_cast<EventTarget*>(m_element->document().domWindow())
            : static_cast<EventTarget*>(m_element);

    // Get all listeners for this event type. A copy: removing a listener changes the element's own list.
    EventListenerVector listeners = target->getEventListeners(AtomicString(eventName));

    // Remove each listener
    for (const auto& registeredListener : listeners) {
        target->removeEventListener(AtomicString(eventName), 
                                    registeredListener.listener.get(),
                                    registeredListener.useCapture);
    }
}


namespace {
// One rule of setDefaultPrevention(): a code and a modifier mask, -1 for any.
struct PreventionRule {
    int code;
    int modifiers;
};
}

static bool parsePreventionField(const QString& field, int max, int& value)
{
    if (field == QLatin1String("*")) {
        value = -1;
        return true;
    }
    bool ok = false;
    value = field.toInt(&ok);
    return ok && value >= 0 && value <= max && !field.startsWith(QLatin1Char('+'));
}

/*!
    Cancels the default action of \a eventName events on this element (or its
    descendants) that match \a rules, replacing any rules set before for that
    event. An empty \a rules removes them.

    \a rules is a list separated by ';' of "code,modifiers" items, written like
    the details requestEvent() reports: the code is the keyCode (keydown, keyup),
    the charCode (keypress) or the mouse button (1 = left, 2 = middle, 3 = right),
    and modifiers is the mask 1 = Shift, 2 = Alt, 4 = Ctrl, 8 = Meta, which must
    match exactly. "*" matches anything, and ",modifiers" may be left out to match
    any modifiers, so "*" alone matches every event. A specific code or mask never
    matches an event that doesn't carry one (e.g. a wheel event has no code).
    Items that can't be parsed are ignored.

    For example, "9,0;9,1" cancels Tab and Shift+Tab for keydown, and "*,4"
    cancels Ctrl+wheel for wheel. Events are cancelled whether or not they're
    also requested with requestEvent().
*/
void QWebElement::setDefaultPrevention(const QString& eventName, const QString& rules)
{
    if (!m_element)
        return;

    EventTarget* target = (eventName == "resize")
            ? static_cast<EventTarget*>(m_element->document().domWindow())
            : static_cast<EventTarget*>(m_element);
    if (!target)
        return;
    AtomicString type(eventName);

    // Remove the rules set before (a copy of the list: removing changes the element's own).
    EventListenerVector listeners = target->getEventListeners(type);
    for (const auto& registeredListener : listeners) {
        EventListener* listener = registeredListener.listener.get();
        if (listener->type() == EventListener::CPPEventListenerType
            && static_cast<HipeCoreEventListener*>(listener)->isPreventer())
            target->removeEventListener(type, listener, registeredListener.useCapture);
    }

    Vector<PreventionRule> parsed;
    for (const QString& item : rules.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        QStringList fields = item.trimmed().split(QLatin1Char(','));
        PreventionRule rule;
        if (fields.size() > 2 || !parsePreventionField(fields[0].trimmed(), 0x10FFFF, rule.code))
            continue;
        rule.modifiers = -1;
        if (fields.size() == 2 && !parsePreventionField(fields[1].trimmed(), 15, rule.modifiers))
            continue;
        parsed.append(rule);
    }
    if (parsed.isEmpty())
        return;

    bool isKeypress = eventName == "keypress";
    auto callback = [parsed, isKeypress](WebCore::Event* event) {
        // The code and modifiers as requestEvent() reports them; -1 where the event has none.
        int code = -1;
        int modifiers = -1;
        if (event->isWheelEvent())
            modifiers = modifierMask(*static_cast<WheelEvent*>(event));
        else if (event->isMouseEvent()) {
            auto* mouseEvent = static_cast<MouseEvent*>(event);
            code = mouseEvent->button() + 1;
            modifiers = modifierMask(*mouseEvent);
        } else if (event->isKeyboardEvent()) {
            auto* keyEvent = static_cast<KeyboardEvent*>(event);
            code = isKeypress ? keyEvent->charCode() : keyEvent->keyCode();
            modifiers = modifierMask(*keyEvent);
        }
        for (const PreventionRule& rule : parsed) {
            if ((rule.code < 0 || rule.code == code) && (rule.modifiers < 0 || rule.modifiers == modifiers)) {
                event->preventDefault();
                return;
            }
        }
    };
    RefPtr<HipeCoreEventListener> listener = WebCore::HipeCoreEventListener::create(callback, true).ptr();
    target->addEventListener(type, WTFMove(listener), false);
}


void QWebElement::setDraggable(bool draggable) {
    if (!m_element)
        return;
    
    if (is<HTMLElement>(*m_element))
        downcast<HTMLElement>(*m_element).setDraggable(draggable);
}


/*!
    Returns the geometry of this element, relative to its containing frame.

    \sa tagName()
*/
QRect QWebElement::geometry() const
{
    if (!m_element)
        return QRect();

    auto* renderer = m_element->renderer();
    if (!renderer)
        return QRect();

    return renderer->absoluteBoundingBoxRect();
}

/*!
    Returns the width, in CSS pixels, that rendering \a text would take using
    this element's computed font. Does not wrap or otherwise lay the text out
    -- for a multi-line estimate, split on newlines and sum/max as needed.

    This reaches the same WebCore::FontCascade::width(const TextRun&) call
    that drives all real layout/painting -- CanvasRenderingContext2D's
    measureText() (kept dormant since hipecore removed JS, see TextMetrics.h)
    is a worked example of the same call against a different font source.

    \sa fontAscent(), fontDescent(), fontLineSpacing()
*/
qreal QWebElement::textWidth(const QString &text) const
{
    if (!m_element)
        return 0;

    // A freshly-inserted or freshly-restyled element may not have an
    // up-to-date renderer/style yet -- force it, same as x11EmbedTargetXid().
    m_element->document().updateLayoutIgnorePendingStylesheets();

    auto* renderer = m_element->renderer();
    if (!renderer)
        return 0;

    return renderer->style().fontCascade().width(WebCore::TextRun(WTF::String(text)));
}

/*!
    Returns the ascent, in CSS pixels, of this element's computed font above
    its baseline.

    \sa fontDescent(), fontLineSpacing(), textWidth()
*/
qreal QWebElement::fontAscent() const
{
    if (!m_element)
        return 0;

    m_element->document().updateLayoutIgnorePendingStylesheets();

    auto* renderer = m_element->renderer();
    if (!renderer)
        return 0;

    return renderer->style().fontMetrics().floatAscent();
}

/*!
    Returns the descent, in CSS pixels, of this element's computed font below
    its baseline.

    \sa fontAscent(), fontLineSpacing(), textWidth()
*/
qreal QWebElement::fontDescent() const
{
    if (!m_element)
        return 0;

    m_element->document().updateLayoutIgnorePendingStylesheets();

    auto* renderer = m_element->renderer();
    if (!renderer)
        return 0;

    return renderer->style().fontMetrics().floatDescent();
}

/*!
    Returns the recommended distance, in CSS pixels, between consecutive
    baselines when laying out multiple lines of this element's computed font.

    \sa fontAscent(), fontDescent(), textWidth()
*/
qreal QWebElement::fontLineSpacing() const
{
    if (!m_element)
        return 0;

    m_element->document().updateLayoutIgnorePendingStylesheets();

    auto* renderer = m_element->renderer();
    if (!renderer)
        return 0;

    return renderer->style().fontMetrics().floatLineSpacing();
}

/*!
    Hints to this element's document that a burst of layout-affecting mutations is under way, so
    its incremental-layout scheduler should batch aggressively for \a milliseconds rather than
    laying out on every change. Repeated calls just keep refreshing that window; it decays back
    down to a small floor on its own once nothing calls this for a while -- see
    WebCore::Document::bumpLayoutBatchingDelay().
*/
void QWebElement::bumpLayoutBatchingDelay(int milliseconds)
{
    if (!m_element)
        return;

    m_element->document().bumpLayoutBatchingDelay(std::chrono::milliseconds(milliseconds));
}

/*!
    Turns this element into an X11 embed target and returns the XID of the
    native child window hipecore creates and geometry-syncs for it. Returns 0
    if this element isn't an unreplaced <object> (i.e. one whose renderer is
    a plain RenderWidget/RenderEmbeddedObject, not fallback content, an
    image, or something else), or if hipecore wasn't built with X11 support.

    \sa geometry()
*/
quintptr QWebElement::x11EmbedTargetXid()
{
#if PLATFORM(X11)
    if (!m_element)
        return 0;

    // A freshly-inserted element may not have a renderer yet - attach can be deferred past the
    // point where a caller (e.g. a Hipe instruction handler) is done mutating the DOM and asks
    // about this element in the same batch. Force it, same as Element::offsetLeft() and friends.
    m_element->document().updateLayoutIgnorePendingStylesheets();

    auto* renderer = m_element->renderer();
    if (!is<WebCore::RenderWidget>(renderer))
        return 0;

    // setWidget() is what synchronously triggers the widget's setParent()
    // override (via ScrollView::addChild()), which is where the child X11
    // window actually gets created - so xid() only becomes non-zero after
    // this call returns. The raw pointer stays valid: the RenderWidget now
    // holds a ref via the RefPtr passed to setWidget().
    auto* widget = new WebCore::X11EmbedWidgetQt();
    downcast<WebCore::RenderWidget>(*renderer).setWidget(adoptRef(widget));
    return widget->xid();
#else
    return 0;
#endif
}

// ---- Canvas 2D dispatch -----------------------------------------------------
//
// Receiving-end implementation for hipe's HIPE_OP_USE_CANVAS / HIPE_OP_CANVAS_ACTION /
// HIPE_OP_CANVAS_SET_PROPERTY, replacing the JS-eval path those opcodes used before
// script execution was disabled. WebCore::CanvasRenderingContext2D itself is
// unmodified upstream code; everything below is purely the string-verb/property
// vocabulary and argument parsing that lets hiped reach it without JS bindings.
//
// Scope: 2D context only (no WebGL). Write-only (no getImageData/putImageData
// readback). No CanvasGradient/CanvasPattern/DOMPath support -- those aren't DOM
// nodes, so they have no id to reference and would need a separate handle mechanism.
// drawImage's image/canvas/video source arguments ARE supported, referenced by the
// same id string used elsewhere in hipe's protocol (HIPE_OP_APPEND_TAG ... id),
// resolved the same way HIPE_OP_GET_BY_ID already does.

namespace {

enum class CanvasVerb : uint8_t {
    Save, Restore,
    Scale, Rotate, Translate, Transform, SetTransform,
    BeginPath, ClosePath, MoveTo, LineTo, QuadraticCurveTo, BezierCurveTo,
    ArcTo, Arc, Ellipse, Rect,
    Fill, Stroke, Clip,
    ClearRect, FillRect, StrokeRect,
    FillText, StrokeText,
    SetLineDash,
    DrawImage,
    // Not part of the Canvas 2D spec -- see HTMLCanvasElement::setSrcRegion()'s own doc
    // comment for why this exists at all (hipe's SET_SRC instruction can't carry a
    // destination rect inline the way a real putImageData() call does).
    SetSrcRegion,
    // Real Canvas 2D spec methods, but their object-taking overloads were excluded from
    // the original table because CanvasGradient/CanvasPattern/DOMPath have no DOM
    // presence to hang an id off. Reachable now via the same client-assigned-id
    // convention HIPE_OP_APPEND_TAG already uses -- see
    // CanvasRenderingContext2D::setNamedGradient()/setNamedPattern()/setNamedPath()'s own
    // doc comment.
    CreateLinearGradient, CreateRadialGradient, AddColorStop,
    CreatePattern,
    DeleteGradient, DeletePattern,
    CreatePath2D, DeletePath2D, SelectPath2D,
};

struct CanvasVerbInfo {
    const char* name; // matches the Canvas 2D spec's method name exactly
    CanvasVerb verb;
    uint8_t minArgs;
    uint8_t maxArgs;
    // drawImage's real valid arities are the set {3, 5, 9}, not every value in
    // [minArgs, maxArgs] -- dispatchCanvasVerb() re-checks args.size() itself, so an
    // out-of-set-but-in-range count (4, 6, 7, 8) just falls through to a no-op rather
    // than misdispatching.
};

static const CanvasVerbInfo canvasVerbTable[] = {
    { "save",             CanvasVerb::Save,             0, 0 },
    { "restore",          CanvasVerb::Restore,          0, 0 },
    { "scale",            CanvasVerb::Scale,            2, 2 },
    { "rotate",           CanvasVerb::Rotate,           1, 1 },
    { "translate",        CanvasVerb::Translate,        2, 2 },
    { "transform",        CanvasVerb::Transform,        6, 6 },
    { "setTransform",     CanvasVerb::SetTransform,     6, 6 },
    { "beginPath",        CanvasVerb::BeginPath,        0, 0 },
    { "closePath",        CanvasVerb::ClosePath,        0, 0 },
    { "moveTo",           CanvasVerb::MoveTo,           2, 2 },
    { "lineTo",           CanvasVerb::LineTo,           2, 2 },
    { "quadraticCurveTo", CanvasVerb::QuadraticCurveTo, 4, 4 },
    { "bezierCurveTo",    CanvasVerb::BezierCurveTo,    6, 6 },
    { "arcTo",            CanvasVerb::ArcTo,            5, 5 }, // x0,y0,x1,y1,radius
    { "arc",              CanvasVerb::Arc,              6, 6 }, // x,y,r,startAngle,endAngle,anticlockwise
    { "ellipse",          CanvasVerb::Ellipse,          8, 8 }, // x,y,rx,ry,rotation,startAngle,endAngle,anticlockwise
    { "rect",             CanvasVerb::Rect,             4, 4 },
    { "fill",             CanvasVerb::Fill,             0, 2 }, // 0/1 = implicit path (+ optional winding); 2 = path id + winding
    { "stroke",           CanvasVerb::Stroke,           0, 1 }, // 0 = implicit path; 1 = path id
    { "clip",             CanvasVerb::Clip,             0, 2 }, // 0/1 = implicit path (+ optional winding); 2 = path id + winding
    { "clearRect",        CanvasVerb::ClearRect,        4, 4 },
    { "fillRect",         CanvasVerb::FillRect,         4, 4 },
    { "strokeRect",       CanvasVerb::StrokeRect,       4, 4 },
    { "fillText",         CanvasVerb::FillText,         3, 4 },
    { "strokeText",       CanvasVerb::StrokeText,       3, 4 },
    { "setLineDash",      CanvasVerb::SetLineDash,      1, 1 }, // single bracketed number-array token, e.g. "[4,2]"
    { "drawImage",        CanvasVerb::DrawImage,        3, 9 }, // arg0 = source element id; see NOTE above
    { "setSrcRegion",     CanvasVerb::SetSrcRegion,     4, 4 }, // dx, dy, width, height -- see HTMLCanvasElement::setSrcRegion()
    { "createLinearGradient", CanvasVerb::CreateLinearGradient, 5, 5 }, // id, x0, y0, x1, y1
    { "createRadialGradient", CanvasVerb::CreateRadialGradient, 7, 7 }, // id, x0, y0, r0, x1, y1, r1
    { "addColorStop",     CanvasVerb::AddColorStop,     3, 3 }, // gradientId, offset, color
    { "createPattern",    CanvasVerb::CreatePattern,    3, 3 }, // id, sourceElementId, repetition
    { "deleteGradient",   CanvasVerb::DeleteGradient,   1, 1 },
    { "deletePattern",    CanvasVerb::DeletePattern,    1, 1 },
    { "createPath2D",     CanvasVerb::CreatePath2D,     1, 1 },
    { "deletePath2D",     CanvasVerb::DeletePath2D,     1, 1 },
    { "selectPath2D",     CanvasVerb::SelectPath2D,     1, 1 }, // "" reselects this context's own implicit path
};

enum class CanvasPropertyArgKind { Number, Str, Bool };

enum class CanvasProperty : uint8_t {
    FillStyle, StrokeStyle, LineWidth, LineCap, LineJoin, MiterLimit,
    LineDashOffset, ShadowOffsetX, ShadowOffsetY, ShadowBlur, ShadowColor,
    GlobalAlpha, GlobalCompositeOperation, Font, TextAlign, TextBaseline,
    ImageSmoothingEnabled,
};

struct CanvasPropertyInfo {
    const char* name;
    CanvasProperty property;
    CanvasPropertyArgKind argKind;
};

static const CanvasPropertyInfo canvasPropertyTable[] = {
    { "fillStyle",                CanvasProperty::FillStyle,                CanvasPropertyArgKind::Str },
    { "strokeStyle",              CanvasProperty::StrokeStyle,              CanvasPropertyArgKind::Str },
    { "lineWidth",                CanvasProperty::LineWidth,                CanvasPropertyArgKind::Number },
    { "lineCap",                  CanvasProperty::LineCap,                  CanvasPropertyArgKind::Str }, // butt|round|square
    { "lineJoin",                 CanvasProperty::LineJoin,                 CanvasPropertyArgKind::Str }, // round|bevel|miter
    { "miterLimit",               CanvasProperty::MiterLimit,               CanvasPropertyArgKind::Number },
    { "lineDashOffset",           CanvasProperty::LineDashOffset,           CanvasPropertyArgKind::Number },
    { "shadowOffsetX",            CanvasProperty::ShadowOffsetX,            CanvasPropertyArgKind::Number },
    { "shadowOffsetY",            CanvasProperty::ShadowOffsetY,            CanvasPropertyArgKind::Number },
    { "shadowBlur",               CanvasProperty::ShadowBlur,               CanvasPropertyArgKind::Number },
    { "shadowColor",              CanvasProperty::ShadowColor,              CanvasPropertyArgKind::Str },
    { "globalAlpha",              CanvasProperty::GlobalAlpha,              CanvasPropertyArgKind::Number },
    { "globalCompositeOperation", CanvasProperty::GlobalCompositeOperation, CanvasPropertyArgKind::Str },
    { "font",                     CanvasProperty::Font,                     CanvasPropertyArgKind::Str }, // CSS font shorthand, e.g. "16px sans-serif"
    { "textAlign",                CanvasProperty::TextAlign,                CanvasPropertyArgKind::Str },
    { "textBaseline",             CanvasProperty::TextBaseline,             CanvasPropertyArgKind::Str },
    { "imageSmoothingEnabled",    CanvasProperty::ImageSmoothingEnabled,    CanvasPropertyArgKind::Bool },
};

static const CanvasVerbInfo* findCanvasVerb(const WTF::String& name)
{
    for (auto& entry : canvasVerbTable) {
        if (name == entry.name)
            return &entry;
    }
    return nullptr;
}

static const CanvasPropertyInfo* findCanvasProperty(const WTF::String& name)
{
    for (auto& entry : canvasPropertyTable) {
        if (name == entry.name)
            return &entry;
    }
    return nullptr;
}

// Trims whitespace, then strips one layer of surrounding double quotes if present,
// unescaping \" and \\ inside. Used on every token the splitter below produces.
static WTF::String unquoteAndTrim(const WTF::String& raw)
{
    WTF::String s = raw.stripWhiteSpace();
    if (s.length() < 2 || s[0] != '"' || s[s.length() - 1] != '"')
        return s;

    WTF::String inner = s.substring(1, s.length() - 2);
    StringBuilder out;
    for (unsigned i = 0; i < inner.length(); ++i) {
        if (inner[i] == '\\' && i + 1 < inner.length()) {
            out.append(inner[i + 1]);
            ++i;
        } else
            out.append(inner[i]);
    }
    return out.toString();
}

// Splits a flat argument-list string into tokens, at top-level (depth-0, unquoted)
// commas only -- so "rgba(0, 0, 0, 0.5)" and "[4, 2]" stay single tokens, and
// "\"a, b\", 1, 2" splits into exactly 3. An empty input produces zero tokens (not
// one empty token), so zero-arg verbs like save() get arity 0.
static Vector<WTF::String> splitCanvasArgs(const WTF::String& argsString)
{
    Vector<WTF::String> tokens;
    if (argsString.isEmpty())
        return tokens;

    unsigned start = 0;
    int depth = 0;
    bool inQuotes = false;
    unsigned length = argsString.length();
    for (unsigned i = 0; i < length; ++i) {
        UChar c = argsString[i];
        if (inQuotes) {
            if (c == '\\' && i + 1 < length)
                ++i; // skip escaped character, including an escaped quote
            else if (c == '"')
                inQuotes = false;
            continue;
        }
        if (c == '"')
            inQuotes = true;
        else if (c == '(' || c == '[')
            ++depth;
        else if (c == ')' || c == ']') {
            if (depth > 0)
                --depth;
        } else if (c == ',' && depth == 0) {
            tokens.append(unquoteAndTrim(argsString.substring(start, i - start)));
            start = i + 1;
        }
    }
    tokens.append(unquoteAndTrim(argsString.substring(start, length - start)));
    return tokens;
}

// Parses `count` floats from args[start..start+count), writing into out[0..count).
// Returns false (leaving out partially written) on the first malformed token, per
// the "bad input no-ops, never crashes" policy -- callers bail immediately on false.
static bool parseFloats(const Vector<WTF::String>& args, unsigned start, unsigned count, float* out)
{
    for (unsigned i = 0; i < count; ++i) {
        bool ok = false;
        out[i] = args[start + i].toFloat(&ok);
        if (!ok)
            return false;
    }
    return true;
}

// Parses a single bracketed number-array token, e.g. "[4, 2]" -> {4.0, 2.0}. Used
// only by setLineDash today. An empty array ("[]") is valid (clears the dash pattern).
static bool parseFloatArrayArg(const WTF::String& token, Vector<float>& out)
{
    WTF::String inner = token.stripWhiteSpace();
    if (inner.length() < 2 || inner[0] != '[' || inner[inner.length() - 1] != ']')
        return false;
    inner = inner.substring(1, inner.length() - 2).stripWhiteSpace();
    if (inner.isEmpty())
        return true; // "[]"

    for (auto& piece : splitCanvasArgs(inner)) {
        bool ok = false;
        float value = piece.toFloat(&ok);
        if (!ok)
            return false;
        out.append(value);
    }
    return true;
}

// Resolves a plain id (no "#", no CSS selector syntax) to the element it names, for
// drawImage's source argument -- the same DOM-id lookup HIPE_OP_GET_BY_ID already
// performs (there via QWebElement::findFirst("#" + id); here via Document's own
// getElementById() directly, since this is a plain id lookup, not a selector query).
static WebCore::Element* resolveCanvasElementRefArg(WebCore::CanvasRenderingContext2D& ctx, const WTF::String& id)
{
    if (!ctx.canvas())
        return nullptr;
    return ctx.canvas()->document().getElementById(id);
}

static bool dispatchDrawImage(WebCore::CanvasRenderingContext2D& ctx, const Vector<WTF::String>& args)
{
    WebCore::Element* source = resolveCanvasElementRefArg(ctx, args[0]);
    if (!source)
        return false; // unknown id: no-op, not a crash

    float n[8];
    if (!parseFloats(args, 1, args.size() - 1, n))
        return false;

    WebCore::ExceptionCode ec = 0; // no reply channel exists to surface this through;
                                    // an invalid rect etc. just draws nothing.
    // drawImage's C++ overloads are selected by source pointer type, not runtime
    // polymorphism, so this switches on tag name explicitly.
    if (source->hasTagName(WebCore::HTMLNames::imgTag)) {
        auto& image = static_cast<WebCore::HTMLImageElement&>(*source);
        if (args.size() == 3) ctx.drawImage(&image, n[0], n[1], ec);
        else if (args.size() == 5) ctx.drawImage(&image, n[0], n[1], n[2], n[3], ec);
        else if (args.size() == 9) ctx.drawImage(&image, n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7], ec);
        else return false;
    } else if (source->hasTagName(WebCore::HTMLNames::canvasTag)) {
        auto& canvas = static_cast<WebCore::HTMLCanvasElement&>(*source);
        if (args.size() == 3) ctx.drawImage(&canvas, n[0], n[1], ec);
        else if (args.size() == 5) ctx.drawImage(&canvas, n[0], n[1], n[2], n[3], ec);
        else if (args.size() == 9) ctx.drawImage(&canvas, n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7], ec);
        else return false;
    }
#if ENABLE(VIDEO)
    else if (source->hasTagName(WebCore::HTMLNames::videoTag)) {
        // Drawing a live <video> frame into a canvas -- kept given how much of this
        // project is video/media work; same three-shape pattern as above.
        auto& video = static_cast<WebCore::HTMLVideoElement&>(*source);
        if (args.size() == 3) ctx.drawImage(&video, n[0], n[1], ec);
        else if (args.size() == 5) ctx.drawImage(&video, n[0], n[1], n[2], n[3], ec);
        else if (args.size() == 9) ctx.drawImage(&video, n[0], n[1], n[2], n[3], n[4], n[5], n[6], n[7], ec);
        else return false;
    }
#endif
    else
        return false; // resolved id isn't an img/canvas/video element

    return true;
}

// The actual per-verb behavior. Table lookup + arity checking happens in
// QWebElement::canvasAction(); this only runs once both have already passed.
static bool dispatchCanvasVerb(WebCore::CanvasRenderingContext2D& ctx, CanvasVerb verb, const Vector<WTF::String>& args)
{
    float a[8];
    switch (verb) {
    case CanvasVerb::Save: ctx.save(); return true;
    case CanvasVerb::Restore: ctx.restore(); return true;
    case CanvasVerb::Scale:
        if (!parseFloats(args, 0, 2, a)) return false;
        ctx.scale(a[0], a[1]); return true;
    case CanvasVerb::Rotate:
        if (!parseFloats(args, 0, 1, a)) return false;
        ctx.rotate(a[0]); return true;
    case CanvasVerb::Translate:
        if (!parseFloats(args, 0, 2, a)) return false;
        ctx.translate(a[0], a[1]); return true;
    case CanvasVerb::Transform:
        if (!parseFloats(args, 0, 6, a)) return false;
        ctx.transform(a[0], a[1], a[2], a[3], a[4], a[5]); return true;
    case CanvasVerb::SetTransform:
        if (!parseFloats(args, 0, 6, a)) return false;
        ctx.setTransform(a[0], a[1], a[2], a[3], a[4], a[5]); return true;
    // Path-building verbs dispatch through currentPathTarget() rather than directly on
    // ctx: this context and a named DOMPath (selected via selectPath2D) share the
    // CanvasPathMethods base, so both cases call the exact same method with the exact
    // same arguments -- no path-specific branching needed per verb. Falls through to a
    // no-op if selectPath2D named an id that doesn't (or no longer) resolves.
    // beginPath() is deliberately NOT routed through currentPathTarget(): it means
    // "reset this context's own current path," a context-level operation with no
    // CanvasPathMethods equivalent (a named DOMPath doesn't get "begun," it's just
    // created fresh via createPath2D() under a new id instead). Always applies to ctx
    // itself regardless of what selectPath2D has selected.
    case CanvasVerb::BeginPath: ctx.beginPath(); return true;
    case CanvasVerb::ClosePath: {
        auto* target = ctx.currentPathTarget();
        if (!target) return false;
        target->closePath(); return true;
    }
    case CanvasVerb::MoveTo: {
        if (!parseFloats(args, 0, 2, a)) return false;
        auto* target = ctx.currentPathTarget();
        if (!target) return false;
        target->moveTo(a[0], a[1]); return true;
    }
    case CanvasVerb::LineTo: {
        if (!parseFloats(args, 0, 2, a)) return false;
        auto* target = ctx.currentPathTarget();
        if (!target) return false;
        target->lineTo(a[0], a[1]); return true;
    }
    case CanvasVerb::QuadraticCurveTo: {
        if (!parseFloats(args, 0, 4, a)) return false;
        auto* target = ctx.currentPathTarget();
        if (!target) return false;
        target->quadraticCurveTo(a[0], a[1], a[2], a[3]); return true;
    }
    case CanvasVerb::BezierCurveTo: {
        if (!parseFloats(args, 0, 6, a)) return false;
        auto* target = ctx.currentPathTarget();
        if (!target) return false;
        target->bezierCurveTo(a[0], a[1], a[2], a[3], a[4], a[5]); return true;
    }
    case CanvasVerb::ArcTo: {
        if (!parseFloats(args, 0, 5, a)) return false;
        auto* target = ctx.currentPathTarget();
        if (!target) return false;
        WebCore::ExceptionCode ec = 0;
        target->arcTo(a[0], a[1], a[2], a[3], a[4], ec); return true;
    }
    case CanvasVerb::Arc: {
        if (!parseFloats(args, 0, 5, a)) return false;
        auto* target = ctx.currentPathTarget();
        if (!target) return false;
        bool anticlockwise = args[5] == "true";
        WebCore::ExceptionCode ec = 0;
        target->arc(a[0], a[1], a[2], a[3], a[4], anticlockwise, ec); return true;
    }
    case CanvasVerb::Ellipse: {
        if (!parseFloats(args, 0, 7, a)) return false;
        auto* target = ctx.currentPathTarget();
        if (!target) return false;
        bool anticlockwise = args[7] == "true";
        WebCore::ExceptionCode ec = 0;
        target->ellipse(a[0], a[1], a[2], a[3], a[4], a[5], a[6], anticlockwise, ec); return true;
    }
    case CanvasVerb::Rect: {
        if (!parseFloats(args, 0, 4, a)) return false;
        auto* target = ctx.currentPathTarget();
        if (!target) return false;
        target->rect(a[0], a[1], a[2], a[3]); return true;
    }
    case CanvasVerb::Fill:
        if (args.isEmpty()) ctx.fill();
        else if (args.size() == 1) ctx.fill(args[0]);
        else { // 2 args: path id + winding rule
            auto* path = ctx.namedPath(args[0]);
            if (!path) return false;
            ctx.fill(path, args[1]);
        }
        return true;
    case CanvasVerb::Stroke:
        if (args.isEmpty()) ctx.stroke();
        else { // 1 arg: path id
            auto* path = ctx.namedPath(args[0]);
            if (!path) return false;
            ctx.stroke(path);
        }
        return true;
    case CanvasVerb::Clip:
        if (args.isEmpty()) ctx.clip();
        else if (args.size() == 1) ctx.clip(args[0]);
        else { // 2 args: path id + winding rule
            auto* path = ctx.namedPath(args[0]);
            if (!path) return false;
            ctx.clip(path, args[1]);
        }
        return true;
    case CanvasVerb::ClearRect:
        if (!parseFloats(args, 0, 4, a)) return false;
        ctx.clearRect(a[0], a[1], a[2], a[3]); return true;
    case CanvasVerb::FillRect:
        if (!parseFloats(args, 0, 4, a)) return false;
        ctx.fillRect(a[0], a[1], a[2], a[3]); return true;
    case CanvasVerb::StrokeRect:
        if (!parseFloats(args, 0, 4, a)) return false;
        ctx.strokeRect(a[0], a[1], a[2], a[3]); return true;
    case CanvasVerb::FillText:
        if (!parseFloats(args, 1, 2, a)) return false;
        if (args.size() == 3) ctx.fillText(args[0], a[0], a[1]);
        else {
            if (!parseFloats(args, 3, 1, a + 2)) return false;
            ctx.fillText(args[0], a[0], a[1], a[2]);
        }
        return true;
    case CanvasVerb::StrokeText:
        if (!parseFloats(args, 1, 2, a)) return false;
        if (args.size() == 3) ctx.strokeText(args[0], a[0], a[1]);
        else {
            if (!parseFloats(args, 3, 1, a + 2)) return false;
            ctx.strokeText(args[0], a[0], a[1], a[2]);
        }
        return true;
    case CanvasVerb::SetLineDash: {
        Vector<float> dashes;
        if (!parseFloatArrayArg(args[0], dashes)) return false;
        ctx.setLineDash(dashes); return true;
    }
    case CanvasVerb::DrawImage:
        return dispatchDrawImage(ctx, args);
    case CanvasVerb::SetSrcRegion: {
        if (!parseFloats(args, 0, 4, a)) return false;
        if (!ctx.canvas()) return false;
        ctx.canvas()->setSrcRegion(WebCore::IntRect(static_cast<int>(a[0]), static_cast<int>(a[1]),
                                                     static_cast<int>(a[2]), static_cast<int>(a[3])));
        return true;
    }
    case CanvasVerb::CreateLinearGradient: {
        if (!parseFloats(args, 1, 4, a)) return false;
        WebCore::ExceptionCode ec = 0;
        auto gradient = ctx.createLinearGradient(a[0], a[1], a[2], a[3], ec);
        if (!gradient) return false;
        ctx.setNamedGradient(args[0], gradient);
        return true;
    }
    case CanvasVerb::CreateRadialGradient: {
        if (!parseFloats(args, 1, 6, a)) return false;
        WebCore::ExceptionCode ec = 0;
        auto gradient = ctx.createRadialGradient(a[0], a[1], a[2], a[3], a[4], a[5], ec);
        if (!gradient) return false;
        ctx.setNamedGradient(args[0], gradient);
        return true;
    }
    case CanvasVerb::AddColorStop: {
        auto* gradient = ctx.namedGradient(args[0]);
        if (!gradient) return false;
        float offset;
        if (!parseFloats(args, 1, 1, &offset)) return false;
        WebCore::ExceptionCode ec = 0;
        gradient->addColorStop(offset, args[2], ec);
        return true;
    }
    case CanvasVerb::CreatePattern: {
        WebCore::Element* source = resolveCanvasElementRefArg(ctx, args[1]);
        if (!source) return false;
        WebCore::ExceptionCode ec = 0;
        RefPtr<WebCore::CanvasPattern> pattern;
        if (source->hasTagName(WebCore::HTMLNames::imgTag)) {
            auto& image = static_cast<WebCore::HTMLImageElement&>(*source);
            pattern = ctx.createPattern(&image, args[2], ec);
        } else if (source->hasTagName(WebCore::HTMLNames::canvasTag)) {
            auto& canvasElement = static_cast<WebCore::HTMLCanvasElement&>(*source);
            pattern = ctx.createPattern(&canvasElement, args[2], ec);
        } else {
            return false;
        }
        if (!pattern) return false;
        ctx.setNamedPattern(args[0], pattern);
        return true;
    }
    case CanvasVerb::DeleteGradient: ctx.deleteNamedGradient(args[0]); return true;
    case CanvasVerb::DeletePattern: ctx.deleteNamedPattern(args[0]); return true;
    case CanvasVerb::CreatePath2D: ctx.setNamedPath(args[0], WebCore::DOMPath::create()); return true;
    case CanvasVerb::DeletePath2D: ctx.deleteNamedPath(args[0]); return true;
    case CanvasVerb::SelectPath2D: ctx.selectPath(args[0]); return true;
    }
    return false;
}

// Applies a single property value. Str-kind properties (colors, enum strings, font
// shorthand) pass the token straight through to the real setter, which does its own
// CSS-level parsing/validation and silently ignores an invalid value -- same as the
// real Canvas 2D spec's behavior for e.g. `ctx.lineCap = "bogus"`. fillStyle/
// strokeStyle are the one exception: CanvasStyle has no direct string constructor, so
// the color string is parsed here first via WebCore::Color.
static bool applyCanvasProperty(WebCore::CanvasRenderingContext2D& ctx, const CanvasPropertyInfo& info, const WTF::String& value)
{
    switch (info.argKind) {
    case CanvasPropertyArgKind::Number: {
        bool ok = false;
        float number = value.toFloat(&ok);
        if (!ok)
            return false;
        switch (info.property) {
        case CanvasProperty::LineWidth: ctx.setLineWidth(number); return true;
        case CanvasProperty::MiterLimit: ctx.setMiterLimit(number); return true;
        case CanvasProperty::LineDashOffset: ctx.setLineDashOffset(number); return true;
        case CanvasProperty::ShadowOffsetX: ctx.setShadowOffsetX(number); return true;
        case CanvasProperty::ShadowOffsetY: ctx.setShadowOffsetY(number); return true;
        case CanvasProperty::ShadowBlur: ctx.setShadowBlur(number); return true;
        case CanvasProperty::GlobalAlpha: ctx.setGlobalAlpha(number); return true;
        default: return false;
        }
    }
    case CanvasPropertyArgKind::Bool: {
        bool boolValue = value == "true";
        if (info.property == CanvasProperty::ImageSmoothingEnabled) {
            ctx.setImageSmoothingEnabled(boolValue);
            return true;
        }
        return false;
    }
    case CanvasPropertyArgKind::Str:
        if (info.property == CanvasProperty::FillStyle || info.property == CanvasProperty::StrokeStyle) {
            // CSS's own url(#id) convention for referencing a paint server defined
            // elsewhere (e.g. `fill: url(#gradientId)`), reused here for a named
            // gradient/pattern rather than inventing new syntax -- see
            // CanvasRenderingContext2D::setNamedGradient()'s doc comment.
            if (value.startsWith("url(#") && value.endsWith(")")) {
                WTF::String id = value.substring(5, value.length() - 6);
                bool isFill = info.property == CanvasProperty::FillStyle;
                if (auto* gradient = ctx.namedGradient(id)) {
                    WebCore::CanvasStyle style(gradient);
                    if (isFill) ctx.setFillStyle(style); else ctx.setStrokeStyle(style);
                } else if (auto* pattern = ctx.namedPattern(id)) {
                    WebCore::CanvasStyle style(pattern);
                    if (isFill) ctx.setFillStyle(style); else ctx.setStrokeStyle(style);
                } else {
                    return false; // unknown id: no-op, not a crash
                }
                return true;
            }
            WebCore::Color color(value);
            if (!color.isValid())
                return false;
            WebCore::CanvasStyle style(color.rgb());
            if (info.property == CanvasProperty::FillStyle)
                ctx.setFillStyle(style);
            else
                ctx.setStrokeStyle(style);
            return true;
        }
        switch (info.property) {
        case CanvasProperty::LineCap: ctx.setLineCap(value); return true;
        case CanvasProperty::LineJoin: ctx.setLineJoin(value); return true;
        case CanvasProperty::ShadowColor: ctx.setShadowColor(value); return true;
        case CanvasProperty::GlobalCompositeOperation: ctx.setGlobalCompositeOperation(value); return true;
        case CanvasProperty::Font: ctx.setFont(value); return true;
        case CanvasProperty::TextAlign: ctx.setTextAlign(value); return true;
        case CanvasProperty::TextBaseline: ctx.setTextBaseline(value); return true;
        default: return false;
        }
    }
    return false;
}

// Returns this element's 2D rendering context if one was already created via
// useCanvasContext(), else nullptr. Does not create one -- that is exactly what
// useCanvasContext() is for.
static WebCore::CanvasRenderingContext2D* existingCanvas2DContext(WebCore::Element* element)
{
    if (!element || !element->hasTagName(WebCore::HTMLNames::canvasTag))
        return nullptr;
    auto& canvasElement = static_cast<WebCore::HTMLCanvasElement&>(*element);
    auto* context = canvasElement.renderingContext();
    if (!context || !context->is2d())
        return nullptr;
    return static_cast<WebCore::CanvasRenderingContext2D*>(context);
}

} // anonymous namespace

bool QWebElement::useCanvasContext(const QString& contextType)
{
    if (!m_element || !m_element->hasTagName(WebCore::HTMLNames::canvasTag))
        return false;
    auto& canvasElement = static_cast<WebCore::HTMLCanvasElement&>(*m_element);
    // getContext() is idempotent/self-caching on the element -- calling it again on
    // an already-active context just returns the same one, so there's no separate
    // cache to maintain here.
    return canvasElement.getContext(WTF::String(contextType)) != nullptr;
}

bool QWebElement::canvasAction(const QString& method, const QString& argsString)
{
    auto* ctx = existingCanvas2DContext(m_element);
    if (!ctx)
        return false;

    const CanvasVerbInfo* info = findCanvasVerb(WTF::String(method));
    if (!info)
        return false;

    Vector<WTF::String> args = splitCanvasArgs(WTF::String(argsString));
    if (args.size() < info->minArgs || args.size() > info->maxArgs)
        return false;

    return dispatchCanvasVerb(*ctx, info->verb, args);
}

bool QWebElement::canvasSetProperty(const QString& property, const QString& value)
{
    auto* ctx = existingCanvas2DContext(m_element);
    if (!ctx)
        return false;

    const CanvasPropertyInfo* info = findCanvasProperty(WTF::String(property));
    if (!info)
        return false;

    return applyCanvasProperty(*ctx, *info, WTF::String(value));
}

namespace {

// pathId is always present (possibly empty) in both isPointInPath's and isPointInStroke's
// query argument lists specifically to avoid an arity ambiguity: the real Canvas 2D
// spec's isPointInPath(x, y, winding) and isPointInPath(path, x, y) overloads can both
// take exactly 3 arguments, which isn't distinguishable by count alone. See
// HIPE_OP_CANVAS_QUERY's own doc comment for the full explanation.
static bool dispatchCanvasQuery(WebCore::CanvasRenderingContext2D& ctx, const WTF::String& queryName,
                                 const Vector<WTF::String>& args, WTF::String& outResult, WTF::String& outError)
{
    if (queryName == "isPointInPath") {
        if (args.size() != 3 && args.size() != 4) {
            outError = "isPointInPath expects 3 or 4 arguments";
            return false;
        }
        float xy[2];
        if (!parseFloats(args, 1, 2, xy)) {
            outError = "Could not parse coordinates";
            return false;
        }
        WTF::String winding = args.size() == 4 ? args[3] : WTF::String("nonzero");
        bool result;
        if (args[0].isEmpty()) {
            result = ctx.isPointInPath(xy[0], xy[1], winding);
        } else {
            auto* path = ctx.namedPath(args[0]);
            if (!path) {
                outError = "Unknown path id";
                return false;
            }
            result = ctx.isPointInPath(path, xy[0], xy[1], winding);
        }
        outResult = result ? "true" : "false";
        return true;
    }

    if (queryName == "isPointInStroke") {
        if (args.size() != 3) {
            outError = "isPointInStroke expects 3 arguments";
            return false;
        }
        float xy[2];
        if (!parseFloats(args, 1, 2, xy)) {
            outError = "Could not parse coordinates";
            return false;
        }
        bool result;
        if (args[0].isEmpty()) {
            result = ctx.isPointInStroke(xy[0], xy[1]);
        } else {
            auto* path = ctx.namedPath(args[0]);
            if (!path) {
                outError = "Unknown path id";
                return false;
            }
            result = ctx.isPointInStroke(path, xy[0], xy[1]);
        }
        outResult = result ? "true" : "false";
        return true;
    }

    if (queryName == "measureText") {
        if (args.size() != 1) {
            outError = "measureText expects 1 argument";
            return false;
        }
        auto metrics = ctx.measureText(args[0]);
        outResult = WTF::String::number(metrics->width());
        return true;
    }

    outError = "Unknown query";
    return false;
}

} // anonymous namespace

bool QWebElement::canvasQuery(const QString& queryName, const QString& argsString, QString& outResult, QString& outError)
{
    outResult.clear();
    outError.clear();

    auto* ctx = existingCanvas2DContext(m_element);
    if (!ctx) {
        outError = QStringLiteral("No 2d context selected");
        return false;
    }

    Vector<WTF::String> args = splitCanvasArgs(WTF::String(argsString));
    WTF::String result, error;
    bool ok = dispatchCanvasQuery(*ctx, WTF::String(queryName), args, result, error);
    outResult = QString(result);
    outError = QString(error);
    return ok;
}

// ---- end Canvas 2D dispatch --------------------------------------------------

// ---- GET_SRC: the read-side counterpart to setAttributeBinaryData()'s "src" case --

bool QWebElement::getSrcData(const QString& formatHint, QByteArray& outData, QString& outMimeType, QString& outError)
{
    outData.clear();
    outMimeType.clear();
    outError.clear();

    if (!m_element) {
        outError = QStringLiteral("No element");
        return false;
    }

    if (m_element->hasTagName(WebCore::HTMLNames::imgTag)) {
        auto& imageElement = static_cast<WebCore::HTMLImageElement&>(*m_element);
        WebCore::CachedImage* cachedImage = imageElement.cachedImage();
        WebCore::SharedBuffer* buffer = cachedImage ? cachedImage->resourceBuffer() : nullptr;
        if (!buffer || !buffer->size()) {
            outError = QStringLiteral("No image data set on this element");
            return false;
        }
        outData = QByteArray(buffer->data(), static_cast<int>(buffer->size()));
        outMimeType = QString(cachedImage->response().mimeType());
        return true;
    }

    if (m_element->hasTagName(WebCore::HTMLNames::canvasTag)) {
        auto& canvasElement = static_cast<WebCore::HTMLCanvasElement&>(*m_element);
        int width = canvasElement.width();
        int height = canvasElement.height();
        // Same sanity ceiling TAKE_SNAPSHOT uses -- guards against a degenerate or
        // absurd canvas size blowing up the intermediate QImage buffer (w*h*4 bytes).
        if (width <= 0 || height <= 0 || static_cast<qint64>(width) * height > 64LL * 1024 * 1024) {
            outError = QStringLiteral("Canvas has no content or an unreasonable size");
            return false;
        }

        QString format = formatHint.trimmed().toLower();
        if (format != QLatin1String("pdf"))
            format = QStringLiteral("png"); // default, and anything unrecognized

        // Render at the canvas's own pixel dimensions, not its CSS/page-scaled box --
        // this is the raw drawing-surface bitmap, matching how toDataURL() behaves in
        // every real browser (independent of how CSS has styled the element).
        QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        WebCore::GraphicsContext context(&painter);
        canvasElement.paint(context, WebCore::LayoutRect(0, 0, width, height));
        painter.end();

        QBuffer buffer(&outData);
        buffer.open(QIODevice::WriteOnly);

        if (format == QLatin1String("pdf")) {
            // Not about vector fidelity -- a canvas's backing store is already fully
            // rasterized by this point regardless of how it was drawn. This is purely
            // for print-readiness: a defined physical page size or ready-to-print
            // format is useful even for pure raster content (e.g. printing a signature
            // pad or receipt canvas), matching HIPE_OP_TAKE_SNAPSHOT's own PDF path.
            QPdfWriter pdf(&buffer);
            pdf.setResolution(96); // 1 painter unit == 1 CSS pixel, matches TAKE_SNAPSHOT
            pdf.setPageSize(QPageSize(QSizeF(width / 96.0, height / 96.0),
                                     QPageSize::Inch, QString(), QPageSize::ExactMatch));
            pdf.setPageMargins(QMarginsF(0, 0, 0, 0));
            QPainter pdfPainter(&pdf);
            pdfPainter.drawImage(0, 0, image);
            pdfPainter.end();
            outMimeType = QStringLiteral("application/pdf");
        } else {
            image.save(&buffer, "PNG");
            outMimeType = QStringLiteral("image/png");
        }
        buffer.close();

        if (outData.isEmpty()) {
            outError = QStringLiteral("Failed to encode canvas content");
            return false;
        }
        return true;
    }

    outError = QStringLiteral("Element is not an <img> or <canvas>");
    return false;
}

// ---- end GET_SRC ---------------------------------------------------------------

/*!
    Returns the tag name of this element.

    \sa geometry()
*/
QString QWebElement::tagName() const
{
    if (!m_element)
        return QString();
    return m_element->tagName();
}

/*!
    Returns the namespace prefix of the element. If the element has no\
    namespace prefix, empty string is returned.
*/
QString QWebElement::prefix() const
{
    if (!m_element)
        return QString();
    return m_element->prefix();
}

/*!
    Returns the local name of the element. If the element does not use
    namespaces, an empty string is returned.
*/
QString QWebElement::localName() const
{
    if (!m_element)
        return QString();
    return m_element->localName();
}

/*!
    Returns the namespace URI of this element. If the element has no namespace
    URI, an empty string is returned.
*/
QString QWebElement::namespaceUri() const
{
    if (!m_element)
        return QString();
    return m_element->namespaceURI();
}

/*!
    Returns the parent element of this elemen. If this element is the root
    document element, a null element is returned.
*/
QWebElement QWebElement::parent() const
{
    if (m_element)
        return QWebElement(m_element->parentElement());
    return QWebElement();
}

/*!
    Returns the element's first child.

    \sa lastChild(), previousSibling(), nextSibling()
*/
QWebElement QWebElement::firstChild() const
{
    if (!m_element)
        return QWebElement();
    for (Node* child = m_element->firstChild(); child; child = child->nextSibling()) {
        if (!child->isElementNode())
            continue;
        Element* e = downcast<Element>(child);
        return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the element's last child.

    \sa firstChild(), previousSibling(), nextSibling()
*/
QWebElement QWebElement::lastChild() const
{
    if (!m_element)
        return QWebElement();
    for (Node* child = m_element->lastChild(); child; child = child->previousSibling()) {
        if (!child->isElementNode())
            continue;
        Element* e = downcast<Element>(child);
        return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the element's next sibling.

    \sa firstChild(), previousSibling(), lastChild()
*/
QWebElement QWebElement::nextSibling() const
{
    if (!m_element)
        return QWebElement();
    for (Node* sib = m_element->nextSibling(); sib; sib = sib->nextSibling()) {
        if (!sib->isElementNode())
            continue;
        Element* e = downcast<Element>(sib);
        return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the element's previous sibling.

    \sa firstChild(), nextSibling(), lastChild()
*/
QWebElement QWebElement::previousSibling() const
{
    if (!m_element)
        return QWebElement();
    for (Node* sib = m_element->previousSibling(); sib; sib = sib->previousSibling()) {
        if (!sib->isElementNode())
            continue;
        Element* e = downcast<Element>(sib);
        return QWebElement(e);
    }
    return QWebElement();
}

/*!
    Returns the document which this element belongs to.
*/
QWebElement QWebElement::document() const
{
    if (!m_element)
        return QWebElement();
    return QWebElement(m_element->document().documentElement());
}

/*!
    Returns the web frame which this element is a part of. If the element is a
    null element, null is returned.
*/
QWebFrame *QWebElement::webFrame() const
{
    if (!m_element)
        return 0;

    Frame* frame = m_element->document().frame();
    if (!frame)
        return 0;
    QWebFrameAdapter* frameAdapter = QWebFrameAdapter::kit(frame);
    return frameAdapter->apiHandle();
}

/*!
    \enum QWebElement::StyleResolveStrategy

    This enum describes how QWebElement's styleProperty resolves the given
    property name.

    \value InlineStyle Return the property value as it is defined in
           the element, without respecting style inheritance and other CSS
           rules.
    \value CascadedStyle The property's value is determined using the
           inheritance and importance rules defined in the document's
           stylesheet.
    \value ComputedStyle The property's value is the absolute value
           of the style property resolved from the environment.
*/

/*!
    Returns the value of the style with the given \a name using the specified
    \a strategy. If a style with \a name does not exist, an empty string is
    returned.

    In CSS, the cascading part depends on which CSS rule has priority and is
    thus applied. Generally, the last defined rule has priority. Thus, an
    inline style rule has priority over an embedded block style rule, which
    in return has priority over an external style rule.

    If the "!important" declaration is set on one of those, the declaration
    receives highest priority, unless other declarations also use the
    "!important" declaration. Then, the last "!important" declaration takes
    predecence.

    \sa setStyleProperty()
*/

QString QWebElement::styleProperty(const QString &name, StyleResolveStrategy strategy) const
{
    if (!m_element || !m_element->isStyledElement())
        return QString();

    CSSPropertyID propID = cssPropertyID(name);

    if (!propID)
        return QString();

    if (strategy == InlineStyle) {
        const StyleProperties* style = static_cast<StyledElement*>(m_element)->inlineStyle();
        if (!style)
            return QString();
        return style->getPropertyValue(propID);
    }

    if (strategy == CascadedStyle) {
        const StyleProperties* style = static_cast<StyledElement*>(m_element)->inlineStyle();
        if (style && style->propertyIsImportant(propID))
            return style->getPropertyValue(propID);

        // We are going to resolve the style property by walking through the
        // list of non-inline matched CSS rules for the element, looking for
        // the highest priority definition.

        // Get an array of matched CSS rules for the given element sorted
        // by importance and inheritance order. This include external CSS
        // declarations, as well as embedded and inline style declarations.

        Document& document = m_element->document();
        Vector<RefPtr<StyleRule>> rules = document.ensureStyleResolver().styleRulesForElement(m_element, StyleResolver::AuthorCSSRules | StyleResolver::CrossOriginCSSRules);
        for (int i = rules.size(); i > 0; --i) {
            if (!rules[i - 1]->isStyleRule())
                continue;
            StyleRule* styleRule = static_cast<StyleRule*>(rules[i - 1].get());

            if (styleRule->properties().propertyIsImportant(propID))
                return styleRule->properties().getPropertyValue(propID);

            if (!style || style->getPropertyValue(propID).isEmpty())
                style = &styleRule->properties();
        }

        if (!style)
            return QString();
        return style->getPropertyValue(propID);
    }

    if (strategy == ComputedStyle) {
        if (!m_element || !m_element->isStyledElement())
            return QString();

        RefPtr<CSSComputedStyleDeclaration> style = CSSComputedStyleDeclaration::create(m_element, true);
        if (!propID || !style)
            return QString();

        return style->getPropertyValue(propID);
    }

    return QString();
}

/*!
    Sets the value of the inline style with the given \a name to \a value.

    Setting a value, does not necessarily mean that it will become the applied
    value, due to the fact that the style property's value might have been set
    earlier with a higher priority in external or embedded style declarations.

    In order to ensure that the value will be applied, you may have to append
    "!important" to the value.
*/
void QWebElement::setStyleProperty(const QString &name, const QString &value)
{
    if (!m_element || !m_element->isStyledElement())
        return;

    // Do the parsing of the token manually since WebCore isn't doing this for us anymore.
    const QLatin1String importantToken("!important");
    QString adjustedValue(value);
    bool important = false;
    if (adjustedValue.contains(importantToken)) {
        important = true;
        adjustedValue.remove(importantToken);
        adjustedValue = adjustedValue.trimmed();
    }

    CSSPropertyID propID = cssPropertyID(name);
    static_cast<StyledElement*>(m_element)->setInlineStyleProperty(propID, adjustedValue, important);
}

/*!
    Returns the list of classes of this element.
*/
// Element -> Hipe location number, for every element a QWebLocationRegistry has bound (see QWebElement::hipeLocation()).
// Holds no reference: the registry that bound the element holds it, and removes the entry when it lets go.
static HashMap<Element*, quint64>& hipeNumbers()
{
    static NeverDestroyed<HashMap<Element*, quint64>> numbers;
    return numbers;
}

quint64 QWebElement::hipeLocation() const
{
    if (!m_element)
        return 0;
    auto entry = hipeNumbers().find(m_element);
    return entry == hipeNumbers().end() ? 0 : entry->value;
}

struct QWebLocationRegistry::Private {
    struct Slot {
        RefPtr<Element> element; // null: bound to none, or released
        bool used = false;
    };
    // Dense for numbers up to about twice the count in use (client pools hand out the lowest free numbers), sparse
    // beyond, so memory follows the count in use.
    Vector<Slot> dense;
    std::map<quint64, RefPtr<Element>> sparse;
    size_t count = 0;
    size_t bindsSinceSweep = 0;

    Slot* denseSlot(quint64 n) { return n < dense.size() ? &dense[n] : nullptr; }
    void release(RefPtr<Element>& element)
    {
        if (element) {
            hipeNumbers().remove(element.get());
            element = nullptr;
        }
    }
};

QWebLocationRegistry::QWebLocationRegistry()
    : d(new Private)
{
}

QWebLocationRegistry::~QWebLocationRegistry()
{
    for (auto& slot : d->dense)
        d->release(slot.element);
    for (auto& entry : d->sparse)
        d->release(entry.second);
    delete d;
}

size_t QWebLocationRegistry::count() const
{
    return d->count;
}

bool QWebLocationRegistry::inUse(quint64 n) const
{
    if (Private::Slot* slot = d->denseSlot(n))
        return slot->used;
    return d->sparse.count(n);
}

quint64 QWebLocationRegistry::firstInUse(const Ranges& ranges) const
{
    for (auto& range : ranges) {
        for (quint64 n = range.first; n <= range.second && n < d->dense.size(); n++) {
            if (d->dense[n].used)
                return n;
        }
        auto entry = d->sparse.lower_bound(range.first);
        if (entry != d->sparse.end() && entry->first <= range.second)
            return entry->first;
    }
    return 0;
}

void QWebLocationRegistry::bind(quint64 n, const QWebElement& w)
{
    RefPtr<Element> element = w.m_element;
    if (element && hipeNumbers().contains(element.get()))
        element = nullptr; // an element has at most one number
    if (n >= d->dense.size() && n < 4096 + 2 * (d->count + 1)) // grow the dense table, at least doubling
        d->dense.resize(std::max<size_t>(n + 1, std::min<size_t>(2 * d->dense.size(), 4096 + 2 * (d->count + 1))));
    if (Private::Slot* slot = d->denseSlot(n)) {
        slot->element = element;
        slot->used = true;
    } else
        d->sparse[n] = element;
    if (element)
        hipeNumbers().set(element.get(), n);
    d->count++;
    if (++d->bindsSinceSweep > std::max<size_t>(1024, d->count / 2)) // amortised: a sweep costs O(count)
        sweep();
}

void QWebLocationRegistry::free(quint64 n)
{
    if (Private::Slot* slot = d->denseSlot(n)) {
        if (!slot->used)
            return;
        d->release(slot->element);
        slot->used = false;
    } else {
        auto entry = d->sparse.find(n);
        if (entry == d->sparse.end())
            return;
        d->release(entry->second);
        d->sparse.erase(entry);
    }
    d->count--;
}

QWebElement QWebLocationRegistry::element(quint64 n) const
{
    if (Private::Slot* slot = d->denseSlot(n))
        return QWebElement(slot->element.get());
    auto entry = d->sparse.find(n);
    return entry == d->sparse.end() ? QWebElement() : QWebElement(entry->second.get());
}

// Releases the elements only this registry still references: out of any tree (a node in a tree is kept alive by its
// parent), with no reference but ours. Releasing one can free its children for the next pass. Run every so often from
// bind(), so its cost is spread over the binds.
void QWebLocationRegistry::sweep()
{
    d->bindsSinceSweep = 0;
    auto unreferenced = [](const RefPtr<Element>& element) {
        return element && !element->parentNode() && element->refCount() == 1;
    };
    bool released;
    do {
        released = false;
        for (auto& slot : d->dense) {
            if (unreferenced(slot.element)) {
                d->release(slot.element);
                released = true;
            }
        }
        for (auto& entry : d->sparse) {
            if (unreferenced(entry.second)) {
                d->release(entry.second);
                released = true;
            }
        }
    } while (released);
}

static bool listedIn(const QWebLocationRegistry::Ranges& ranges, quint64 n)
{
    auto range = std::upper_bound(ranges.begin(), ranges.end(), n,
        [](quint64 value, const QPair<quint64, quint64>& r) { return value < r.first; });
    return range != ranges.begin() && n <= (range - 1)->second;
}

QWebLocationRegistry::MarkupResult QWebLocationRegistry::bindMarkup(const QList<QPair<QWebElement, QString>>& carriers,
    const Ranges& listed)
{
    MarkupResult result;
    if (listed.isEmpty())
        return result;
    for (auto& carrier : carriers) {
        const QString& value = carrier.second;
        bool digits = !value.isEmpty() && value.size() <= 20;
        for (QChar c : value)
            digits = digits && c >= QLatin1Char('0') && c <= QLatin1Char('9');
        bool ok = false;
        quint64 n = digits ? value.toULongLong(&ok) : 0;
        if (!ok || !listedIn(listed, n))
            continue; // not a number the client listed: ignored
        if (inUse(n)) { // listed numbers were all free, so another carrier took it
            if (result.duplicates.size() < 5)
                result.duplicates.append(n);
            continue;
        }
        bind(n, carrier.first);
    }
    for (auto& range : listed) {
        for (quint64 n = range.first; ; n++) {
            if (!inUse(n)) {
                bind(n, QWebElement());
                if (result.unbound.size() < 5)
                    result.unbound.append(n);
                result.unboundCount++;
            }
            if (n == range.second)
                break;
        }
    }
    return result;
}

// The namespace and qualified name the HTML parser would give a tag created inside parent.
static void parserNameFor(Element& parent, const QString& tag, AtomicString& namespaceURI, String& name)
{
    String lowered = String(tag).convertToASCIILowercase();
    const AtomicString& parentNamespace = parent.namespaceURI();
    bool htmlIntegrationPoint = (parentNamespace == SVGNames::svgNamespaceURI
            && (parent.hasLocalName(SVGNames::foreignObjectTag.localName()) || parent.hasLocalName(SVGNames::descTag.localName())
                || parent.hasLocalName(SVGNames::titleTag.localName())))
        || (parentNamespace == MathMLNames::mathmlNamespaceURI
            && (parent.hasLocalName(MathMLNames::miTag.localName()) || parent.hasLocalName(MathMLNames::moTag.localName())
                || parent.hasLocalName(MathMLNames::mnTag.localName()) || parent.hasLocalName(MathMLNames::msTag.localName())
                || parent.hasLocalName(MathMLNames::mtextTag.localName()) || parent.hasLocalName(MathMLNames::annotation_xmlTag.localName())));
    if (lowered == "svg")
        namespaceURI = SVGNames::svgNamespaceURI;
    else if (lowered == "math")
        namespaceURI = MathMLNames::mathmlNamespaceURI;
    else if (!htmlIntegrationPoint && (parentNamespace == SVGNames::svgNamespaceURI || parentNamespace == MathMLNames::mathmlNamespaceURI))
        namespaceURI = parentNamespace;
    else
        namespaceURI = HTMLNames::xhtmlNamespaceURI;

    name = lowered;
    if (namespaceURI == SVGNames::svgNamespaceURI) {
        static NeverDestroyed<HashMap<AtomicString, AtomicString>> svgCase = [] {
            HashMap<AtomicString, AtomicString> map;
            const SVGQualifiedName* const* tags = SVGNames::getSVGTags();
            for (unsigned i = 0; i < SVGNames::SVGTagsCount; ++i) {
                const AtomicString& localName = tags[i]->localName();
                AtomicString lowerName = localName.convertToASCIILowercase();
                if (lowerName != localName)
                    map.add(lowerName, localName);
            }
            return map;
        }();
        AtomicString cased = svgCase.get().get(AtomicString(lowered));
        if (!cased.isNull())
            name = cased;
    }
}

static bool isVoidHTMLElement(const Element& element)
{
    static const char* const voidTags[] = { "area", "base", "br", "col", "embed", "hr", "img", "input", "keygen", "link",
        "meta", "param", "source", "track", "wbr" };
    if (!element.isHTMLElement())
        return false;
    for (const char* tag : voidTags) {
        if (element.localName() == tag)
            return true;
    }
    return false;
}

static RefPtr<Element> createDirectly(Element& parent, const QString& tag, const QString& id, const QString& classes,
    const QString& text)
{
    AtomicString namespaceURI;
    String name;
    parserNameFor(parent, tag, namespaceURI, name);
    ExceptionCode exception = 0;
    RefPtr<Element> element = parent.document().createElementNS(namespaceURI, name, exception);
    if (exception || !element)
        return nullptr;
    if (!id.isEmpty())
        element->setAttribute(HTMLNames::idAttr, id);
    if (!classes.isEmpty())
        element->setAttribute(HTMLNames::classAttr, classes);
    if (!text.isEmpty() && !isVoidHTMLElement(*element)) {
        QString normalised = text;
        normalised.replace(QLatin1String("\r\n"), QLatin1String("\n")).replace(QLatin1Char('\r'), QLatin1Char('\n'));
        element->setTextContent(normalised, exception);
    }
    return element;
}

QWebElement QWebElement::appendNewElement(const QString& tag, const QString& id, const QString& classes, const QString& text)
{
    if (!m_element)
        return QWebElement();
    RefPtr<Element> element = createDirectly(*m_element, tag, id, classes, text);
    if (!element)
        return QWebElement();
    ExceptionCode exception = 0;
    m_element->appendChild(*element, exception);
    return exception ? QWebElement() : QWebElement(element.get());
}

QWebElement QWebElement::insertNewElementBefore(const QString& tag, const QString& id, const QString& classes, const QString& text)
{
    if (!m_element || !m_element->parentElement())
        return QWebElement();
    Element& parent = *m_element->parentElement();
    RefPtr<Element> element = createDirectly(parent, tag, id, classes, text);
    if (!element)
        return QWebElement();
    ExceptionCode exception = 0;
    parent.insertBefore(*element, m_element, exception);
    return exception ? QWebElement() : QWebElement(element.get());
}

QStringList QWebElement::classes() const
{
    if (!hasAttribute(QLatin1String("class")))
        return QStringList();

    QStringList classes =  attribute(QLatin1String("class")).simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    classes.removeDuplicates();
    return classes;
}

/*!
    Returns true if this element has a class with the given \a name; otherwise
    returns false.
*/
bool QWebElement::hasClass(const QString &name) const
{
    QStringList list = classes();
    return list.contains(name);
}

/*!
    Adds the specified class with the given \a name to the element.
*/
void QWebElement::addClass(const QString &name)
{
    QStringList list = classes();
    if (!list.contains(name)) {
        list.append(name);
        QString value = list.join(QLatin1String(" "));
        setAttribute(QLatin1String("class"), value);
    }
}

/*!
    Removes the specified class with the given \a name from the element.
*/
void QWebElement::removeClass(const QString &name)
{
    QStringList list = classes();
    if (list.contains(name)) {
        list.removeAll(name);
        QString value = list.join(QLatin1String(" "));
        setAttribute(QLatin1String("class"), value);
    }
}

/*!
    Adds the specified class with the given \a name if it is not present. If
    the class is already present, it will be removed.
*/
void QWebElement::toggleClass(const QString &name)
{
    QStringList list = classes();
    if (list.contains(name))
        list.removeAll(name);
    else
        list.append(name);

    QString value = list.join(QLatin1String(" "));
    setAttribute(QLatin1String("class"), value);
}

/*!
    Appends the given \a element as the element's last child.

    If \a element is the child of another element, it is re-parented to this
    element. If \a element is a child of this element, then its position in
    the list of children is changed.

    Calling this function on a null element does nothing.

    \sa prependInside(), prependOutside(), appendOutside()
*/
void QWebElement::appendInside(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    ExceptionCode exception = 0;
    m_element->appendChild(*element.m_element, exception);
}

/*!
    Appends the result of parsing \a markup as the element's last child.

    If \a markup is plain text and the element's last child is a text node
    shorter than 64K characters, the text is added to that node instead of
    becoming a new one.

    Calling this function on a null element does nothing.

    \sa prependInside(), prependOutside(), appendOutside()
*/
void QWebElement::appendInside(const QString &markup)
{
    appendInside(markup, nullptr);
}

void QWebElement::appendInside(const QString &markup, QList<QPair<QWebElement, QString>>* hipeLocations)
{
    if (!m_element)
        return;

    //createFragmentForInnerOuterHTML() (the namespace-aware fragment parser Element::setInnerHTML()
    //itself uses) is generic over Element*, unlike the narrower Range-oriented
    //createContextualFragment(HTMLElement*) this used to call - that's what let the old
    //isHTMLElement() guard here silently no-op for SVG (and other non-HTML) targets. The
    //ieForbidsInsertHTML() check that guarded void HTML elements (<img>, <br>, <input>, ...)
    //lived inside that narrower function, so it's preserved explicitly here, HTML-only.
    if (is<HTMLElement>(*m_element) && downcast<HTMLElement>(*m_element).ieForbidsInsertHTML())
        return;

    ExceptionCode exception = 0;
    RefPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(markup, m_element, AllowScriptingContent, exception);
    if (!fragment)
        return;
    for (auto& carrier : takeHipeLocations(*fragment)) {
        if (hipeLocations)
            hipeLocations->append(qMakePair(QWebElement(carrier.first.ptr()), QString(carrier.second)));
    }

    // Plain text appended after a text node is added to that node rather than inserted as a new one.
    // Every inserted node makes WebCore look back for the nearest preceding element sibling
    // (ContainerNode::notifyChildInserted), stepping over each text node on the way, so appending text
    // to the same element again and again became O(n) per append. Merging stops at the parser's own
    // text node length limit, since appendData() copies the node's whole text each time.
    Node* last = m_element->lastChild();
    Node* added = fragment->firstChild();
    if (last && added && !added->nextSibling() && last->nodeType() == Node::TEXT_NODE
        && added->nodeType() == Node::TEXT_NODE && downcast<Text>(*last).length() < Text::defaultLengthLimit) {
        downcast<Text>(*last).appendData(downcast<Text>(*added).data());
        return;
    }

    m_element->appendChild(*fragment, exception);
}

/*!
    Prepends \a element as the element's first child.

    If \a element is the child of another element, it is re-parented to this
    element. If \a element is a child of this element, then its position in
    the list of children is changed.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependOutside(), appendOutside()
*/
void QWebElement::prependInside(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    ExceptionCode exception = 0;

    if (m_element->hasChildNodes())
        m_element->insertBefore(*element.m_element, m_element->firstChild(), exception);
    else
        m_element->appendChild(*element.m_element, exception);
}

/*!
    Prepends the result of parsing \a markup as the element's first child.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependOutside(), appendOutside()
*/
void QWebElement::prependInside(const QString &markup)
{
    if (!m_element)
        return;

    if (is<HTMLElement>(*m_element) && downcast<HTMLElement>(*m_element).ieForbidsInsertHTML())
        return;

    ExceptionCode exception = 0;
    RefPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(markup, m_element, AllowScriptingContent, exception);
    if (!fragment)
        return;

    if (m_element->hasChildNodes())
        m_element->insertBefore(*fragment, m_element->firstChild(), exception);
    else
        m_element->appendChild(*fragment, exception);
}


/*!
    Inserts the given \a element before this element.

    If \a element is the child of another element, it is re-parented to the
    parent of this element.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependInside(), appendOutside()
*/
void QWebElement::prependOutside(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    if (!m_element->parentNode())
        return;

    ExceptionCode exception = 0;
    m_element->parentNode()->insertBefore(*element.m_element, m_element, exception);
}

/*!
    Inserts the result of parsing \a markup before this element.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependInside(), appendOutside()
*/
void QWebElement::prependOutside(const QString &markup)
{
    if (!m_element)
        return;

    Node* parent = m_element->parentNode();
    if (!parent || !is<Element>(*parent))
        return;

    if (is<HTMLElement>(*parent) && downcast<HTMLElement>(*parent).ieForbidsInsertHTML())
        return;

    ExceptionCode exception = 0;
    RefPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(markup, &downcast<Element>(*parent), AllowScriptingContent, exception);
    if (!fragment)
        return;
    takeHipeLocations(*fragment);

    parent->insertBefore(fragment, m_element, exception);
}

/*!
    Inserts the given \a element after this element.

    If \a element is the child of another element, it is re-parented to the
    parent of this element.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependInside(), prependOutside()
*/
void QWebElement::appendOutside(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    if (!m_element->parentNode())
        return;

    ExceptionCode exception = 0;
    if (!m_element->nextSibling())
        m_element->parentNode()->appendChild(*element.m_element, exception);
    else
        m_element->parentNode()->insertBefore(*element.m_element, m_element->nextSibling(), exception);
}

/*!
    Inserts the result of parsing \a markup after this element.

    Calling this function on a null element does nothing.

    \sa appendInside(), prependInside(), prependOutside()
*/
void QWebElement::appendOutside(const QString &markup)
{
    if (!m_element)
        return;

    Node* parent = m_element->parentNode();
    if (!parent || !is<Element>(*parent))
        return;

    if (is<HTMLElement>(*parent) && downcast<HTMLElement>(*parent).ieForbidsInsertHTML())
        return;

    ExceptionCode exception = 0;
    RefPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(markup, &downcast<Element>(*parent), AllowScriptingContent, exception);

    if (!m_element->nextSibling())
        parent->appendChild(fragment, exception);
    else
        parent->insertBefore(fragment, m_element->nextSibling(), exception);
}

/*!
    Returns a clone of this element.

    The clone may be inserted at any point in the document.

    \sa appendInside(), prependInside(), prependOutside(), appendOutside()
*/
QWebElement QWebElement::clone() const
{
    if (!m_element)
        return QWebElement();

    // FIXME: Do we need to add document argument? What is use case for cloning to different document?
    return QWebElement(&m_element->cloneElementWithChildren(m_element->document()).get());
}

/*!
    Removes this element from the document and returns a reference to it.

    The element is still valid after removal, and can be inserted into other
    parts of the document.

    \sa removeAllChildren(), removeFromDocument()
*/
QWebElement &QWebElement::takeFromDocument()
{
    if (!m_element)
        return *this;

    ExceptionCode exception = 0;
    m_element->remove(exception);

    return *this;
}

/*!
    Removes this element from the document and makes it a null element.

    \sa removeAllChildren(), takeFromDocument()
*/
void QWebElement::removeFromDocument()
{
    if (!m_element)
        return;

    ExceptionCode exception = 0;
    m_element->remove(exception);
    m_element->deref();
    m_element = 0;
}

/*!
    Removes all children from this element.

    \sa removeFromDocument(), takeFromDocument()
*/
void QWebElement::removeAllChildren()
{
    if (!m_element)
        return;

    m_element->removeChildren();
}

// FIXME: This code, and all callers are wrong, and have no place in a
// WebKit implementation.  These should be replaced with WebCore implementations.
static RefPtr<Node> findInsertionPoint(PassRefPtr<Node> root)
{
    RefPtr<Node> node = root;

    // Go as far down the tree as possible.
    while (node->hasChildNodes() && node->firstChild()->isElementNode())
        node = node->firstChild();

    // TODO: Implement SVG support
    if (node->isHTMLElement()) {
        HTMLElement* element = static_cast<HTMLElement*>(node.get());

        // The insert point could be a non-enclosable tag and it can thus
        // never have children, so go one up. Get the parent element, and not
        // note as a root note will always exist.
        if (element->ieForbidsInsertHTML())
            node = node->parentElement();
    }

    return node;
}

/*!
    Encloses the contents of this element with \a element. This element becomes
    the child of the deepest descendant within \a element.

    ### illustration

    \sa encloseWith()
*/
void QWebElement::encloseContentsWith(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    RefPtr<Node> insertionPoint = findInsertionPoint(element.m_element);

    if (!insertionPoint)
        return;

    ExceptionCode exception = 0;

    // reparent children
    for (RefPtr<Node> child = m_element->firstChild(); child;) {
        RefPtr<Node> next = child->nextSibling();
        insertionPoint->appendChild(child, exception);
        child = next;
    }

    if (m_element->hasChildNodes())
        m_element->insertBefore(*element.m_element, m_element->firstChild(), exception);
    else
        m_element->appendChild(*element.m_element, exception);
}

/*!
    Encloses the contents of this element with the result of parsing \a markup.
    This element becomes the child of the deepest descendant within \a markup.

    \sa encloseWith()
*/
void QWebElement::encloseContentsWith(const QString &markup)
{
    if (!m_element)
        return;

    if (!m_element->parentNode())
        return;

    if (is<HTMLElement>(*m_element) && downcast<HTMLElement>(*m_element).ieForbidsInsertHTML())
        return;

    ExceptionCode exception = 0;
    RefPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(markup, m_element, AllowScriptingContent, exception);

    if (!fragment || !fragment->firstChild())
        return;

    RefPtr<Node> insertionPoint = findInsertionPoint(fragment->firstChild());

    if (!insertionPoint)
        return;

    // reparent children
    for (RefPtr<Node> child = m_element->firstChild(); child;) {
        RefPtr<Node> next = child->nextSibling();
        insertionPoint->appendChild(child, exception);
        child = next;
    }

    if (m_element->hasChildNodes())
        m_element->insertBefore(*fragment, m_element->firstChild(), exception);
    else
        m_element->appendChild(*fragment, exception);
}

/*!
    Encloses this element with \a element. This element becomes the child of
    the deepest descendant within \a element.

    \sa replace()
*/
void QWebElement::encloseWith(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    RefPtr<Node> insertionPoint = findInsertionPoint(element.m_element);

    if (!insertionPoint)
        return;

    // Keep reference to these two nodes before pulling out this element and
    // wrapping it in the fragment. The reason for doing it in this order is
    // that once the fragment has been added to the document it is empty, so
    // we no longer have access to the nodes it contained.
    Node* parent = m_element->parentNode();
    Node* siblingNode = m_element->nextSibling();

    ExceptionCode exception = 0;
    insertionPoint->appendChild(m_element, exception);

    if (!siblingNode)
        parent->appendChild(element.m_element, exception);
    else
        parent->insertBefore(element.m_element, siblingNode, exception);
}

/*!
    Encloses this element with the result of parsing \a markup. This element
    becomes the child of the deepest descendant within \a markup.

    \sa replace()
*/
void QWebElement::encloseWith(const QString &markup)
{
    if (!m_element)
        return;

    Node* parent = m_element->parentNode();
    if (!parent || !is<Element>(*parent))
        return;

    if (is<HTMLElement>(*parent) && downcast<HTMLElement>(*parent).ieForbidsInsertHTML())
        return;

    ExceptionCode exception = 0;
    RefPtr<DocumentFragment> fragment = createFragmentForInnerOuterHTML(markup, &downcast<Element>(*parent), AllowScriptingContent, exception);

    if (!fragment || !fragment->firstChild())
        return;

    RefPtr<Node> insertionPoint = findInsertionPoint(fragment->firstChild());

    if (!insertionPoint)
        return;

    // Keep reference to parent & siblingNode before pulling out this element and
    // wrapping it in the fragment. The reason for doing it in this order is
    // that once the fragment has been added to the document it is empty, so
    // we no longer have access to the nodes it contained.
    Node* siblingNode = m_element->nextSibling();

    insertionPoint->appendChild(m_element, exception);

    if (!siblingNode)
        parent->appendChild(fragment, exception);
    else
        parent->insertBefore(fragment, siblingNode, exception);
}

/*!
    Replaces this element with \a element.

    This method will not replace the <html>, <head> or <body> elements.

    \sa encloseWith()
*/
void QWebElement::replace(const QWebElement &element)
{
    if (!m_element || element.isNull())
        return;

    appendOutside(element);
    takeFromDocument();
}

/*!
    Replaces this element with the result of parsing \a markup.

    This method will not replace the <html>, <head> or <body> elements.

    \sa encloseWith()
*/
void QWebElement::replace(const QString &markup)
{
    if (!m_element)
        return;

    appendOutside(markup);
    takeFromDocument();
}

/*!
    \fn inline bool QWebElement::operator==(const QWebElement& o) const;

    Returns true if this element points to the same underlying DOM object as
    \a o; otherwise returns false.
*/

/*!
    \fn inline bool QWebElement::operator!=(const QWebElement& o) const;

    Returns true if this element points to a different underlying DOM object
    than \a o; otherwise returns false.
*/


/*! 
  Render the element into \a painter .
*/
void QWebElement::render(QPainter* painter)
{
    render(painter, QRect());
}

/*!
  Render the element into \a painter clipping to \a clip.
*/
void QWebElement::render(QPainter* painter, const QRect& clip)
{
    WebCore::Element* e = m_element;
    if (!e)
        return;

    auto* renderer = e->renderer();
    if (!renderer)
        return;

    Frame* frame = e->document().frame();
    if (!frame || !frame->view() || !frame->contentRenderer())
        return;

    FrameView* view = frame->view();

    view->updateLayoutAndStyleIfNeededRecursive();

    IntRect rect = renderer->absoluteBoundingBoxRect();

    if (rect.isEmpty())
        return;

    QRect finalClipRect = rect;
    if (!clip.isEmpty())
        rect.intersect(clip.translated(rect.location()));

    GraphicsContext context(painter);

    context.save();
    context.translate(-rect.x(), -rect.y());
    painter->setClipRect(finalClipRect, Qt::IntersectClip);
    view->setNodeToDraw(e);
    view->paintContents(context, finalClipRect);
    view->setNodeToDraw(0);
    context.restore();
}

class QWebElementCollectionPrivate : public QSharedData
{
public:
    static QWebElementCollectionPrivate* create(const PassRefPtr<ContainerNode> &context, const QString &query);

    RefPtr<NodeList> m_result;

private:
    inline QWebElementCollectionPrivate() {}
};

QWebElementCollectionPrivate* QWebElementCollectionPrivate::create(const PassRefPtr<ContainerNode> &context, const QString &query)
{
    if (!context)
        return 0;

    // Let WebKit do the hard work hehehe
    ExceptionCode exception = 0; // ###
    RefPtr<NodeList> nodes = context->querySelectorAll(query, exception);
    if (!nodes)
        return 0;

    QWebElementCollectionPrivate* priv = new QWebElementCollectionPrivate;
    priv->m_result = nodes;
    return priv;
}

/*!
    \class QWebElementCollection
    \inmodule QtWebKit
    \since 4.6
    \brief The QWebElementCollection class represents a collection of web elements.
    \preliminary

    Elements in a document can be selected using QWebElement::findAll() or using the
    QWebElement constructor. The collection is composed by choosing all elements in the
    document that match a specified CSS selector expression.

    The number of selected elements is provided through the count() property. Individual
    elements can be retrieved by index using at().

    It is also possible to iterate through all elements in the collection using Qt's foreach
    macro:

    \code
        QWebElementCollection collection = document.findAll("p");
        foreach (QWebElement paraElement, collection) {
            ...
        }
    \endcode
*/

/*!
    Constructs an empty collection.
*/
QWebElementCollection::QWebElementCollection()
{
}

/*!
    Constructs a copy of \a other.
*/
QWebElementCollection::QWebElementCollection(const QWebElementCollection &other)
    : d(other.d)
{
}

/*!
    Constructs a collection of elements from the list of child elements of \a contextElement that
    match the specified CSS selector \a query.
*/
QWebElementCollection::QWebElementCollection(const QWebElement &contextElement, const QString &query)
{
    d = QExplicitlySharedDataPointer<QWebElementCollectionPrivate>(QWebElementCollectionPrivate::create(contextElement.m_element, query));
}

/*!
    Assigns \a other to this collection and returns a reference to this collection.
*/
QWebElementCollection &QWebElementCollection::operator=(const QWebElementCollection &other)
{
    d = other.d;
    return *this;
}

/*!
    Destroys the collection.
*/
QWebElementCollection::~QWebElementCollection()
{
}

/*! \fn QWebElementCollection &QWebElementCollection::operator+=(const QWebElementCollection &other)

    Appends the items of the \a other list to this list and returns a
    reference to this list.

    \sa operator+(), append()
*/

/*!
    Returns a collection that contains all the elements of this collection followed
    by all the elements in the \a other collection. Duplicates may occur in the result.

    \sa operator+=()
*/
QWebElementCollection QWebElementCollection::operator+(const QWebElementCollection &other) const
{
    QWebElementCollection n = *this; n.d.detach(); n += other; return n;
}

/*!
    Extends the collection by appending all items of \a other.

    The resulting collection may include duplicate elements.

    \sa operator+=()
*/
void QWebElementCollection::append(const QWebElementCollection &other)
{
    if (!d) {
        *this = other;
        return;
    }
    if (!other.d)
        return;
    Vector<Ref<Node>> nodes;
    RefPtr<NodeList> results[] = { d->m_result, other.d->m_result };
    nodes.reserveInitialCapacity(results[0]->length() + results[1]->length());

    for (int i = 0; i < 2; ++i) {
        int j = 0;
        Node* n = results[i]->item(j);
        while (n) {
            nodes.append(*n);
            n = results[i]->item(++j);
        }
    }

    d->m_result = StaticNodeList::adopt(nodes);
}

/*!
    Returns the number of elements in the collection.
*/
int QWebElementCollection::count() const
{
    if (!d)
        return 0;
    return d->m_result->length();
}

/*!
    Returns the element at index position \a i in the collection.
*/
QWebElement QWebElementCollection::at(int i) const
{
    if (!d)
        return QWebElement();
    Node* n = d->m_result->item(i);
    return QWebElement(downcast<Element>(n));
}

/*!
    \fn const QWebElement QWebElementCollection::operator[](int position) const

    Returns the element at the specified \a position in the collection.
*/

/*! \fn QWebElement QWebElementCollection::first() const

    Returns the first element in the collection.

    \sa last(), operator[](), at(), count()
*/

/*! \fn QWebElement QWebElementCollection::last() const

    Returns the last element in the collection.

    \sa first(), operator[](), at(), count()
*/

/*!
    Returns a QList object with the elements contained in this collection.
*/
QList<QWebElement> QWebElementCollection::toList() const
{
    if (!d)
        return QList<QWebElement>();
    QList<QWebElement> elements;
    int i = 0;
    Node* n = d->m_result->item(i);
    while (n) {
        if (n->isElementNode())
            elements.append(QWebElement(downcast<Element>(n)));
        n = d->m_result->item(++i);
    }
    return elements;
}

/*!
    \fn QWebElementCollection::const_iterator QWebElementCollection::begin() const

    Returns an STL-style iterator pointing to the first element in the collection.

    \sa end()
*/

/*!
    \fn QWebElementCollection::const_iterator QWebElementCollection::end() const

    Returns an STL-style iterator pointing to the imaginary element after the
    last element in the list.

    \sa begin()
*/

/*!
    \class QWebElementCollection::const_iterator
    \inmodule QtWebKit
    \since 4.6
    \brief The QWebElementCollection::const_iterator class provides an STL-style const iterator for QWebElementCollection.

    QWebElementCollection provides STL style const iterators for fast low-level access to the elements.

    QWebElementCollection::const_iterator allows you to iterate over a QWebElementCollection.
*/

/*!
    \fn QWebElementCollection::const_iterator::const_iterator(const const_iterator &other)

    Constructs a copy of \a other.
*/

/*!
    \fn QWebElementCollection::const_iterator::const_iterator(const QWebElementCollection *collection, int index)
    \internal
*/

/*!
    \fn const QWebElement QWebElementCollection::const_iterator::operator*() const

    Returns the current element.
*/

/*!
    \fn bool QWebElementCollection::const_iterator::operator==(const const_iterator &other) const

    Returns true if \a other points to the same item as this iterator;
    otherwise returns false.

    \sa operator!=()
*/

/*!
    \fn bool QWebElementCollection::const_iterator::operator!=(const const_iterator &other) const

    Returns true if \a other points to a different element than this;
    iterator; otherwise returns false.

    \sa operator==()
*/

/*!
    \fn QWebElementCollection::const_iterator &QWebElementCollection::const_iterator::operator++()

    The prefix ++ operator (\c{++it}) advances the iterator to the next element in the collection
    and returns an iterator to the new current element.

    Calling this function on QWebElementCollection::end() leads to undefined results.

    \sa operator--()
*/

/*!
    \fn QWebElementCollection::const_iterator QWebElementCollection::const_iterator::operator++(int)

    \overload

    The postfix ++ operator (\c{it++}) advances the iterator to the next element in the collection
    and returns an iterator to the previously current element.

    Calling this function on QWebElementCollection::end() leads to undefined results.
*/

/*!
    \fn QWebElementCollection::const_iterator &QWebElementCollection::const_iterator::operator--()

    The prefix -- operator (\c{--it}) makes the preceding element current and returns an
    iterator to the new current element.

    Calling this function on QWebElementCollection::begin() leads to undefined results.

    \sa operator++()
*/

/*!
    \fn QWebElementCollection::const_iterator QWebElementCollection::const_iterator::operator--(int)

    \overload

    The postfix -- operator (\c{it--}) makes the preceding element current and returns
    an iterator to the previously current element.
*/

/*!
    \fn QWebElementCollection::const_iterator &QWebElementCollection::const_iterator::operator+=(int j)

    Advances the iterator by \a j elements. If \a j is negative, the iterator goes backward.

    \sa operator-=(), operator+()
*/

/*!
    \fn QWebElementCollection::const_iterator &QWebElementCollection::const_iterator::operator-=(int j)

    Makes the iterator go back by \a j elements. If \a j is negative, the iterator goes forward.

    \sa operator+=(), operator-()
*/

/*!
    \fn QWebElementCollection::const_iterator QWebElementCollection::const_iterator::operator+(int j) const

    Returns an iterator to the element at \a j positions forward from this iterator. If \a j
    is negative, the iterator goes backward.

    \sa operator-(), operator+=()
*/

/*!
    \fn QWebElementCollection::const_iterator QWebElementCollection::const_iterator::operator-(int j) const

    Returns an iterator to the element at \a j positiosn backward from this iterator.
    If \a j is negative, the iterator goes forward.

    \sa operator+(), operator-=()
*/

/*!
    \fn int QWebElementCollection::const_iterator::operator-(const_iterator other) const

    Returns the number of elements between the item point to by \a other
    and the element pointed to by this iterator.
*/

/*!
    \fn bool QWebElementCollection::const_iterator::operator<(const const_iterator &other) const

    Returns true if the element pointed to by this iterator is less than the element pointed to
    by the \a other iterator.
*/

/*!
    \fn bool QWebElementCollection::const_iterator::operator<=(const const_iterator &other) const

    Returns true if the element pointed to by this iterator is less than or equal to the
    element pointed to by the \a other iterator.
*/

/*!
    \fn bool QWebElementCollection::const_iterator::operator>(const const_iterator &other) const

    Returns true if the element pointed to by this iterator is greater than the element pointed to
    by the \a other iterator.
*/

/*!
    \fn bool QWebElementCollection::const_iterator::operator>=(const const_iterator &other) const

    Returns true if the element pointed to by this iterator is greater than or equal to the
    element pointed to by the \a other iterator.
*/

/*!
    \fn QWebElementCollection::iterator QWebElementCollection::begin()

    Returns an STL-style iterator pointing to the first element in the collection.

    \sa end()
*/

/*!
    \fn QWebElementCollection::iterator QWebElementCollection::end()

    Returns an STL-style iterator pointing to the imaginary element after the
    last element in the list.

    \sa begin()
*/

/*!
    \fn QWebElementCollection::const_iterator QWebElementCollection::constBegin() const

    Returns an STL-style iterator pointing to the first element in the collection.

    \sa end()
*/

/*!
    \fn QWebElementCollection::const_iterator QWebElementCollection::constEnd() const

    Returns an STL-style iterator pointing to the imaginary element after the
    last element in the list.

    \sa begin()
*/

/*!
    \class QWebElementCollection::iterator
    \inmodule QtWebKit
    \since 4.6
    \brief The QWebElementCollection::iterator class provides an STL-style iterator for QWebElementCollection.

    QWebElementCollection provides STL style iterators for fast low-level access to the elements.

    QWebElementCollection::iterator allows you to iterate over a QWebElementCollection.
*/

/*!
    \fn QWebElementCollection::iterator::iterator(const iterator &other)

    Constructs a copy of \a other.
*/

/*!
    \fn QWebElementCollection::iterator::iterator(const QWebElementCollection *collection, int index)
    \internal
*/

/*!
    \fn const QWebElement QWebElementCollection::iterator::operator*() const

    Returns the current element.
*/

/*!
    \fn bool QWebElementCollection::iterator::operator==(const iterator &other) const

    Returns true if \a other points to the same item as this iterator;
    otherwise returns false.

    \sa operator!=()
*/

/*!
    \fn bool QWebElementCollection::iterator::operator!=(const iterator &other) const

    Returns true if \a other points to a different element than this;
    iterator; otherwise returns false.

    \sa operator==()
*/

/*!
    \fn QWebElementCollection::iterator &QWebElementCollection::iterator::operator++()

    The prefix ++ operator (\c{++it}) advances the iterator to the next element in the collection
    and returns an iterator to the new current element.

    Calling this function on QWebElementCollection::end() leads to undefined results.

    \sa operator--()
*/

/*!
    \fn QWebElementCollection::iterator QWebElementCollection::iterator::operator++(int)

    \overload

    The postfix ++ operator (\c{it++}) advances the iterator to the next element in the collection
    and returns an iterator to the previously current element.

    Calling this function on QWebElementCollection::end() leads to undefined results.
*/

/*!
    \fn QWebElementCollection::iterator &QWebElementCollection::iterator::operator--()

    The prefix -- operator (\c{--it}) makes the preceding element current and returns an
    iterator to the new current element.

    Calling this function on QWebElementCollection::begin() leads to undefined results.

    \sa operator++()
*/

/*!
    \fn QWebElementCollection::iterator QWebElementCollection::iterator::operator--(int)

    \overload

    The postfix -- operator (\c{it--}) makes the preceding element current and returns
    an iterator to the previously current element.
*/

/*!
    \fn QWebElementCollection::iterator &QWebElementCollection::iterator::operator+=(int j)

    Advances the iterator by \a j elements. If \a j is negative, the iterator goes backward.

    \sa operator-=(), operator+()
*/

/*!
    \fn QWebElementCollection::iterator &QWebElementCollection::iterator::operator-=(int j)

    Makes the iterator go back by \a j elements. If \a j is negative, the iterator goes forward.

    \sa operator+=(), operator-()
*/

/*!
    \fn QWebElementCollection::iterator QWebElementCollection::iterator::operator+(int j) const

    Returns an iterator to the element at \a j positions forward from this iterator. If \a j
    is negative, the iterator goes backward.

    \sa operator-(), operator+=()
*/

/*!
    \fn QWebElementCollection::iterator QWebElementCollection::iterator::operator-(int j) const

    Returns an iterator to the element at \a j positiosn backward from this iterator.
    If \a j is negative, the iterator goes forward.

    \sa operator+(), operator-=()
*/

/*!
    \fn int QWebElementCollection::iterator::operator-(iterator other) const

    Returns the number of elements between the item point to by \a other
    and the element pointed to by this iterator.
*/

/*!
    \fn bool QWebElementCollection::iterator::operator<(const iterator &other) const

    Returns true if the element pointed to by this iterator is less than the element pointed to
    by the \a other iterator.
*/

/*!
    \fn bool QWebElementCollection::iterator::operator<=(const iterator &other) const

    Returns true if the element pointed to by this iterator is less than or equal to the
    element pointed to by the \a other iterator.
*/

/*!
    \fn bool QWebElementCollection::iterator::operator>(const iterator &other) const

    Returns true if the element pointed to by this iterator is greater than the element pointed to
    by the \a other iterator.
*/

/*!
    \fn bool QWebElementCollection::iterator::operator>=(const iterator &other) const

    Returns true if the element pointed to by this iterator is greater than or equal to the
    element pointed to by the \a other iterator.
*/

