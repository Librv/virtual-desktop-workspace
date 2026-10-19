// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "workspacemodel.h"

#include "configwriter.h"

#include <KConfig>
#include <KConfigGroup>

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Vdw;

class WorkspaceModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void addUniqueIds();
    void removeClearsDefault();
    void moveAndDefault();
    void slotEditing();
    void slotEditKeepsProfile();
    void profileChoices_data();
    void profileChoices();
    void slotProfileField();
    void slotProfileClearedOnAppChange();
    void slotProfileWritten();
    void slotMoving();
    void mergeNew();
    void mergeNewKeepsDeletions();
};

void WorkspaceModelTest::addUniqueIds()
{
    WorkspaceModel m;
    QSignalSpy changed(&m, &WorkspaceModel::changed);
    QCOMPARE(m.addWorkspace("Dev"), 0);
    QCOMPARE(m.addWorkspace("Dev"), 1);
    QCOMPARE(m.index(0).data(WorkspaceModel::IdRole).toString(), QStringLiteral("dev"));
    QCOMPARE(m.index(1).data(WorkspaceModel::IdRole).toString(), QStringLiteral("dev-2"));
    QCOMPARE(changed.size(), 2);
    // Renaming keeps the stable id.
    m.setName(0, "Something else");
    QCOMPARE(m.index(0).data(WorkspaceModel::IdRole).toString(), QStringLiteral("dev"));
}

void WorkspaceModelTest::removeClearsDefault()
{
    WorkspaceModel m;
    m.addWorkspace("A");
    m.addWorkspace("B");
    m.setDefaultRow(1);
    QCOMPARE(m.defaultWorkspace(), QStringLiteral("b"));
    m.removeWorkspace(1);
    QCOMPARE(m.defaultWorkspace(), QString());
    QCOMPARE(m.rowCount(), 1);
}

void WorkspaceModelTest::moveAndDefault()
{
    WorkspaceModel m;
    m.addWorkspace("A");
    m.addWorkspace("B");
    m.addWorkspace("C");
    QCOMPARE(m.moveWorkspace(2, -1), 1);
    QCOMPARE(m.moveWorkspace(0, 1), 1);
    QCOMPARE(m.moveWorkspace(0, -1), 0);
    QStringList order;
    for (const Workspace &ws : m.workspaces()) {
        order << ws.id;
    }
    QCOMPARE(order, QStringList({"c", "a", "b"}));
}

void WorkspaceModelTest::slotEditing()
{
    WorkspaceModel m;
    m.addWorkspace("A");
    SlotModel slots(&m);
    slots.setWorkspaceRow(0);
    QCOMPARE(slots.rowCount(), 0);
    m.addSlot(0);
    QCOMPARE(slots.rowCount(), 1);
    QCOMPARE(m.index(0).data(WorkspaceModel::SlotCountRole).toInt(), 1);

    QVERIFY(slots.setField(0, "app", " org.kde.konsole "));
    QVERIFY(slots.setField(0, "args", "--x"));
    QVERIFY(slots.setField(0, "geoX", 0.25));
    QVERIFY(slots.setField(0, "geoWidth", 0.5));
    // Out of range or zero-size values are refused and change nothing.
    QVERIFY(!slots.setField(0, "geoWidth", 0.0));
    QVERIFY(!slots.setField(0, "geoY", 1.5));
    QVERIFY(!slots.setField(0, "nope", 1));
    const Slot *s = m.slotAt(0, 0);
    QVERIFY(s);
    QCOMPARE(s->app, QStringLiteral("org.kde.konsole"));
    QCOMPARE(s->args, QStringLiteral("--x"));
    QCOMPARE(s->geometry, QRectF(0.25, 0.0, 0.5, 1.0));

    m.removeSlot(0, 0);
    QCOMPARE(slots.rowCount(), 0);
}

void WorkspaceModelTest::slotEditKeepsProfile()
{
    WorkspaceModel m;
    m.addWorkspace("A");
    m.addSlot(0);
    Slot s = *m.slotAt(0, 0);
    s.app = QStringLiteral("helium");
    s.profile = QStringLiteral("Profile 1");
    QVERIFY(m.setSlotAt(0, 0, s));
    SlotModel slots(&m);
    slots.setWorkspaceRow(0);
    QVERIFY(slots.setField(0, "app", "helium"));
    QVERIFY(slots.setField(0, "args", "--incognito"));
    QVERIFY(slots.setField(0, "geoX", 0.25));
    m.addSlot(0);
    m.moveSlot(0, 1, 0);
    QCOMPARE(m.slotAt(0, 1)->profile, QStringLiteral("Profile 1"));
    QCOMPARE(m.slotAt(0, 0)->profile, QString());
}

