// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "configwriter.h"
#include "config.h"

#include <KConfig>
#include <KConfigGroup>

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Vdw;

class ConfigWriterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void roundTrip();
    void exactFormat();
    void keepsGeneralKeys();
    void slugs();
    void validation();
};

static QList<Workspace> sample()
{
    Workspace a;
    a.id = "3-konsoles";
    a.name = "3 Konsoles";
    a.icon = "utilities-terminal";
    a.slots = {
        {"org.kde.konsole", "", QRectF(0.5, 0, 0.5, 0.5)},
        {"org.kde.konsole", "--workdir /tmp", QRectF(0.5, 0.5, 0.5, 0.5)},
        {"org.kde.dolphin", "", QRectF(0.0, 0.0, 0.333333, 1.0)},
    };
    Workspace b;
    b.id = "empty-one";
    b.name = "Empty one";
    b.icon = "";
    Workspace c;
    c.id = "alpha";
    c.name = "Alpha";
    c.icon = "kate";
    c.slots = {{"org.kde.kate", "-n \"a b\"", QRectF(0.25, 0.125, 0.75, 0.875)}};
    // Deliberately not sorted by name, so only the list order can explain Order.
    return {a, b, c};
}

void ConfigWriterTest::roundTrip()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("vdwrc");
    const QList<Workspace> input = sample();
    {
        KConfig file(path, KConfig::SimpleConfig);
        ConfigWriter::write(file, input, "alpha");
        QVERIFY(file.sync());
    }
    const KConfig file(path, KConfig::SimpleConfig);
    const Config loaded = Config::load(file);
    QVERIFY2(loaded.warnings().isEmpty(), qPrintable(loaded.warnings().join("; ")));
    QCOMPARE(loaded.defaultWorkspace(), QStringLiteral("alpha"));
    QCOMPARE(loaded.workspaces().size(), input.size());
    for (int i = 0; i < input.size(); ++i) {
        const Workspace &want = input.at(i);
        const Workspace &got = loaded.workspaces().at(i);
        QCOMPARE(got.id, want.id);
        QCOMPARE(got.name, want.name);
        QCOMPARE(got.icon, want.icon);
        QCOMPARE(got.order, i);
        QCOMPARE(got.slots.size(), want.slots.size());
        for (int s = 0; s < want.slots.size(); ++s) {
            QCOMPARE(got.slots.at(s).app, want.slots.at(s).app);
            QCOMPARE(got.slots.at(s).args, want.slots.at(s).args);
            const QRectF g = got.slots.at(s).geometry;
            const QRectF w = want.slots.at(s).geometry;
            QVERIFY(qAbs(g.x() - w.x()) < 1e-6);
            QVERIFY(qAbs(g.y() - w.y()) < 1e-6);
            QVERIFY(qAbs(g.width() - w.width()) < 1e-6);
            QVERIFY(qAbs(g.height() - w.height()) < 1e-6);
        }
    }
}

void ConfigWriterTest::exactFormat()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("vdwrc");
    Workspace a;
    a.id = "3-konsoles";
    a.name = "3 Konsoles";
    a.icon = "utilities-terminal";
    a.slots = {{"org.kde.konsole", "", QRectF(0.5, 0, 0.5, 0.5)}};
    {
        KConfig file(path, KConfig::SimpleConfig);
        ConfigWriter::write(file, {a}, "3-konsoles");
        QVERIFY(file.sync());
    }
    QFile f(path);
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QString text = QString::fromUtf8(f.readAll());
    QVERIFY(text.contains("[General]\nDefaultWorkspace=3-konsoles\n"));
    QVERIFY(text.contains("[Workspace][3-konsoles]\nIcon=utilities-terminal\nName=3 Konsoles\nOrder=0\n")
            || text.contains("[Workspace][3-konsoles]\nName=3 Konsoles\n"));
    QVERIFY(text.contains("[Workspace][3-konsoles][Slot][0]\nApp=org.kde.konsole\nArgs=\nGeometry=0.5,0,0.5,0.5\n"));
}

void ConfigWriterTest::keepsGeneralKeys()
{
    QTemporaryDir dir;
    const QString path = dir.filePath("vdwrc");
    {
        KConfig file(path, KConfig::SimpleConfig);
        file.group("General").writeEntry("HoldThresholdMs", 400);
        file.group("General").writeEntry("DefaultWorkspace", "old");
        file.group("Workspace").group("old").writeEntry("Name", "Old");
        file.sync();
    }
    {
        KConfig file(path, KConfig::SimpleConfig);
        ConfigWriter::write(file, sample(), QString());
        file.sync();
    }
    const Config loaded = Config::load(KConfig(path, KConfig::SimpleConfig));
    QCOMPARE(loaded.holdThresholdMs(), 400);
    QCOMPARE(loaded.defaultWorkspace(), QString());
    QVERIFY(!loaded.find("old"));
    QVERIFY(loaded.find("alpha"));
}

void ConfigWriterTest::slugs()
{
    QCOMPARE(ConfigWriter::slugFromName("3 Konsoles", {}), QStringLiteral("3-konsoles"));
    QCOMPARE(ConfigWriter::slugFromName("  Web & Mail!! ", {}), QStringLiteral("web-mail"));
    QCOMPARE(ConfigWriter::slugFromName(QString::fromUtf8("Café"), {}), QStringLiteral("cafe"));
    QCOMPARE(ConfigWriter::slugFromName(QString::fromUtf8("テスト"), {}), QStringLiteral("workspace"));
    QCOMPARE(ConfigWriter::slugFromName("", {}), QStringLiteral("workspace"));
    QCOMPARE(ConfigWriter::slugFromName("Dev", {"dev"}), QStringLiteral("dev-2"));
    QCOMPARE(ConfigWriter::slugFromName("Dev", {"dev", "dev-2"}), QStringLiteral("dev-3"));
    QVERIFY(Config::isValidId(ConfigWriter::slugFromName("Any *** name 42", {})));
}

void ConfigWriterTest::validation()
{
    QVERIFY(ConfigWriter::validate(sample(), "alpha").isEmpty());
    QVERIFY(ConfigWriter::validate({}, QString()).isEmpty());

    QList<Workspace> ws = sample();
    ws[0].slots[0].app.clear();
    QCOMPARE(ConfigWriter::validate(ws, QString()).size(), 1);

    ws = sample();
    ws[0].slots[0].geometry = QRectF(0, 0, 0, 0.5);
    QCOMPARE(ConfigWriter::validate(ws, QString()).size(), 1);
    ws[0].slots[0].geometry = QRectF(1.5, 0, 0.5, 0.5);
    QCOMPARE(ConfigWriter::validate(ws, QString()).size(), 1);

    ws = sample();
    ws[1].id = "alpha";
    QVERIFY(!ConfigWriter::validate(ws, QString()).isEmpty());
    ws = sample();
    ws[1].id = "Bad Id";
    QVERIFY(!ConfigWriter::validate(ws, QString()).isEmpty());
    ws = sample();
    ws[1].name = "  ";
    QVERIFY(!ConfigWriter::validate(ws, QString()).isEmpty());
    QVERIFY(!ConfigWriter::validate(sample(), "nope").isEmpty());
}

QTEST_APPLESS_MAIN(ConfigWriterTest)
#include "configwritertest.moc"
