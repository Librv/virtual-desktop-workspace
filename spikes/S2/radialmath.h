// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT
// S2 spike: slice selection math for the radial menu. Pure, no GUI.
#pragma once

#include <QPointF>

namespace RadialMath
{
// Angle of the vector from center to point, in radians, measured clockwise
// from straight up (screen coordinates, y grows downwards), in [0, 2*pi).
double angleFromUp(QPointF center, QPointF point);

// Center angle of slice `index` out of `count`, same convention as
// angleFromUp(). Slice 0 is centered straight up, the rest follow clockwise.
double sliceCenterAngle(int index, int count);

// Index of the slice in the direction of `pointer` as seen from `center`,
// or -1 when there are no slices or the pointer is inside the dead zone
// (distance <= deadZoneRadius). Distance does not matter outside it.
// Slice i covers the half-open angle range
// [center(i) - width/2, center(i) + width/2), so a pointer exactly on a
// boundary belongs to the next slice clockwise.
int sliceAt(QPointF center, QPointF pointer, int count, double deadZoneRadius);
}