Q_DECLARE_METATYPE(QList<Vdw::ProfileChoice>)

void WorkspaceModelTest::profileChoices_data()
{
    using P = QList<BrowserProfile>;
    using C = QList<ProfileChoice>;
    QTest::addColumn<P>("profiles");
    QTest::addColumn<QString>("saved");
    QTest::addColumn<C>("expected");
    QTest::newRow("empty") << P() << QString() << C();
    QTest::newRow("empty list, saved is missing") << P() << QString("Profile 1") << C{{"Profile 1", "Profile 1", true}};
    QTest::newRow("natural sort by name") << P{{"A", "Profile 10"}, {"B", "Profile 2"}, {"C", "Profile 1"}} << QString()
                                          << C{{"C", "Profile 1", false}, {"B", "Profile 2", false}, {"A", "Profile 10", false}};
    QTest::newRow("same name: by dir") << P{{"Profile 2", "Work"}, {"Profile 1", "Work"}} << QString()
                                       << C{{"Profile 1", "Work", false}, {"Profile 2", "Work", false}};
    QTest::newRow("unicode names") << P{{"Profile 2", QString::fromUtf8("\u00c4rger")}, {"Default", QString::fromUtf8("\u65e5\u672c")}} << QString()
                                   << C{{"Profile 2", QString::fromUtf8("\u00c4rger"), false}, {"Default", QString::fromUtf8("\u65e5\u672c"), false}};
    QTest::newRow("saved missing: appended last") << P{{"Default", "Zed"}} << QString("Gone") << C{{"Default", "Zed", false}, {"Gone", "Gone", true}};
    QTest::newRow("saved present: not duplicated") << P{{"Default", "Person 1"}} << QString("Default") << C{{"Default", "Person 1", false}};
    QTest::newRow("saved empty adds nothing") << P{{"Default", "Person 1"}} << QString() << C{{"Default", "Person 1", false}};
}

void WorkspaceModelTest::profileChoices()
{
    QFETCH(QList<BrowserProfile>, profiles);
    QFETCH(QString, saved);
    QFETCH(QList<ProfileChoice>, expected);
    const QList<ProfileChoice> actual = Vdw::profileChoices(profiles, saved);
    QCOMPARE(actual.size(), expected.size());
    for (qsizetype i = 0; i < actual.size(); ++i) {
        QCOMPARE(actual.at(i).dir, expected.at(i).dir);
        QCOMPARE(actual.at(i).name, expected.at(i).name);
        QCOMPARE(actual.at(i).missing, expected.at(i).missing);
    }
}

void WorkspaceModelTest::slotProfileField()
{
    WorkspaceModel m;
    m.addWorkspace("A");
    m.addSlot(0);
    SlotModel slots(&m);
    slots.setWorkspaceRow(0);
    QVERIFY(slots.setField(0, "profile", "  Profile 1 "));
    QCOMPARE(slots.index(0).data(SlotModel::ProfileRole).toString(), QStringLiteral("Profile 1"));
    QCOMPARE(m.slotAt(0, 0)->profile, QStringLiteral("Profile 1"));
    QVERIFY(slots.setField(0, "profile", ""));
    QCOMPARE(m.slotAt(0, 0)->profile, QString());
}

void WorkspaceModelTest::slotProfileClearedOnAppChange()
{
    WorkspaceModel m;
    m.addWorkspace("A");
    m.addSlot(0);
    SlotModel slots(&m);
    slots.setWorkspaceRow(0);
    QVERIFY(slots.setField(0, "app", "helium"));
    QVERIFY(slots.setField(0, "profile", "Profile 1"));
    // Same app: the profile stays.
    QVERIFY(slots.setField(0, "app", " helium "));
    QCOMPARE(m.slotAt(0, 0)->profile, QStringLiteral("Profile 1"));
    // Other app: a profile folder belongs to one browser.
    QVERIFY(slots.setField(0, "app", "firefox"));
    QCOMPARE(m.slotAt(0, 0)->app, QStringLiteral("firefox"));
    QCOMPARE(m.slotAt(0, 0)->profile, QString());
}

