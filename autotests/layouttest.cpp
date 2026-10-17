#include "layout.h"

#include <QTest>

using namespace Vdw;

Q_DECLARE_METATYPE(QRect)
Q_DECLARE_METATYPE(QRectF)

class LayoutTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testSlotRect_data()
    {
        QTest::addColumn<QRect>("area");
        QTest::addColumn<QRectF>("fractions");
        QTest::addColumn<QRect>("expected");

        const QRect full(0, 0, 1920, 1080);
        QTest::newRow("whole area") << full << QRectF(0, 0, 1, 1) << full;
        QTest::newRow("top right quarter") << full << QRectF(0.5, 0, 0.5, 0.5) << QRect(960, 0, 960, 540);
        QTest::newRow("bottom left quarter") << full << QRectF(0, 0.5, 0.5, 0.5) << QRect(0, 540, 960, 540);
        // A panel at the bottom: the area does not start at the origin.
        QTest::newRow("offset area") << QRect(0, 30, 1920, 1010) << QRectF(0, 0, 0.5, 1) << QRect(0, 30, 960, 1010);
        QTest::newRow("offset area right half") << QRect(100, 50, 1000, 800) << QRectF(0.5, 0.5, 0.5, 0.5) << QRect(600, 450, 500, 400);
        QTest::newRow("rounds edges") << QRect(0, 0, 1000, 1000) << QRectF(0.3333, 0, 0.3333, 1) << QRect(333, 0, 334, 1000);
        QTest::newRow("overshoot is kept") << full << QRectF(0.5, 0, 0.75, 1) << QRect(960, 0, 1440, 1080);
    }

    void testSlotRect()
    {
        QFETCH(QRect, area);
        QFETCH(QRectF, fractions);
        QFETCH(QRect, expected);
        QCOMPARE(Layout::slotRect(area, fractions), expected);
    }

    // Thirds of an odd width: neighbours touch exactly, whatever the rounding.
    void testNeighboursAreFlush()
    {
        const QRect area(0, 0, 1001, 777);
        const qreal third = 1.0 / 3.0;
        const QRect a = Layout::slotRect(area, QRectF(0, 0, third, 1));
        const QRect b = Layout::slotRect(area, QRectF(third, 0, third, 1));
        const QRect c = Layout::slotRect(area, QRectF(2 * third, 0, third, 1));
        QCOMPARE(a.x(), 0);
        QCOMPARE(a.x() + a.width(), b.x());
        QCOMPARE(b.x() + b.width(), c.x());
        QCOMPARE(c.x() + c.width(), 1001);

        const QRect top = Layout::slotRect(area, QRectF(0, 0, 1, 0.5));
        const QRect bottom = Layout::slotRect(area, QRectF(0, 0.5, 1, 0.5));
        QCOMPARE(top.y() + top.height(), bottom.y());
        QCOMPARE(bottom.y() + bottom.height(), 777);
    }
};

QTEST_GUILESS_MAIN(LayoutTest)

#include "layouttest.moc"
