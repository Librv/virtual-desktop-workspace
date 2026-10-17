// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "config.h"

#include <KConfig>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>
#include <QTest>

using namespace Vdw;

class ConfigTest : public QObject
{
    Q_OBJECT

private:
    static Config parse(const QByteArray &text)
    {
        QTemporaryFile file;
        if (!file.open()) {
            qFatal("cannot create temporary file");
        }
        file.write(text);
        file.flush();
        return Config::load(KConfig(file.fileName(), KConfig::SimpleConfig));
    }

private Q_SLOTS:
    void testValid()
    {
        const Config c = parse(
            "[General]\n"
            "DefaultWorkspace=3-konsoles\n"
            "HoldThresholdMs=300\n"
            "ConfirmTimeoutMs=1500\n"
            "PlacementTimeoutMs=5000\n"
            "\n"
            "[Workspace][3-konsoles]\n"
            "Name=3 Konsoles\n"
            "Icon=utilities-terminal\n"
            "Order=0\n"
            "\n"
            "[Workspace][3-konsoles][Slot][0]\n"
            "App=org.kde.konsole\n"
            "Args=--workdir /tmp\n"
            "Geometry=0.5,0,0.5,0.5\n"
            "\n"
            "[Workspace][3-konsoles][Slot][1]\n"
            "App=org.kde.dolphin\n"
            "Geometry=0,0,0.5,1\n");
        QVERIFY(c.warnings().isEmpty());
        QCOMPARE(c.defaultWorkspace(), QStringLiteral("3-konsoles"));
        QCOMPARE(c.holdThresholdMs(), 300);
        QCOMPARE(c.confirmTimeoutMs(), 1500);
        QCOMPARE(c.placementTimeoutMs(), 5000);
        QCOMPARE(c.workspaces().size(), 1);
        const Workspace &ws = c.workspaces().first();
        QCOMPARE(ws.id, QStringLiteral("3-konsoles"));
        QCOMPARE(ws.name, QStringLiteral("3 Konsoles"));
        QCOMPARE(ws.icon, QStringLiteral("utilities-terminal"));
        QCOMPARE(ws.slots.size(), 2);
        QCOMPARE(ws.slots[0].app, QStringLiteral("org.kde.konsole"));
        QCOMPARE(ws.slots[0].args, QStringLiteral("--workdir /tmp"));
        QCOMPARE(ws.slots[0].geometry, QRectF(0.5, 0, 0.5, 0.5));
        QCOMPARE(ws.slots[1].args, QString());
        QCOMPARE(ws.slots[1].geometry, QRectF(0, 0, 0.5, 1));
    }

    void testDefaults()
    {
        const Config c = parse("");
        QVERIFY(c.workspaces().isEmpty());
        QVERIFY(c.defaultWorkspace().isEmpty());
        QCOMPARE(c.holdThresholdMs(), 250);
        QCOMPARE(c.confirmTimeoutMs(), 2000);
        QCOMPARE(c.placementTimeoutMs(), 10000);
        QCOMPARE(c.toJson(), QByteArray("[]"));
    }

    void testNameFallsBackToId()
    {
        const Config c = parse("[Workspace][abc]\nOrder=1\n");
        QCOMPARE(c.workspaces().size(), 1);
        QCOMPARE(c.workspaces().first().name, QStringLiteral("abc"));
    }

    void testInvalidId_data()
    {
        QTest::addColumn<QByteArray>("header");
        QTest::newRow("uppercase") << QByteArray("[Workspace][Bad]");
        QTest::newRow("underscore") << QByteArray("[Workspace][a_b]");
        QTest::newRow("space") << QByteArray("[Workspace][a b]");
    }
    void testInvalidId()
    {
        QFETCH(QByteArray, header);
        const Config c = parse(header + "\nName=Bad\n\n[Workspace][good]\nName=Good\n");
        QCOMPARE(c.workspaces().size(), 1);
        QCOMPARE(c.workspaces().first().id, QStringLiteral("good"));
        QCOMPARE(c.warnings().size(), 1);
    }