void WorkspaceModelTest::slotProfileWritten()
{
    WorkspaceModel m;
    m.addWorkspace("A");
    m.addSlot(0);
    SlotModel slots(&m);
    slots.setWorkspaceRow(0);
    QVERIFY(slots.setField(0, "app", "helium"));
    QVERIFY(slots.setField(0, "profile", "Profile 1"));
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("vdwrc");
    {
        KConfig file(path, KConfig::SimpleConfig);
        ConfigWriter::write(file, m.workspaces(), m.defaultWorkspace());
    }
    const KConfig file(path, KConfig::SimpleConfig);
    const KConfigGroup slot = file.group(QStringLiteral("Workspace")).group(m.workspaces().at(0).id).group(QStringLiteral("Slot")).group(QStringLiteral("0"));
    QCOMPARE(slot.readEntry("App"), QStringLiteral("helium"));
    QCOMPARE(slot.readEntry("Profile"), QStringLiteral("Profile 1"));
}

void WorkspaceModelTest::slotMoving()
{
    WorkspaceModel m;
    m.addWorkspace("A");
    for (const char *app : {"a", "b", "c"}) {
        m.addSlot(0);
        Slot s = *m.slotAt(0, m.slotCount(0) - 1);
        s.app = QString::fromLatin1(app);
        QVERIFY(m.setSlotAt(0, m.slotCount(0) - 1, s));
    }
    auto order = [&m]() {
        QString out;
        for (int i = 0; i < m.slotCount(0); ++i) {
            out += m.slotAt(0, i)->app;
        }
        return out;
    };
    QSignalSpy changed(&m, &WorkspaceModel::changed);

    m.moveSlot(0, 0, 2);
    QCOMPARE(order(), QStringLiteral("bca"));
    QCOMPARE(changed.size(), 1);
    m.moveSlot(0, 2, 0);
    QCOMPARE(order(), QStringLiteral("abc"));
    QCOMPARE(changed.size(), 2);

    m.moveSlot(0, 1, 1);
    m.moveSlot(0, -1, 0);
    m.moveSlot(0, 0, 3);
    m.moveSlot(0, 3, 0);
    m.moveSlot(1, 0, 1);
    QCOMPARE(order(), QStringLiteral("abc"));
    QCOMPARE(changed.size(), 2);
}

void WorkspaceModelTest::mergeNew()
{
    WorkspaceModel m;
    m.addWorkspace("A");
    m.setName(0, "A edited");
    Workspace oldA;
    oldA.id = "a";
    oldA.name = "A on disk";
    Workspace fresh;
    fresh.id = "error-log";
    fresh.name = "Error log";
    QCOMPARE(m.mergeNew({oldA, fresh}), 1);
    QCOMPARE(m.rowCount(), 2);
    QCOMPARE(m.index(0).data(WorkspaceModel::NameRole).toString(), QStringLiteral("A edited"));
    QCOMPARE(m.index(1).data(WorkspaceModel::IdRole).toString(), QStringLiteral("error-log"));
    QCOMPARE(m.mergeNew({oldA, fresh}), 0);
    QCOMPARE(m.rowCount(), 2);
}

void WorkspaceModelTest::mergeNewKeepsDeletions()
{
    Workspace a;
    a.id = "a";
    a.name = "A";
    Workspace b;
    b.id = "b";
    b.name = "B";
    Workspace c;
    c.id = "c";
    c.name = "C";
    WorkspaceModel m;
    m.setWorkspaces({a, b}, QString());
    m.removeWorkspace(1);
    // Disk still has the deleted "b" and gained "c" from a capture.
    QCOMPARE(m.mergeNew({a, b, c}), 1);
    QCOMPARE(m.rowCount(), 2);
    QCOMPARE(m.index(1).data(WorkspaceModel::IdRole).toString(), QStringLiteral("c"));
    // Once saved without "b", a "b" that shows up on disk again is new.
    m.markSaved();
    QCOMPARE(m.mergeNew({a, b, c}), 1);
    QCOMPARE(m.rowCount(), 3);
}

QTEST_APPLESS_MAIN(WorkspaceModelTest)
#include "workspacemodeltest.moc"
