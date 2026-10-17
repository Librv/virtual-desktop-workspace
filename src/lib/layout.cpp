#include "layout.h"

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

}