    void testInvalidSlots_data()
    {
        QTest::addColumn<QByteArray>("slot");
        QTest::newRow("missing App") << QByteArray("[Workspace][w][Slot][0]\nGeometry=0,0,1,1\n");
        QTest::newRow("empty App") << QByteArray("[Workspace][w][Slot][0]\nApp=\nGeometry=0,0,1,1\n");
        QTest::newRow("missing Geometry") << QByteArray("[Workspace][w][Slot][0]\nApp=a\n");
        QTest::newRow("three numbers") << QByteArray("[Workspace][w][Slot][0]\nApp=a\nGeometry=0,0,1\n");
        QTest::newRow("five numbers") << QByteArray("[Workspace][w][Slot][0]\nApp=a\nGeometry=0,0,1,1,1\n");
        QTest::newRow("not a number") << QByteArray("[Workspace][w][Slot][0]\nApp=a\nGeometry=0,0,x,1\n");
        QTest::newRow("above one") << QByteArray("[Workspace][w][Slot][0]\nApp=a\nGeometry=0,0,1.5,1\n");
        QTest::newRow("negative") << QByteArray("[Workspace][w][Slot][0]\nApp=a\nGeometry=-0.1,0,0.5,1\n");
        QTest::newRow("zero width") << QByteArray("[Workspace][w][Slot][0]\nApp=a\nGeometry=0,0,0,1\n");
        QTest::newRow("bad slot name") << QByteArray("[Workspace][w][Slot][first]\nApp=a\nGeometry=0,0,1,1\n");
        QTest::newRow("negative slot name") << QByteArray("[Workspace][w][Slot][-1]\nApp=a\nGeometry=0,0,1,1\n");
    }
    void testInvalidSlots()
    {
        QFETCH(QByteArray, slot);
        // The valid slot 5 must survive; the workspace itself stays.
        const Config c = parse("[Workspace][w]\nName=W\n\n" + slot + "\n[Workspace][w][Slot][5]\nApp=ok\nGeometry=0,0,1,1\n");
        QCOMPARE(c.workspaces().size(), 1);
        QCOMPARE(c.workspaces().first().slots.size(), 1);
        QCOMPARE(c.workspaces().first().slots.first().app, QStringLiteral("ok"));
        QCOMPARE(c.warnings().size(), 1);
    }

    void testUnknownKeys()
    {
        const Config c = parse(
            "[General]\nFoo=1\n\n"
            "[Workspace][w]\nName=W\nColor=red\n\n"
            "[Workspace][w][Slot][0]\nApp=a\nGeometry=0,0,1,1\nExtra=1\n");
        QCOMPARE(c.warnings().size(), 3);
        QCOMPARE(c.workspaces().size(), 1);
        QCOMPARE(c.workspaces().first().slots.size(), 1);
    }

    void testBadGeneral()
    {
        const Config c = parse("[General]\nHoldThresholdMs=abc\nConfirmTimeoutMs=-5\nDefaultWorkspace=nope\n");
        QCOMPARE(c.holdThresholdMs(), 250);
        QCOMPARE(c.confirmTimeoutMs(), 2000);
        QVERIFY(c.defaultWorkspace().isEmpty());
        QCOMPARE(c.warnings().size(), 3);
    }

    void testSlotOrderIsNumeric()
    {
        const Config c = parse(
            "[Workspace][w]\n\n"
            "[Workspace][w][Slot][10]\nApp=ten\nGeometry=0,0,1,1\n\n"
            "[Workspace][w][Slot][2]\nApp=two\nGeometry=0,0,1,1\n");
        const QList<Slot> slots = c.workspaces().first().slots;
        QCOMPARE(slots.size(), 2);
        QCOMPARE(slots[0].app, QStringLiteral("two"));
        QCOMPARE(slots[1].app, QStringLiteral("ten"));
    }

    void testWorkspaceOrdering()
    {
        const Config c = parse(
            "[Workspace][c]\nName=Charlie\nOrder=1\n\n"
            "[Workspace][b]\nName=bravo\nOrder=1\n\n"
            "[Workspace][a]\nName=Zulu\nOrder=0\n\n"
            "[Workspace][d]\nName=Alpha\nOrder=2\n");
        QStringList ids;
        for (const Workspace &ws : c.workspaces()) {
            ids << ws.id;
        }
        QCOMPARE(ids, (QStringList{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("d")}));
    }

    void testJson()
    {
        const Config c = parse(
            "[General]\nDefaultWorkspace=b\n\n"
            "[Workspace][a]\nName=A\nOrder=0\n\n"
            "[Workspace][b]\nName=B\nIcon=ic\nOrder=1\n\n"
            "[Workspace][b][Slot][0]\nApp=org.kde.kate\nArgs=-n\nGeometry=0.25,0,0.5,1\n");
        const QJsonArray array = QJsonDocument::fromJson(c.toJson()).array();
        QCOMPARE(array.size(), 2);
        const QJsonObject a = array[0].toObject();
        QCOMPARE(a[QStringLiteral("id")].toString(), QStringLiteral("a"));
        QCOMPARE(a[QStringLiteral("isDefault")].toBool(), false);
        QVERIFY(a[QStringLiteral("slots")].toArray().isEmpty());
        const QJsonObject b = array[1].toObject();
        QCOMPARE(b[QStringLiteral("name")].toString(), QStringLiteral("B"));
        QCOMPARE(b[QStringLiteral("icon")].toString(), QStringLiteral("ic"));
        QCOMPARE(b[QStringLiteral("isDefault")].toBool(), true);
        const QJsonArray slots = b[QStringLiteral("slots")].toArray();
        QCOMPARE(slots.size(), 1);
        const QJsonObject slot = slots[0].toObject();
        QCOMPARE(slot[QStringLiteral("app")].toString(), QStringLiteral("org.kde.kate"));
        QCOMPARE(slot[QStringLiteral("args")].toString(), QStringLiteral("-n"));
        QCOMPARE(slot[QStringLiteral("geometry")].toObject()[QStringLiteral("width")].toDouble(), 0.5);
    }
};

QTEST_GUILESS_MAIN(ConfigTest)

#include "configtest.moc"
