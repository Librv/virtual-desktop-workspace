// Placeholder test so the build and CTest wiring can be checked before real
// code exists. Replace or remove once the first real unit tests land.

#include <QString>
#include <QTest>

class SanityTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testToolchain()
    {
        const QString name = QStringLiteral("virtual-desktop-workspace");
        QCOMPARE(name.size(), 25);
        QVERIFY(__cplusplus >= 202002L);
    }
};

QTEST_GUILESS_MAIN(SanityTest)

#include "sanitytest.moc"
