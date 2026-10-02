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

#ifndef QWEBELEMENT_H
#define QWEBELEMENT_H

#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qrect.h>
#include <QtCore/qvariant.h>
#include <QtCore/qvector.h>
#include <QtCore/qshareddata.h>

#include "qwebkitglobal.h"

namespace WebCore {
class ChromeClientQt;
class Element;
class Node;
class HTMLMediaElement;

//class HipeCoreEventListener;
}
//namespace WTF {
//    template<typename T> class RefPtr;
//}


QT_BEGIN_NAMESPACE
class QPainter;
QT_END_NAMESPACE

class QWebFrame;
class QWebElementCollection;
class QWebElementPrivate;

class QWEBKIT_EXPORT QWebElement {
public:
    QWebElement();
    QWebElement(const QWebElement&);
    QWebElement &operator=(const QWebElement&);
    ~QWebElement();

    bool operator==(const QWebElement& o) const;
    bool operator!=(const QWebElement& o) const;

    bool isNull() const;

    QWebElementCollection findAll(const QString &selectorQuery) const;
    QWebElement findFirst(const QString &selectorQuery) const;

    void setPlainText(const QString& text);
    QString toPlainText() const;
    QString toTextContent() const;
    QString toLayoutText() const;

    void setOuterXml(const QString& markup);
    QString toOuterXml() const;

    void setInnerXml(const QString& markup);
    QString toInnerXml() const;

    void setAttribute(const QString& name, const QString& value);
    void setAttributeNS(const QString& namespaceUri, const QString& name, const QString& value);
    QString attribute(const QString& name, const QString& defaultValue = QString()) const;
    QString attributeNS(const QString& namespaceUri, const QString& name, const QString& defaultValue = QString()) const;
    bool hasAttribute(const QString& name) const;
    bool hasAttributeNS(const QString& namespaceUri, const QString& name) const;
    void removeAttribute(const QString& name);
    void removeAttributeNS(const QString& namespaceUri, const QString& name);
    bool hasAttributes() const;
    QStringList attributeNames(const QString& namespaceUri = QString()) const;

    void setAttributeBinaryData(const QString& name, const QString& mimeType, const char* data, size_t length);

    // Chunked counterpart to setAttributeBinaryData()'s "src" case -- lets binary image data be
    // handed over across several calls (e.g. a large file streamed in from hiped's protocol) rather
    // than needing the whole thing in memory at once, decoding/repainting progressively as each
    // chunk arrives. Image elements only for now. No-ops on any other element type.
    void beginBinaryImageData(const QString& mimeType);
    void appendBinaryImageData(const char* data, size_t length);
    void finishBinaryImageData();

    // Same idea as the trio above, but for <audio>/<video> "src" via a MediaSample buffer instead
    // of decoded image data. No-ops on any other element type. expectedTotalSize is the real,
    // final size of the complete file in bytes if known in advance, or -1 (the default) if not --
    // see HTMLMediaElement::beginBinaryMediaData() for what it unlocks. Only meaningful on the
    // first call for a given load (a single-shot load, or the first chunk of a chunked one).
    void beginBinaryMediaData(const QString& mimeType, qint64 expectedTotalSize = -1);
    void appendBinaryMediaData(const char* data, size_t length);
    void finishBinaryMediaData();


    QStringList classes() const;
    bool hasClass(const QString& name) const;
    void addClass(const QString& name);
    void removeClass(const QString& name);
    void toggleClass(const QString& name);

    bool hasFocus() const;
    void setFocus();

    void setSelectionRange(int anchor, int focus, bool scrollIntoView = false);
    QList<QRectF> rangeGeometry(int start, int end) const;
    QVector<qreal> measureText(const QString& text, qreal fontSize = 0) const;
    QVector<int> findText(const QString& text, bool backwards = false, bool caseSensitive = false, bool wholeWords = false, int limit = 1000);
    bool getSelectionRange(int* anchor, int* focus);
    void select();

    int selectionStart();
    int selectionEnd();

    QString getSelection();

    double offsetLeft() const;
    double offsetTop() const;
    double offsetWidth() const;
    double offsetHeight() const;

    double clientWidth() const;
    double clientHeight() const;

