#pragma once

#include <QRect>
#include <QRectF>

namespace Vdw::Layout
{

/// Turn slot fractions (x, y, width, height of the usable area) into a pixel
/// rect. Edges are rounded, not sizes, so neighbouring slots whose fractions
/// touch are flush against each other with no gap or overlap.
QRect slotRect(const QRect &area, const QRectF &fractions);

}
