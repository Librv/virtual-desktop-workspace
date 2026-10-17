// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include <QRect>
#include <QRectF>

namespace Vdw::Layout
{

/// Turn slot fractions (x, y, width, height of the usable area) into a pixel
/// rect. Edges are rounded, not sizes, so neighbouring slots whose fractions
/// touch are flush against each other with no gap or overlap.
QRect slotRect(const QRect &area, const QRectF &fractions);

/// The inverse: a window rect as fractions of `area` (which must not be
/// empty), clamped to 0..1, with width and height at least one pixel's worth.
QRectF fractionsOf(const QRect &area, const QRectF &rect);

}