    int scrollLeft() const;
    int scrollTop() const;
    void setScrollLeft(int newLeft);
    void setScrollTop(int newTop);
    int scrollWidth() const;
    int scrollHeight() const;

    WebCore::HTMLMediaElement* isMediaElement();
    std::string getMediaPositionString(WebCore::HTMLMediaElement* mediaElement);
    std::string getPlaybackRate(WebCore::HTMLMediaElement* mediaElement);
    bool isMediaPlaying(WebCore::HTMLMediaElement* mediaElement);
    std::string getVolume(WebCore::HTMLMediaElement* mediaElement);

    void setMediaPlaying(WebCore::HTMLMediaElement* mediaElement, std::string flag);
    void setPlaybackRate(WebCore::HTMLMediaElement* mediaElement, double rate);
    void setVolume(WebCore::HTMLMediaElement* mediaElement, double volume);
    void setCurrentTime(WebCore::HTMLMediaElement* mediaElement, double time);

    using HipeCoreEventCallback = void (*)(const QString&, void* usrPtr, uint64_t usrVal1,
                                            uint64_t usrVal2, const QString& eventDetails);
    // Lifetime contract: usrPtr is not tracked or owned by hipecore -- the listener this
    // creates lives attached to the DOM node until cancelEvent()/removeAllChildren()/document
    // teardown removes it, however long that is. Whatever usrPtr points to MUST outlive the
    // DOM node (or be released via cancelEvent() first), or a later event on this node will
    // invoke callback with a dangling pointer. There is no auto-detach-on-owner-destroyed
    // safety net here (unlike e.g. GObject's weak-ref-based listeners) -- the caller is
    // responsible for the ordering.
    void requestEvent(const QString& eventName, void* usrPtr, uint64_t usrVal1, uint64_t usrVal2,
                        HipeCoreEventCallback callback, bool preventDefault=false);
    bool handlesEvent(const QString& eventName); //returns true if a handler has been set.
    void cancelEvent(const QString& eventName); //removes ALL listeners of eventName on this element, not just one.
    void setDefaultPrevention(const QString& eventName, const QString& rules); //cancels matching events' default actions.

    void setDraggable(bool draggable);

    QRect geometry() const;

    // Width, in CSS pixels, that rendering the given text would take using this
    // element's computed font. Does not wrap or otherwise lay the text out --
    // for a multi-line estimate, split on newlines and sum/max as needed.
    qreal textWidth(const QString &text) const;

    // Metrics of this element's computed font, in CSS pixels.
    qreal fontAscent() const;
    qreal fontDescent() const;
    qreal fontLineSpacing() const;

    // Hints to this element's document that a burst of layout-affecting mutations is under way,
    // so its incremental-layout scheduler should batch aggressively for a while rather than laying
    // out on every change -- see WebCore::Document::bumpLayoutBatchingDelay() for the decay rule.
    // A caller with visibility into how much work is queued up (e.g. a protocol layer batching many
    // instructions per client) can use this to extend that same batching to non-table bulk mutation
    // bursts, which nothing currently detects on its own.
    void bumpLayoutBatchingDelay(int milliseconds);

    // Turns this <object> element into an X11 embed target: hipecore creates
    // a native child X11 window at the element's render-box position and
    // keeps it geometry-synced to CSS layout from then on. Returns the XID
    // of that window, or 0 if this element isn't an unreplaced <object> (or
    // hipecore wasn't built with X11 support). Foreign top-level windows can
    // then be XReparentWindow'd into the returned XID directly - hipecore
    // does not do any reparenting itself.
    quintptr x11EmbedTargetXid();

    // Direct C++ entry points for <canvas> drawing, replacing the JS-only Canvas 2D
    // API path (unreachable since hipecore disabled script execution). This element
    // must be a <canvas>; contextType is currently only "2d" (WebGL is not exposed).
    // Returns false if this isn't a <canvas> or contextType is unsupported.
    bool useCanvasContext(const QString& contextType);

    // Invokes a Canvas 2D drawing method by name (matching the Canvas 2D spec's own
    // method names, e.g. "fillRect", "lineTo", "drawImage") against the context most
    // recently selected by useCanvasContext() on this element. argsString is a flat,
    // comma-separated argument list (whitespace around commas is tolerated; a
    // double-quoted token may contain literal commas/whitespace, e.g. for fillText's
    // text argument). Returns false if the method is unknown, the argument count is
    // wrong, an argument fails to parse, or no context has been selected yet -- in
    // every case this is a silent no-op, not an error the caller needs to handle.
    bool canvasAction(const QString& method, const QString& argsString);

