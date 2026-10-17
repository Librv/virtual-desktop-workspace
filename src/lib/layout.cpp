#include "layout.h"

#include <algorithm>
#include <cmath>

namespace Vdw::Layout
{

QRect slotRect(const QRect &area, const QRectF &fractions)
{
    const auto edge = [](int origin, int size, qreal fraction) {
        return origin + static_cast<int>(std::lround(fraction * size));
    };
    const int left = edge(area.x(), area.width(), fractions.x());
    const int top = edge(area.y(), area.height(), fractions.y());
    const int right = edge(area.x(), area.width(), fractions.x() + fractions.width());
    const int bottom = edge(area.y(), area.height(), fractions.y() + fractions.height());
    return QRect(left, top, right - left, bottom - top);
}

QRectF fractionsOf(const QRect &area, const QRectF &rect)
{
    const auto clamp01 = [](qreal v) {
        return std::clamp(v, qreal(0), qreal(1));
    };
    const qreal left = clamp01((rect.left() - area.x()) / area.width());
    const qreal top = clamp01((rect.top() - area.y()) / area.height());
    const qreal right = clamp01((rect.right() - area.x()) / area.width());
    const qreal bottom = clamp01((rect.bottom() - area.y()) / area.height());
    const qreal width = std::clamp(right - left, qreal(1) / area.width(), qreal(1));
    const qreal height = std::clamp(bottom - top, qreal(1) / area.height(), qreal(1));
    // A window outside the area keeps one pixel inside it.
    return QRectF(std::min(left, 1 - width), std::min(top, 1 - height), width, height);
}

}
