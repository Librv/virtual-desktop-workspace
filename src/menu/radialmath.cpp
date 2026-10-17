// SPDX-License-Identifier: GPL-2.0-or-later
#include "radialmath.h"

#include <cmath>
#include <numbers>

namespace RadialMath
{
constexpr double twoPi = 2.0 * std::numbers::pi;

double angleFromUp(QPointF center, QPointF point)
{
    const double dx = point.x() - center.x();
    const double dy = point.y() - center.y();
    // atan2(dx, -dy): 0 when pointing up, pi/2 when pointing right.
    double a = std::atan2(dx, -dy);
    if (a < 0) {
        a += twoPi;
    }
    if (a >= twoPi) {
        a -= twoPi;
    }
    return a;
}

double sliceCenterAngle(int index, int count)
{
    if (count <= 0) {
        return 0.0;
    }
    return twoPi * index / count;
}

int sliceAt(QPointF center, QPointF pointer, int count, double deadZoneRadius)
{
    if (count <= 0) {
        return -1;
    }
    const double dx = pointer.x() - center.x();
    const double dy = pointer.y() - center.y();
    if (std::hypot(dx, dy) <= deadZoneRadius) {
        return -1;
    }
    const double width = twoPi / count;
    // Shift by half a slice so slice 0 spans [-width/2, width/2) around up.
    // Work in units of slices to keep exact boundaries exact.
    const double shifted = angleFromUp(center, pointer) / width + 0.5;
    int index = static_cast<int>(std::floor(shifted + 1e-9));
    return index % count;
}
}