    // Sets a Canvas 2D state property by name (e.g. "fillStyle", "lineWidth", "font"),
    // matching the Canvas 2D spec's own property names. value is a single token, not a
    // comma-separated list. Same silent-no-op-on-failure contract as canvasAction().
    bool canvasSetProperty(const QString& property, const QString& value);

    // Retrieves image data for an <img> (the original bytes most recently given via
    // setAttributeBinaryData()/appendBinaryImageData() et al, verbatim -- the exact
    // counterpart to SET_SRC) or a <canvas> (a fresh render of its current pixel
    // content). formatHint is only consulted for <canvas> ("png" or "pdf"; empty or
    // unrecognized defaults to "png") and ignored for <img>, which always returns its
    // native format. Returns false with outError set if this element isn't an
    // <img>/<canvas>, or an <img> has no image data loaded yet. Deliberately doesn't
    // support <audio>/<video> -- a client that already pushed a media file via SET_SRC
    // has no legitimate reason to fetch the same large payload back from the server.
    bool getSrcData(const QString& formatHint, QByteArray& outData, QString& outMimeType, QString& outError);

    // Answers the handful of Canvas 2D queries that return a value rather than drawing
    // (isPointInPath, isPointInStroke, measureText) against the context most recently
    // selected by useCanvasContext() -- CANVAS_ACTION has no reply channel, so these
    // couldn't be served as ordinary actions. queryName is "isPointInPath"/
    // "isPointInStroke"/"measureText"; argsString is a comma-separated argument list,
    // same splitting rules as canvasAction()'s own argsString. Returns false with
    // outError set on an unrecognised query, wrong argument count, or an unknown path
    // id; outResult holds the answer as a string ("true"/"false", or a number) on
    // success.
    bool canvasQuery(const QString& queryName, const QString& argsString, QString& outResult, QString& outError);

    QString tagName() const;
    QString prefix() const;
    QString localName() const;
    QString namespaceUri() const;

    QWebElement parent() const;
    QWebElement firstChild() const;
    QWebElement lastChild() const;
    QWebElement nextSibling() const;
    QWebElement previousSibling() const;
    QWebElement document() const;
    QWebFrame *webFrame() const;

    // TODO: Add QWebElementCollection overloads
    // docs need example snippet
    void appendInside(const QString& markup);
    void appendInside(const QWebElement& element);

    // docs need example snippet
    void prependInside(const QString& markup);
    void prependInside(const QWebElement& element);

    // docs need example snippet
    void appendOutside(const QString& markup);
    void appendOutside(const QWebElement& element);

    // docs need example snippet
    void prependOutside(const QString& markup);
    void prependOutside(const QWebElement& element);

    // docs need example snippet
    void encloseContentsWith(const QWebElement& element);
    void encloseContentsWith(const QString& markup);
    void encloseWith(const QString& markup);
    void encloseWith(const QWebElement& element);

    void replace(const QString& markup);
    void replace(const QWebElement& element);

    QWebElement clone() const;
    QWebElement& takeFromDocument();
    void removeFromDocument();
    void removeAllChildren();

    QVariant evaluateJavaScript(const QString& scriptSource);

    enum StyleResolveStrategy {
         InlineStyle,
         CascadedStyle,
         ComputedStyle
    };
    QString styleProperty(const QString& name, StyleResolveStrategy strategy) const;
    void setStyleProperty(const QString& name, const QString& value);

    void render(QPainter* painter);
    void render(QPainter* painter, const QRect& clipRect);

private:
    explicit QWebElement(WebCore::Element*);

    friend class WebCore::ChromeClientQt;
    friend class QWebElementCollection;
    friend class QWebFrameAdapter;
    friend class QWebHitTestResult;
    friend class QWebHitTestResultPrivate;
    friend class QWebPage;
    friend class QWebPageAdapter;
    friend class QWebPagePrivate;

    QWebElementPrivate* d;
    WebCore::Element* m_element;
};

class QWebElementCollectionPrivate;

