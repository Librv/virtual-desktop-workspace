#include "workspacemodel.h"

#include <QSignalSpy>
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
    void mergeNew();
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

QTEST_APPLESS_MAIN(WorkspaceModelTest)
#include "workspacemodeltest.moc"
