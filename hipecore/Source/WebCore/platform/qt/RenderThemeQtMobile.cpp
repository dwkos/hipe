/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
 *
 */

#include "config.h"
#include "RenderThemeQtMobile.h"

#include "CSSValueKeywords.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Color.h"
#include "Document.h"
#include "Font.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HTMLSelectElement.h"
#include "Page.h"
#include "PaintInfo.h"
#include "QWebPageClient.h"
#include "RenderBox.h"
#include "RenderProgress.h"
#include "StyleResolver.h"

#include <wtf/PassRefPtr.h>

#include <QColor>
#include <QFile>
#include <QGuiApplication>
#include <QPainter>
#include <QPixmapCache>

namespace WebCore {

using namespace HTMLNames;

// Constants used by the mobile theme
static const int arrowBoxWidth = 26;
static const int frameWidth = 2;
static const int checkBoxWidth = 21;
static const int radioWidth = 21;
static const int sliderSize = 20;

static const float buttonHeightRatio = 1.5;
static const float multipleComboDotsOffsetFactor = 1.8;
static const float buttonPaddingLeft = 18;
static const float buttonPaddingRight = 18;
static const float buttonPaddingTop = 2;
static const float buttonPaddingBottom = 3;
static const float menuListPadding = 9;
static const float textFieldPadding = 10;
static const float radiusFactor = 0.36;
static const float progressBarChunkPercentage = 0.2;
static const int progressAnimationGranularity = 2;
static const float sliderGrooveBorderRatio = 0.2;
static const QColor darkColor(40, 40, 40);
static const QColor highlightColor(16, 128, 221);
static const QColor buttonGradientBottom(245, 245, 245);
static const QColor shadowColor(80, 80, 80, 160);

static QHash<KeyIdentifier, CacheKey> cacheKeys;

static qreal painterScale(QPainter* painter)
{
    if (!painter)
        return 1;

    const QTransform& transform = painter->transform();
    qreal scale = 1;

    if (transform.type() == QTransform::TxScale)
        scale = qAbs(transform.m11());
    else if (transform.type() >= QTransform::TxRotate) {
        const QLineF l1(0, 0, 1, 0);
        const QLineF l2 = transform.map(l1);
        scale = qAbs(l2.length() / l1.length());
    }
    return scale;
}

uint qHash(const KeyIdentifier& id)
{
    const quint32 value = id.trait1 + (id.trait2 << 1) + (uint(id.type) << 2) + (id.height << 5) + (id.width << 14) + (id.trait3 << 25);
    const unsigned char* p = reinterpret_cast<const unsigned char*>(&value);
    uint hash = 0;
    for (int i = 0; i < 4; ++i)
        hash ^= (hash << 5) + (hash >> 2) + p[i];
    return hash;
}

/*
 * The octants' indices are identified below, for each point (x,y)
 * in the first octant, we can populate the 7 others with the corresponding
 * point.
 *
 *                                       index |   xpos   |   ypos
 *                xd                    ---------------------------
 *      4      |<--->| 3                    0  |  xd + x  |    y
 *     __________________                   1  |  xd + y  |    x
 *    /                  \                  2  |  xd + y  |   -x
 * 5 |         .(c)       |  2              3  |  xd + x  |   -y
 * 6 |                    |  1              4  | -xd - x  |   -y
 *    \__________________/                  5  | -xd - y  |   -x
 *                                          6  | -xd - y  |    x
 *      7              0                    7  | -xd - x  |    y
 *
 **/

static void addPointToOctants(QVector<QPainterPath>& octants, const QPointF& center, qreal x, qreal y , int xDelta = 0)
{
    ASSERT(octants.count() == 8);

    for (short i = 0; i < 8; ++i) {
        QPainterPath& octant = octants[i];
        QPointF pos(center);
        // The Gray code corresponding to the octant's index helps doing the math in a more generic way.
        const short gray = (i >> 1) ^ i;
        const qreal xOffset = xDelta + ((gray & 1) ? y : x);
        pos.ry() += ((gray & 2)? -1 : 1) * ((gray & 1) ? x : y);
        pos.rx() += (i < 4) ? xOffset : -xOffset;

        if (octant.elementCount())
            octant.lineTo(pos);
        else // The path is empty. Initialize the start point.
            octant.moveTo(pos);
    }
}

static void drawControlBackground(QPainter* painter, const QPen& pen, const QRect& rect, const QBrush& brush)
{
    QPen oldPen = painter->pen();
    QBrush oldBrush = painter->brush();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(pen);
    painter->setBrush(brush);

    static const qreal line = 1.5;
    const QRectF paddedRect = rect.adjusted(line, line, -line, -line);

    static const int n = 3;
    const qreal invPow = 1 / double(n);
    ASSERT(paddedRect.width() >= paddedRect.height());
    const int radius = paddedRect.height() / 2;
    const int xDelta = paddedRect.width() / 2 - radius;
    const QPointF center = paddedRect.center();
    qreal x = 0;
    qreal y;
    QVector<QPainterPath> octants(8);
    // Stay within reasonable distance from edge values, which can cause artifacts at certain zoom levels.
    static const float epsilon = 0.02;
    for (y = radius - epsilon; y - epsilon > x; y -= 0.5) {
        x = radius * pow(1 - pow(qAbs(y) / radius , n), invPow);
        addPointToOctants(octants, center, x, y, xDelta);
    }

    QPainterPath path = octants.first();
    for (int i = 1; i < 8; ++i) {
        // Due to the orientation of the arcs, we need to reverse the paths with odd indices.
        QPainterPath subPath = (i % 2) ?  octants.at(i).toReversed() : octants.at(i);
        path.connectPath(subPath);
    }
    path.closeSubpath();

    painter->drawPath(path);
    painter->setPen(oldPen);
    painter->setBrush(oldBrush);
}

static inline QRect shrinkRectToSquare(const QRect& rect)
{
    const int side = qMin(rect.height(), rect.width());
    return QRect(rect.topLeft(), QSize(side, side));
}

static inline QPen borderPen(QPainter* painter = 0)
{
    return QPen(darkColor, qMin(1.0, 0.4 * painterScale(painter)));
}

QSharedPointer<StylePainter> RenderThemeQtMobile::getStylePainter(const PaintInfo& pi)
{
    return QSharedPointer<StylePainter>(new StylePainterMobile(this, pi));
}

QPalette RenderThemeQtMobile::colorPalette() const
{
    // The self-drawn mobile controls use fixed colours and never consult this
    // palette. It is still used by RenderThemeQt for text-selection colours and
    // CSS system colours, which should follow the embedder's palette -- this used
    // to be handled by RenderThemeQStyle (now removed), so do it here instead.
    QPalette palette = QGuiApplication::palette();
    if (m_page) {
        if (QWebPageClient* pageClient = m_page->chrome().client().platformPageClient())
            palette = pageClient->palette();
    }
    return palette;
}

StylePainterMobile::StylePainterMobile(RenderThemeQtMobile*, const PaintInfo& paintInfo)
    : StylePainter(paintInfo.context())
{
    m_previousSmoothPixmapTransform = painter->testRenderHint(QPainter::SmoothPixmapTransform);
    if (!m_previousSmoothPixmapTransform)
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
}

StylePainterMobile::~StylePainterMobile()
{
    painter->setRenderHints(QPainter::SmoothPixmapTransform, m_previousSmoothPixmapTransform);
}

bool StylePainterMobile::findCachedControl(const KeyIdentifier& keyId, QPixmap* result)
{
    static CacheKey emptyKey;
    CacheKey key = cacheKeys.value(keyId, emptyKey);
    if (key == emptyKey)
        return false;
    const bool ret = QPixmapCache::find(key, result);
    if (!ret)
        cacheKeys.remove(keyId);
    return ret;
}

void StylePainterMobile::insertIntoCache(const KeyIdentifier& keyId, const QPixmap& pixmap)
{
    ASSERT(keyId.type);
    const int sizeInKiloBytes = pixmap.width() * pixmap.height() * pixmap.depth() / (8 * 1024);
    // Don't cache pixmaps over 512 KB;
    if (sizeInKiloBytes > 512)
        return;
    cacheKeys.insert(keyId, QPixmapCache::insert(pixmap));
}

QSize StylePainterMobile::sizeForPainterScale(const QRect& rect) const
{
    qreal scale = painterScale(painter);
    QTransform scaleTransform = QTransform::fromScale(scale, scale);

    return scaleTransform.mapRect(rect).size();
}

QSizeF StylePainterMobile::sizeForPainterScale(const QRectF& rect) const
{
    qreal scale = painterScale(painter);
    QTransform scaleTransform = QTransform::fromScale(scale, scale);

    return scaleTransform.mapRect(rect).size();
}

void StylePainterMobile::drawMultipleComboButton(QPainter* painter, const QSizeF& size, const QColor& color) const
{
    const qreal dotDiameter = size.height();
    const qreal dotRadii = dotDiameter / 2;

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(color);
    painter->setBrush(color);

    for (int i = 0; i < 3; ++i) {
        QPointF center(dotRadii + i * multipleComboDotsOffsetFactor * dotDiameter, dotRadii);
        painter->drawEllipse(center, dotRadii, dotRadii);
    }
}

void StylePainterMobile::drawSimpleComboButton(QPainter* painter, const QSizeF& size, const QColor& color) const
{
    const qreal gap = size.height() / 5.0;
    const qreal arrowHeight = (size.height() - gap) / 2.0;
    const qreal right = arrowHeight * 2;
    const qreal bottomBaseline = size.height() - arrowHeight;
    QPolygonF upArrow, downArrow;
    upArrow << QPointF(0, arrowHeight) << QPointF(arrowHeight, 0) << QPointF(right, arrowHeight);
    downArrow << QPointF(0, bottomBaseline) << QPointF(arrowHeight, bottomBaseline + arrowHeight)
              << QPointF(right, bottomBaseline);

    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawPolygon(upArrow);
    painter->drawPolygon(downArrow);
}

QSizeF StylePainterMobile::getButtonImageSize(int buttonHeight, bool multiple) const
{
    if (multiple)
        return QSizeF(qreal(2 + buttonHeight * 3 * multipleComboDotsOffsetFactor/ 10.0)
                      , qreal(2 + buttonHeight / 10.0));

    const qreal height = buttonHeight / 2.5;
    const qreal width = 4 * height / 5.0;
    return QSizeF(2 + width, 2 + height);
}

QPixmap StylePainterMobile::findComboButton(const QSize& size, bool multiple, bool enabled) const
{
    if (size.isNull())
        return QPixmap();
    QPixmap result;
    KeyIdentifier id;
    id.type = KeyIdentifier::ComboButton;
    id.width = size.width();
    id.height = size.height();
    id.trait1 = multiple;
    id.trait2 = enabled;

    if (!findCachedControl(id, &result)) {
        result = QPixmap(size);
        const qreal border = painterScale(painter);
        const QSizeF padding(2 * border, 2 * border);
        const QSizeF innerSize = size - padding;
        ASSERT(innerSize.isValid());
        result.fill(Qt::transparent);
        QPainter cachePainter(&result);
        cachePainter.translate(border, border);
        if (multiple)
            drawMultipleComboButton(&cachePainter, innerSize, enabled ? darkColor : Qt::lightGray);
        else
            drawSimpleComboButton(&cachePainter, innerSize, enabled ? darkColor : Qt::lightGray);
        insertIntoCache(id, result);
    }
    return result;
}

void StylePainterMobile::drawLineEdit(const QRectF& rect, bool focused, bool enabled)
{
    Q_UNUSED(enabled);
    QPixmap lineEdit = findLineEdit(sizeForPainterScale(rect), focused);
    if (lineEdit.isNull())
        return;
    painter->drawPixmap(rect, lineEdit, lineEdit.rect());
}

QPixmap StylePainterMobile::findLineEdit(const QSize & size, bool focused) const
{
    QPixmap result;
    KeyIdentifier id;
    id.type = KeyIdentifier::LineEdit;
    id.width = size.width();
    id.height = size.height();
    id.trait1 = focused;

    if (!findCachedControl(id, &result)) {
        const int focusFrame = painterScale(painter);
        result = QPixmap(size);
        result.fill(Qt::transparent);
        const QRect rect = result.rect().adjusted(focusFrame, focusFrame, -focusFrame, -focusFrame);
        QPainter cachePainter(&result);
        drawControlBackground(&cachePainter, borderPen(painter), rect, Qt::white);

        if (focused) {
            QPen focusPen(highlightColor, 1.2 * painterScale(painter), Qt::SolidLine);
            drawControlBackground(&cachePainter, focusPen, rect, Qt::NoBrush);
        }
        insertIntoCache(id, result);
    }
    return result;
}

QPixmap StylePainterMobile::findLineEdit(const QSizeF & size, bool focused) const
{
    return findLineEdit(size.toSize(), focused);
}

// Checkboxes and radios are drawn as minimal monochrome strokes in the element's
// own foreground colour (its CSS `color`), so they read correctly on both light and
// dark pages: a square outline (+ tick when checked) for a checkbox, a circle
// outline (+ filled centre dot when checked) for a radio. The two are now clearly
// distinguishable by shape, and checked-ness by the mark rather than a fill colour.
// An explicit non-transparent CSS `background-color` fills the box; otherwise the
// control is transparent and shows against the page. Drawn straight to the painter
// (no pixmap cache) since the colour now varies per element.
//
// While the control is held down (pressed), the centre mark is drawn at a low alpha
// as a transitional "click registered" cue -- previewing the tick/dot appearing on
// an unchecked control, or fading on a checked one. WebCore repaints the control on
// the active-state change (Element::setActive -> RenderTheme::stateChanged), so no
// extra plumbing is needed.

// Opacity of the centre mark: solid when settled-checked, a faint preview while the
// (enabled) control is held down, nothing otherwise.
static inline qreal markOpacity(bool checked, bool pressed, bool enabled)
{
    if (pressed && enabled)
        return 0.4;
    return checked ? 1.0 : 0.0;
}

void StylePainterMobile::drawCheckBox(const QRect& rect, bool checked, bool pressed, bool enabled, const QColor& fg, const QColor& bg)
{
    const QRectF square(shrinkRectToSquare(rect));
    QColor stroke(fg);
    if (!enabled)
        stroke.setAlphaF(stroke.alphaF() * 0.4);
    const qreal penWidth = qMax<qreal>(1, square.width() / 14.0);
    const QRectF box = square.adjusted(penWidth, penWidth, -penWidth, -penWidth);

    const bool previousAntialiasing = painter->testRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (bg.isValid() && bg.alpha()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(bg);
        painter->drawRect(box);
    }

    QPen pen(stroke, penWidth);
    pen.setJoinStyle(Qt::MiterJoin);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(box);

    const qreal opacity = markOpacity(checked, pressed, enabled);
    if (opacity > 0) {
        QColor tickColor(stroke);
        tickColor.setAlphaF(tickColor.alphaF() * opacity);
        painter->save();
        painter->translate(box.topLeft());
        painter->scale(box.width(), box.height());
        QPen tick(tickColor, (penWidth * 1.4) / box.width());
        tick.setCapStyle(Qt::RoundCap);
        tick.setJoinStyle(Qt::RoundJoin);
        painter->setPen(tick);
        painter->setBrush(Qt::NoBrush);
        QPainterPath path;
        path.moveTo(0.22, 0.52);
        path.lineTo(0.42, 0.72);
        path.lineTo(0.80, 0.28);
        painter->drawPath(path);
        painter->restore();
    }

    painter->setRenderHint(QPainter::Antialiasing, previousAntialiasing);
}

void StylePainterMobile::drawRadioButton(const QRect& rect, bool checked, bool pressed, bool enabled, const QColor& fg, const QColor& bg)
{
    const QRectF square(shrinkRectToSquare(rect));
    QColor stroke(fg);
    if (!enabled)
        stroke.setAlphaF(stroke.alphaF() * 0.4);
    const qreal penWidth = qMax<qreal>(1, square.width() / 14.0);
    const QRectF circle = square.adjusted(penWidth, penWidth, -penWidth, -penWidth);

    const bool previousAntialiasing = painter->testRenderHint(QPainter::Antialiasing);
    painter->setRenderHint(QPainter::Antialiasing, true);

    if (bg.isValid() && bg.alpha()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(bg);
        painter->drawEllipse(circle);
    }

    painter->setPen(QPen(stroke, penWidth));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(circle);

    const qreal opacity = markOpacity(checked, pressed, enabled);
    if (opacity > 0) {
        QColor dotColor(stroke);
        dotColor.setAlphaF(dotColor.alphaF() * opacity);
        const qreal radius = circle.width() / 4.0;
        QRectF dot(0, 0, 2 * radius, 2 * radius);
        dot.moveCenter(circle.center());
        painter->setPen(Qt::NoPen);
        painter->setBrush(dotColor);
        painter->drawEllipse(dot);
    }

    painter->setRenderHint(QPainter::Antialiasing, previousAntialiasing);
}

void StylePainterMobile::drawPushButton(const QRect& rect, bool sunken, bool enabled)
{
    QPixmap pushButton = findPushButton(sizeForPainterScale(rect), sunken, enabled);
    if (pushButton.isNull())
        return;
    painter->drawPixmap(rect, pushButton);
}

QPixmap StylePainterMobile::findPushButton(const QSize& size, bool sunken, bool enabled) const
{
    QPixmap result;
    KeyIdentifier id;
    id.type = KeyIdentifier::PushButton;
    id.width = size.width();
    id.height = size.height();
    id.trait1 = sunken;
    id.trait2 = enabled;
    if (!findCachedControl(id, &result)) {
        const qreal dropShadowSize = painterScale(painter);
        result = QPixmap(size);
        result.fill(Qt::transparent);
        const QRect rect = QRect(0, 0, size.width(), size.height() - dropShadowSize);
        QPainter cachePainter(&result);
        drawControlBackground(&cachePainter, Qt::NoPen, rect.adjusted(0, dropShadowSize, 0, dropShadowSize), shadowColor);

        QBrush brush;
        if (enabled && !sunken) {
            QLinearGradient linearGradient;
            linearGradient.setStart(rect.bottomLeft());
            linearGradient.setFinalStop(rect.topLeft());
            linearGradient.setColorAt(0.0, buttonGradientBottom);
            linearGradient.setColorAt(1.0, Qt::white);
            brush = linearGradient;
        } else if (!enabled)
            brush = QColor(241, 242, 243);
        else { // sunken
            QLinearGradient linearGradient;
            linearGradient.setStart(rect.bottomLeft());
            linearGradient.setFinalStop(rect.topLeft());
            linearGradient.setColorAt(0.0, highlightColor);
            linearGradient.setColorAt(1.0, highlightColor.lighter());
            brush = linearGradient;
        }
        drawControlBackground(&cachePainter, borderPen(painter), rect, brush);
        insertIntoCache(id, result);
    }
    return result;
}

QPixmap StylePainterMobile::findPushButton(const QSizeF& size, bool sunken, bool enabled) const
{
    return findPushButton(size.toSize(), sunken, enabled);
}

void StylePainterMobile::drawComboBox(const QRect& rect, bool multiple, bool enabled)
{
    drawComboBox(QRectF(rect), multiple, enabled);
}

void StylePainterMobile::drawComboBox(const QRectF& rect, bool multiple, bool enabled)
{
    QPixmap pushButton = findPushButton(sizeForPainterScale(rect), /*sunken = */false, enabled);
    if (pushButton.isNull())
        return;
    painter->drawPixmap(rect, pushButton, rect);
    QRectF targetRect(QPointF(0, 0), getButtonImageSize(rect.height() - 1, multiple));
    const QPointF buttonCenter(rect.right() - arrowBoxWidth / 2, rect.top() + (rect.height() - 1) / 2);
    targetRect.moveCenter(buttonCenter);
    QPixmap pic = findComboButton(sizeForPainterScale(targetRect.toRect()), multiple, enabled);
    if (pic.isNull())
        return;

    painter->drawPixmap(targetRect.toRect(), pic);
}

void StylePainterMobile::drawProgress(const QRect& rect, double progress, bool leftToRight, bool animated, bool vertical) const
{
    const int horizontalBorder = (vertical ? rect.width() / 4 : 0);
    const int verticalBorder = (vertical ? 0 : rect.height() / 4);
    const QRect targetRect = rect.adjusted(horizontalBorder, verticalBorder, -horizontalBorder, -verticalBorder);

    QPixmap result;
    QSize imageSize = sizeForPainterScale(targetRect);
    if (vertical)
        qSwap(imageSize.rheight(), imageSize.rwidth());
    KeyIdentifier id;
    id.type = KeyIdentifier::Progress;
    id.width = imageSize.width();
    id.height = imageSize.height();
    id.trait1 = animated;
    id.trait2 = (!animated && !leftToRight);
    id.trait3 = progress * 100;
    if (!findCachedControl(id, &result)) {
        if (imageSize.isNull())
            return;
        result = QPixmap(imageSize);
        result.fill(Qt::transparent);
        QPainter painter(&result);
        painter.setRenderHint(QPainter::Antialiasing);
        QRect progressRect(QPoint(0, 0), imageSize);
        qreal radius = radiusFactor * progressRect.height();
        painter.setBrush(Qt::NoBrush);
        painter.setPen(borderPen());
        progressRect.adjust(1, 1, -1, -1);
        painter.drawRoundedRect(progressRect, radius, radius);
        progressRect.adjust(1, 1, -1, -1);
        if (animated) {
            const int right = progressRect.right();
            const int startPos = right * (1 - progressBarChunkPercentage) * 2 * fabs(progress - 0.5);
            progressRect.setWidth(progressBarChunkPercentage * right);
            progressRect.moveLeft(startPos);
        } else {
            progressRect.setWidth(progress * progressRect.width());
            if (!leftToRight)
                progressRect.moveRight(imageSize.width() - 2);
        }
        if (progressRect.width() > 0) {
            QLinearGradient gradient;
            gradient.setStart(progressRect.bottomLeft());
            gradient.setFinalStop(progressRect.topLeft());
            gradient.setColorAt(0.0, highlightColor);
            gradient.setColorAt(1.0, highlightColor.lighter());
            painter.setBrush(gradient);
            painter.setPen(Qt::NoPen);
            radius = radiusFactor * progressRect.height();
            painter.drawRoundedRect(progressRect, radius, radius);
        }
        insertIntoCache(id, result);
    }
    QTransform transform;
    transform.rotate(-90);
    painter->drawPixmap(targetRect, vertical ? result.transformed(transform) : result);
}

void StylePainterMobile::drawSliderThumb(const QRect & rect, bool pressed) const
{
    QPixmap result;
    const QSize size = sizeForPainterScale(rect);
    KeyIdentifier id;
    id.type = KeyIdentifier::SliderThumb;
    id.width = size.width();
    id.height = size.height();
    id.trait1 = pressed;
    if (!findCachedControl(id, &result)) {
        if (size.isNull())
            return;
        result = QPixmap(size);
        result.fill(Qt::transparent);
        QPainter cachePainter(&result);
        drawControlBackground(&cachePainter, borderPen(painter), QRect(QPoint(0, 0), size), pressed? Qt::lightGray : buttonGradientBottom);
        insertIntoCache(id, result);
    }
    painter->drawPixmap(rect, result);
}


PassRefPtr<RenderTheme> RenderThemeQtMobile::create(Page* page)
{
    return adoptRef(new RenderThemeQtMobile(page));
}

RenderThemeQtMobile::RenderThemeQtMobile(Page* page)
    : RenderThemeQt(page)
{
}

RenderThemeQtMobile::~RenderThemeQtMobile()
{
}

bool RenderThemeQtMobile::isControlStyled(const RenderStyle& style, const BorderData& border, const FillLayer& fill, const Color& backgroundColor) const
{
    switch (style.appearance()) {
    case CheckboxPart:
    case RadioPart:
        return false;
    default:
        return RenderThemeQt::isControlStyled(style, border, fill, backgroundColor);
    }
}

LengthBox RenderThemeQtMobile::popupInternalPaddingBox(const RenderStyle&) const
{
    return { 0, 0, 1, 0 };
}

void RenderThemeQtMobile::computeSizeBasedOnStyle(RenderStyle& renderStyle) const
{
    QSize size(0, 0);

    switch (renderStyle.appearance()) {
    case TextAreaPart:
    case SearchFieldPart:
    case TextFieldPart: {
        int padding = frameWidth;
        renderStyle.setPaddingLeft(Length(padding, Fixed));
        renderStyle.setPaddingRight(Length(padding, Fixed));
        renderStyle.setPaddingTop(Length(padding, Fixed));
        renderStyle.setPaddingBottom(Length(padding, Fixed));
        break;
    }
    default:
        renderStyle.resetPadding();
        break;
    }
    // If the width and height are both specified, then we have nothing to do.
    if (!renderStyle.width().isIntrinsicOrAuto() && !renderStyle.height().isAuto())
        return;

    switch (renderStyle.appearance()) {
    case CheckboxPart: {
        const int w = checkBoxWidth * renderStyle.effectiveZoom();
        size = QSize(w, w);
        break;
    }
    case RadioPart: {
        const int w = radioWidth * renderStyle.effectiveZoom();
        size = QSize(w, w);
        break;
    }
    case PushButtonPart:
    case SquareButtonPart:
    case DefaultButtonPart:
    case ButtonPart:
    case MenulistPart: {
        const int height = renderStyle.fontMetrics().height() * buttonHeightRatio * renderStyle.effectiveZoom();
        size = QSize(renderStyle.width().value(), height);
        break;
    }
    default:
        break;
    }

    // FIXME: Check is flawed, since it doesn't take min-width/max-width into account.
    if (renderStyle.width().isIntrinsicOrAuto() && size.width() > 0)
        renderStyle.setMinWidth(Length(size.width(), Fixed));
    if (renderStyle.height().isAuto() && size.height() > 0)
        renderStyle.setMinHeight(Length(size.height(), Fixed));
}

void RenderThemeQtMobile::adjustButtonStyle(StyleResolver&, RenderStyle& style, Element*) const
{
    // Ditch the border.
    style.resetBorder();

    FontCascadeDescription fontDescription = style.fontDescription();
    fontDescription.setIsAbsoluteSize(true);

    fontDescription.setSpecifiedSize(style.fontSize());
    fontDescription.setComputedSize(style.fontSize());

    style.setLineHeight(RenderStyle::initialLineHeight());
    setButtonSize(style);
    setButtonPadding(style);
}

void RenderThemeQtMobile::setButtonPadding(RenderStyle& style) const
{
    style.setPaddingLeft(Length(buttonPaddingLeft, Fixed));
    style.setPaddingRight(Length(buttonPaddingRight, Fixed));
    style.setPaddingTop(Length(buttonPaddingTop, Fixed));
    style.setPaddingBottom(Length(buttonPaddingBottom, Fixed));
}

bool RenderThemeQtMobile::paintButton(const RenderObject& o, const PaintInfo& i, const IntRect& r)
{
    StylePainterMobile p(this, i);
    if (!p.isValid())
       return true;

    ControlPart appearance = o.style().appearance();
    if (appearance == PushButtonPart || appearance == ButtonPart) {
        p.drawPushButton(r, isPressed(o), isEnabled(o));
    } else if (appearance == RadioPart || appearance == CheckboxPart) {
        const QColor fg = o.style().visitedDependentColor(CSSPropertyColor);
        const QColor bg = o.style().visitedDependentColor(CSSPropertyBackgroundColor);
        if (appearance == RadioPart)
            p.drawRadioButton(r, isChecked(o), isPressed(o), isEnabled(o), fg, bg);
        else
            p.drawCheckBox(r, isChecked(o), isPressed(o), isEnabled(o), fg, bg);
    }

    return false;
}

void RenderThemeQtMobile::adjustTextFieldStyle(StyleResolver&, RenderStyle& style, Element*) const
{
    // Resetting the style like this leads to differences like:
    // - RenderTextControl {INPUT} at (2,2) size 168x25 [bgcolor=#FFFFFF] border: (2px inset #000000)]
    // + RenderTextControl {INPUT} at (2,2) size 166x26
    // in layout tests when a CSS style is applied that doesn't affect background color, border or
    // padding. Just worth keeping in mind!
    style.setBackgroundColor(Color::transparent);
    style.resetBorder();
    style.setBorderTopWidth(frameWidth);
    style.setBorderRightWidth(frameWidth);
    style.setBorderBottomWidth(frameWidth);
    style.setBorderLeftWidth(frameWidth);
    style.resetPadding();
    computeSizeBasedOnStyle(style);
    style.setPaddingLeft(Length(textFieldPadding, Fixed));
    style.setPaddingRight(Length(textFieldPadding, Fixed));
}

bool RenderThemeQtMobile::paintTextField(const RenderObject& o, const PaintInfo& i, const FloatRect& r)
{
    StylePainterMobile p(this, i);
    if (!p.isValid())
        return true;

    ControlPart appearance = o.style().appearance();
    if (appearance != TextFieldPart
        && appearance != SearchFieldPart
        && appearance != TextAreaPart)
        return true;

    // Now paint the text field.
    if (appearance == TextAreaPart) {
        const bool previousAntialiasing = p.painter->testRenderHint(QPainter::Antialiasing);
        p.painter->setRenderHint(QPainter::Antialiasing);
        p.painter->setPen(borderPen());
        p.painter->setBrush(Qt::white);
        const int radius = checkBoxWidth * radiusFactor;
        p.painter->drawRoundedRect(r, radius, radius);

        if (isFocused(o)) {
            QPen focusPen(highlightColor, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.painter->setPen(focusPen);
            p.painter->setBrush(Qt::NoBrush);
            p.painter->drawRoundedRect(r, radius, radius);
        }
        p.painter->setRenderHint(QPainter::Antialiasing, previousAntialiasing);
    } else
        p.drawLineEdit(r, isFocused(o), isEnabled(o));
    return false;
}

void RenderThemeQtMobile::adjustMenuListStyle(StyleResolver& styleResolver, RenderStyle& style, Element* e) const
{
    RenderThemeQt::adjustMenuListStyle(styleResolver, style, e);
    style.setPaddingLeft(Length(menuListPadding, Fixed));
}

void RenderThemeQtMobile::setPopupPadding(RenderStyle& style) const
{
    const int paddingLeft = 4;
    const int paddingRight = style.width().isFixed() || style.width().isPercent() ? 5 : 8;

    style.setPaddingLeft(Length(paddingLeft, Fixed));
    style.setPaddingRight(Length(paddingRight + arrowBoxWidth, Fixed));

    style.setPaddingTop(Length(2, Fixed));
    style.setPaddingBottom(Length(2, Fixed));
}

bool RenderThemeQtMobile::paintMenuList(const RenderObject& o, const PaintInfo& i, const FloatRect& r)
{
    StylePainterMobile p(this, i);
    if (!p.isValid())
        return true;

    p.drawComboBox(r, checkMultiple(o), isEnabled(o));
    return false;
}

bool RenderThemeQtMobile::paintMenuListButton(RenderObject& o, const PaintInfo& i,
                                        const IntRect& r)
{
    StylePainterMobile p(this, i);
    if (!p.isValid())
        return true;

    p.drawComboBox(r, checkMultiple(o), isEnabled(o));

    return false;
}

double RenderThemeQtMobile::animationDurationForProgressBar(RenderProgress& renderProgress) const
{
    if (renderProgress.isDeterminate())
        return 0;
    // Our animation goes back and forth so we need to make it last twice as long
    // and we need the numerator to be an odd number to ensure we get a progress value of 0.5.
    return (2 * progressAnimationGranularity +1) / progressBarChunkPercentage * animationRepeatIntervalForProgressBar(renderProgress);
}

bool RenderThemeQtMobile::paintProgressBar(const RenderObject& o, const PaintInfo& pi, const IntRect& r)
{
    if (!o.isProgress())
        return true;

    StylePainterMobile p(this, pi);
    if (!p.isValid())
        return true;

    auto& renderProgress = downcast<RenderProgress>(o);
    const bool isRTL = (renderProgress.style().direction() == RTL);

    if (renderProgress.isDeterminate())
        p.drawProgress(r, renderProgress.position(), !isRTL);
    else
        p.drawProgress(r, renderProgress.animationProgress(), !isRTL, true);

    return false;
}

bool RenderThemeQtMobile::paintSliderTrack(const RenderObject& o, const PaintInfo& pi,
                                     const IntRect& r)
{
    StylePainterMobile p(this, pi);
    if (!p.isValid())
        return true;

    const HTMLInputElement* slider = downcast<HTMLInputElement>(o.node());

    const double min = slider->minimum();
    const double max = slider->maximum();
    const double progress = (max - min > 0) ? (slider->valueAsNumber() - min) / (max - min) : 0;

    QRect rect(r);
    const bool vertical = (o.style().appearance() == SliderVerticalPart);
    const int groovePadding = vertical ? r.width() * sliderGrooveBorderRatio : r.height() * sliderGrooveBorderRatio;
    if (vertical) {
        rect.adjust(groovePadding, 0, -groovePadding, 0);
        // Direction is ignored on vertical sliders and we assume LTR.
        p.drawProgress(rect, progress, true, /*animated = */ false, vertical);
    } else {
        rect.adjust(0, groovePadding, 0, -groovePadding);
        p.drawProgress(rect, progress, o.style().isLeftToRightDirection(), /*animated = */ false, vertical);
    }

    return false;
}

bool RenderThemeQtMobile::paintSliderThumb(const RenderObject& o, const PaintInfo& pi,
                                     const IntRect& r)
{
    StylePainterMobile p(this, pi);
    if (!p.isValid())
        return true;

    p.drawSliderThumb(r, isPressed(o));

    return false;
}

bool RenderThemeQtMobile::checkMultiple(const RenderObject& o) const
{
    // FIXME: looks too generic
    const HTMLSelectElement* select = downcast<HTMLSelectElement>(o.node());
    return select ? select->multiple() : false;
}

void RenderThemeQtMobile::adjustSliderThumbSize(RenderStyle& style, Element* element) const
{
    const ControlPart part = style.appearance();
    if (part == SliderThumbHorizontalPart || part == SliderThumbVerticalPart) {
        const int size = sliderSize * style.effectiveZoom();
        style.setWidth(Length(size, Fixed));
        style.setHeight(Length(size, Fixed));
    } else
        RenderThemeQt::adjustSliderThumbSize(style, element);
}

}

// vim: ts=4 sw=4 et