class QWEBKIT_EXPORT QWebElementCollection
{
public:
    QWebElementCollection();
    QWebElementCollection(const QWebElement &contextElement, const QString &query);
    QWebElementCollection(const QWebElementCollection &);
    QWebElementCollection &operator=(const QWebElementCollection &);
    ~QWebElementCollection();

    QWebElementCollection operator+(const QWebElementCollection &other) const;
    inline QWebElementCollection &operator+=(const QWebElementCollection &other)
    {
        append(other); return *this;
    }

    void append(const QWebElementCollection &collection);

    int count() const;
    QWebElement at(int i) const;
    inline QWebElement operator[](int i) const { return at(i); }

    inline QWebElement first() const { return at(0); }
    inline QWebElement last() const { return at(count() - 1); }

    QList<QWebElement> toList() const;

    class const_iterator {
       public:
           inline const_iterator(const QWebElementCollection* collection_, int index) : i(index), collection(collection_) {}
           inline const_iterator(const const_iterator& o) : i(o.i), collection(o.collection) {}

           inline const QWebElement operator*() const { return collection->at(i); }

           inline bool operator==(const const_iterator& o) const { return i == o.i && collection == o.collection; }
           inline bool operator!=(const const_iterator& o) const { return i != o.i || collection != o.collection; }
           inline bool operator<(const const_iterator& o) const { return i < o.i; }
           inline bool operator<=(const const_iterator& o) const { return i <= o.i; }
           inline bool operator>(const const_iterator& o) const { return i > o.i; }
           inline bool operator>=(const const_iterator& o) const { return i >= o.i; }

           inline const_iterator& operator++() { ++i; return *this; }
           inline const_iterator operator++(int) { const_iterator n(collection, i); ++i; return n; }
           inline const_iterator& operator--() { i--; return *this; }
           inline const_iterator operator--(int) { const_iterator n(collection, i); i--; return n; }
           inline const_iterator& operator+=(int j) { i += j; return *this; }
           inline const_iterator& operator-=(int j) { i -= j; return *this; }
           inline const_iterator operator+(int j) const { return const_iterator(collection, i + j); }
           inline const_iterator operator-(int j) const { return const_iterator(collection, i - j); }
           inline int operator-(const_iterator j) const { return i - j.i; }
       private:
            int i;
            const QWebElementCollection* const collection;
    };
    friend class const_iterator;

    inline const_iterator begin() const { return constBegin(); }
    inline const_iterator end() const { return constEnd(); }
    inline const_iterator constBegin() const { return const_iterator(this, 0); }
    inline const_iterator constEnd() const { return const_iterator(this, count()); };

    class iterator {
    public:
        inline iterator(const QWebElementCollection* collection_, int index) : i(index), collection(collection_) {}
        inline iterator(const iterator& o) : i(o.i), collection(o.collection) {}

        inline QWebElement operator*() const { return collection->at(i); }

        inline bool operator==(const iterator& o) const { return i == o.i && collection == o.collection; }
        inline bool operator!=(const iterator& o) const { return i != o.i || collection != o.collection; }
        inline bool operator<(const iterator& o) const { return i < o.i; }
        inline bool operator<=(const iterator& o) const { return i <= o.i; }
        inline bool operator>(const iterator& o) const { return i > o.i; }
        inline bool operator>=(const iterator& o) const { return i >= o.i; }

        inline iterator& operator++() { ++i; return *this; }
        inline iterator operator++(int) { iterator n(collection, i); ++i; return n; }
        inline iterator& operator--() { i--; return *this; }
        inline iterator operator--(int) { iterator n(collection, i); i--; return n; }
        inline iterator& operator+=(int j) { i += j; return *this; }
        inline iterator& operator-=(int j) { i -= j; return *this; }
        inline iterator operator+(int j) const { return iterator(collection, i + j); }
        inline iterator operator-(int j) const { return iterator(collection, i - j); }
        inline int operator-(iterator j) const { return i - j.i; }
    private:
        int i;
        const QWebElementCollection* const collection;
    };
    friend class iterator;

    inline iterator begin() { return iterator(this, 0); }
    inline iterator end()  { return iterator(this, count()); }
private:
    QExplicitlySharedDataPointer<QWebElementCollectionPrivate> d;
};

Q_DECLARE_METATYPE(QWebElement)

#endif // QWEBELEMENT_H
