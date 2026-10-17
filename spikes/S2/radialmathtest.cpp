// SPDX-License-Identifier: GPL-2.0-or-later
#include "radialmath.h"

#include <QTest>

#include <cmath>
#include <numbers>

class RadialMathTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void angle_data();
    void angle();
    void sliceAt_data();
    void sliceAt();
    void sliceAtIsTotal();
};

void RadialMathTest::angle_data()
{
    QTest::addColumn<QPointF>("point");
    QTest::addColumn<double>("expectedDegrees");
    QTest::newRow("up") << QPointF(0, -10) << 0.0;
    QTest::newRow("right") << QPointF(10, 0) << 90.0;
    QTest::newRow("down") << QPointF(0, 10) << 180.0;
    QTest::newRow("left") << QPointF(-10, 0) << 270.0;
    QTest::newRow("up-right") << QPointF(10, -10) << 45.0;
    QTest::newRow("up-left") << QPointF(-10, -10) << 315.0;
}

void RadialMathTest::angle()
{
    QFETCH(QPointF, point);
    QFETCH(double, expectedDegrees);
    const double deg = RadialMath::angleFromUp(QPointF(0, 0), point) * 180.0 / std::numbers::pi;
    QVERIFY2(std::abs(deg - expectedDegrees) < 1e-9, qPrintable(QString::number(deg)));
}

void RadialMathTest::sliceAt_data()
{
    QTest::addColumn<QPointF>("center");
    QTest::addColumn<QPointF>("pointer");
    QTest::addColumn<int>("count");
    QTest::addColumn<double>("deadZone");
    QTest::addColumn<int>("expected");

    const QPointF c(960, 540);
    QTest::newRow("no slices") << c << QPointF(960, 100) << 0 << 40.0 << -1;
    QTest::newRow("negative count") << c << QPointF(960, 100) << -3 << 40.0 << -1;
    QTest::newRow("at center") << c << c << 4 << 40.0 << -1;
    QTest::newRow("inside dead zone") << c << QPointF(980, 520) << 4 << 40.0 << -1;
    QTest::newRow("on dead zone edge") << c << QPointF(1000, 540) << 4 << 40.0 << -1;
    QTest::newRow("just outside dead zone") << c << QPointF(1001, 540) << 4 << 40.0 << 1;
    QTest::newRow("zero dead zone, at center") << c << c << 4 << 0.0 << -1;
    QTest::newRow("single slice, any direction") << c << QPointF(100, 1000) << 1 << 40.0 << 0;
    // Four slices: 0 up, 1 right, 2 down, 3 left.
    QTest::newRow("4: up") << c << QPointF(960, 100) << 4 << 40.0 << 0;
    QTest::newRow("4: right") << c << QPointF(1500, 540) << 4 << 40.0 << 1;
    QTest::newRow("4: down") << c << QPointF(960, 1000) << 4 << 40.0 << 2;
    QTest::newRow("4: left") << c << QPointF(100, 540) << 4 << 40.0 << 3;
    QTest::newRow("4: slightly left of up") << c << QPointF(950, 100) << 4 << 40.0 << 0;
    QTest::newRow("4: far away, distance does not matter") << c << QPointF(960, -100000) << 4 << 40.0 << 0;
    // Boundaries are half-open: the boundary belongs to the next slice
    // clockwise. 45 degrees is the edge between slice 0 and 1.
    QTest::newRow("4: exactly 45 deg") << c << QPointF(1060, 440) << 4 << 40.0 << 1;
    QTest::newRow("4: exactly 315 deg") << c << QPointF(860, 440) << 4 << 40.0 << 0;
    // Three slices: 0 up (300..60), 1 lower right (60..180), 2 lower left (180..300).
    QTest::newRow("3: up") << c << QPointF(960, 0) << 3 << 40.0 << 0;
    QTest::newRow("3: right") << c << QPointF(1500, 540) << 3 << 40.0 << 1;
    QTest::newRow("3: left") << c << QPointF(100, 540) << 3 << 40.0 << 2;
    QTest::newRow("3: straight down is a boundary, goes to 2") << c << QPointF(960, 1000) << 3 << 40.0 << 2;
    // Center near a screen corner.
    QTest::newRow("6: center top-left, pointer down-right") << QPointF(0, 0) << QPointF(100, 100) << 6 << 10.0 << 2;
}

void RadialMathTest::sliceAt()
{
    QFETCH(QPointF, center);
    QFETCH(QPointF, pointer);
    QFETCH(int, count);
    QFETCH(double, deadZone);
    QFETCH(int, expected);
    QCOMPARE(RadialMath::sliceAt(center, pointer, count, deadZone), expected);
}

void RadialMathTest::sliceAtIsTotal()
{
    // Each slice's own center angle maps back to it, and every direction
    // outside the dead zone maps to a valid slice.
    for (int count = 1; count <= 12; ++count) {
        for (int i = 0; i < count; ++i) {
            const double a = RadialMath::sliceCenterAngle(i, count);
            const QPointF p(100 * std::sin(a), -100 * std::cos(a));
            QCOMPARE(RadialMath::sliceAt(QPointF(0, 0), p, count, 10.0), i);
        }
        for (int step = 0; step < 3600; ++step) {
            const double a = step * std::numbers::pi / 1800.0;
            const QPointF p(50 * std::sin(a), -50 * std::cos(a));
            const int s = RadialMath::sliceAt(QPointF(0, 0), p, count, 10.0);
            QVERIFY(s >= 0 && s < count);
        }
    }
}

QTEST_GUILESS_MAIN(RadialMathTest)
#include "radialmathtest.moc"
