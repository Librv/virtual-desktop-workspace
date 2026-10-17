// Starts vdwd inside the nested KWin session (scripts/dev-session.sh) and
// checks vdwctl against it. Skips when not run inside that session, so it
// never touches the live session bus:
//   scripts/dev-session.sh run -- ctest --preset dev -R integrationtest

#include "layout.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

#include <memory>

namespace
{
const QString testService = QStringLiteral("org.kde.vdw.T2test");

// Receives the window list from the KWin script the test loads.
class Receiver : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.vdw.T2test")
public Q_SLOTS:
    void Report(const QString &json)
    {
        reports.append(json);
    }

public:
    QStringList reports;
};
}

class IntegrationTest : public QObject
{
    Q_OBJECT

private:
    struct Result {
        int exitCode = -1;
        QString out;
    };

    Result ctl(const QStringList &args)
    {
        QProcess p;
        p.start(QStringLiteral(VDWCTL_PATH), args);
        if (!p.waitForFinished(15000)) {
            p.kill();
            return {};
        }
        return {p.exitCode(), QString::fromUtf8(p.readAllStandardOutput()).trimmed()};
    }

    // Runs a KWin script in the nested KWin (never the live one: the session
    // bus was checked in initTestCase).
    bool runScript(const QString &name, const QString &source)
    {
        const QString path = m_dir.filePath(name + QStringLiteral(".js"));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        file.write(source.toUtf8());
        file.close();
        QDBusInterface scripting(QStringLiteral("org.kde.KWin"), QStringLiteral("/Scripting"), QStringLiteral("org.kde.kwin.Scripting"));
        scripting.call(QStringLiteral("unloadScript"), name);
        const QDBusReply<int> id = scripting.call(QStringLiteral("loadScript"), path, name);
        if (!id.isValid() || id.value() < 0) {
            return false;
        }
        // Scripting.start() instead of Script<id>.run: KWin can hand out the
        // same id twice after an unload, then the script object path is wrong.
        const bool ok = scripting.call(QStringLiteral("start")).type() != QDBusMessage::ErrorMessage;
        return ok;
    }

    struct Window {
        QString cls;
        QRect rect;
    };
    struct Snapshot {
        QRect area;
        QList<Window> windows;
    };

    Snapshot snapshot()
    {
        m_receiver.reports.clear();
        const QString source = QStringLiteral(
            "const d = workspace.currentDesktop;\n"
            "const a = workspace.clientArea(KWin.PlacementArea, workspace.activeScreen, d);\n"
            "const ws = workspace.windowList().filter(function(w) { return w.normalWindow && w.minimizable; }).map(function(w) {\n"
            "  const g = w.frameGeometry;\n"
            "  return {cls: w.resourceClass, x: Math.round(g.x), y: Math.round(g.y), width: Math.round(g.width), height: Math.round(g.height)};\n"
            "});\n"
            "callDBus('org.kde.vdw.T2test', '/T2', 'org.kde.vdw.T2test', 'Report',\n"
            "  JSON.stringify({area: {x: a.x, y: a.y, width: a.width, height: a.height}, windows: ws}));\n");
        Snapshot snap;
        if (!runScript(QStringLiteral("vdw-t2-list"), source)) {
            return snap;
        }
        if (!QTest::qWaitFor(
                [this] {
                    return !m_receiver.reports.isEmpty();
                },
                5000)) {
            return snap;
        }
        const QJsonObject o = QJsonDocument::fromJson(m_receiver.reports.last().toUtf8()).object();
        const QJsonObject a = o[QStringLiteral("area")].toObject();
        snap.area =
            QRect(a[QStringLiteral("x")].toInt(), a[QStringLiteral("y")].toInt(), a[QStringLiteral("width")].toInt(), a[QStringLiteral("height")].toInt());
        for (const QJsonValue &v : o[QStringLiteral("windows")].toArray()) {
            const QJsonObject w = v.toObject();
            snap.windows.append({w[QStringLiteral("cls")].toString(),
                                 QRect(w[QStringLiteral("x")].toInt(),
                                       w[QStringLiteral("y")].toInt(),
                                       w[QStringLiteral("width")].toInt(),
                                       w[QStringLiteral("height")].toInt())});
        }
        return snap;
    }

    static bool near(const QRect &a, const QRect &b)
    {
        return qAbs(a.x() - b.x()) <= 2 && qAbs(a.y() - b.y()) <= 2 && qAbs(a.width() - b.width()) <= 2 && qAbs(a.height() - b.height()) <= 2;
    }

    // True if every expected rect (each repeated `times`) has its own window.
    bool windowsAtSlots(int times, int expectedTotal)
    {
        const Snapshot snap = snapshot();
        if (snap.windows.size() != expectedTotal) {
            return false;
        }
        QList<Window> free = snap.windows;
        const QList<QRectF> fractions = {QRectF(0, 0, 0.5, 1), QRectF(0.5, 0, 0.5, 0.5), QRectF(0.5, 0.5, 0.5, 0.5)};
        for (int n = 0; n < times; ++n) {
            for (const QRectF &f : fractions) {
                const QRect want = Vdw::Layout::slotRect(snap.area, f);
                bool found = false;
                for (int i = 0; i < free.size(); ++i) {
                    if (near(free[i].rect, want)) {
                        free.removeAt(i);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return false;
                }
            }
        }
        return true;
    }

    QTemporaryDir m_dir;
    Receiver m_receiver;
    std::unique_ptr<QProcess> m_daemon;

private Q_SLOTS:
    void initTestCase()
    {
        // dev-session.sh run sets WAYLAND_DISPLAY=wayland-vdw and its own bus.
        if (qEnvironmentVariable("WAYLAND_DISPLAY") != QLatin1String("wayland-vdw")) {
            QSKIP("not inside the nested session (use scripts/dev-session.sh run -- ctest ...)");
        }
        const QString address = qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS");
        const QString live = QStringLiteral("/run/user/%1/bus").arg(QString::number(::getuid()));
        // The nested bus socket lives under the dev-session state dir
        // (vdw-dev-session by default, vdw-<task> by convention).
        if (address.isEmpty() || address.contains(live) || !address.contains(QLatin1String("/vdw"))) {
            QSKIP("session bus is missing or looks like the live one");
        }
        if (!QDBusConnection::sessionBus().isConnected()) {
            QSKIP("nested session bus is not reachable");
        }
        QVERIFY(m_dir.isValid());

        QFile cfg(m_dir.filePath(QStringLiteral("vdwrc")));
        QVERIFY(cfg.open(QIODevice::WriteOnly));
        cfg.write(
            "[General]\nDefaultWorkspace=two\n\n"
            "[Workspace][two]\nName=Two\nOrder=1\n\n"
            "[Workspace][two][Slot][0]\nApp=org.kde.konsole\nGeometry=0,0,0.5,1\n\n"
            "[Workspace][two][Slot][1]\nApp=org.kde.konsole\nGeometry=0.5,0,0.5,0.5\n\n"
            "[Workspace][two][Slot][2]\nApp=org.kde.konsole\nGeometry=0.5,0.5,0.5,0.5\n\n"
            "[Workspace][one]\nName=One\nOrder=0\nIcon=folder\n");
        cfg.close();

        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerObject(QStringLiteral("/T2"), &m_receiver, QDBusConnection::ExportAllSlots));
        QVERIFY(bus.registerService(testService));

        m_daemon = std::make_unique<QProcess>();
        m_daemon->setProcessChannelMode(QProcess::ForwardedChannels);
        m_daemon->start(QStringLiteral(VDWD_PATH), {QStringLiteral("--config"), cfg.fileName()});
        QVERIFY(m_daemon->waitForStarted());

        auto *iface = QDBusConnection::sessionBus().interface();
        QTRY_VERIFY_WITH_TIMEOUT(iface->isServiceRegistered(QStringLiteral("io.github.librv.vdw")), 15000);
    }

    void cleanupTestCase()
    {
        if (QDBusConnection::sessionBus().interface()->isServiceRegistered(testService)) {
            runScript(QStringLiteral("vdw-t2-close"),
                      QStringLiteral("workspace.windowList().forEach(function(w) { if (w.normalWindow && w.minimizable) { w.closeWindow(); } });\n"));
            QTest::qWait(1000);
        }
        if (m_daemon) {
            m_daemon->terminate();
            if (!m_daemon->waitForFinished(5000)) {
                m_daemon->kill();
                m_daemon->waitForFinished();
            }
        }
    }

    void testList()
    {
        const Result r = ctl({QStringLiteral("list")});
        QCOMPARE(r.exitCode, 0);
        const QJsonArray array = QJsonDocument::fromJson(r.out.toUtf8()).array();
        QCOMPARE(array.size(), 2);
        QCOMPARE(array[0].toObject()[QStringLiteral("id")].toString(), QStringLiteral("one"));
        QCOMPARE(array[1].toObject()[QStringLiteral("id")].toString(), QStringLiteral("two"));
        QVERIFY(array[1].toObject()[QStringLiteral("isDefault")].toBool());
    }

    // Nested desktop is empty: two non-forced opens at the same time (one
    // through vdwctl, one raw D-Bus call) give exactly one "opened" and one
    // "busy", and only three Konsoles land on their slot rects.
    void testOpenPlacesWindows()
    {
        QCOMPARE(snapshot().windows.size(), 0);

        QProcess process;
        process.start(QStringLiteral(VDWCTL_PATH), {QStringLiteral("open"), QStringLiteral("two")});
        QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("io.github.librv.vdw"),
                                                              QStringLiteral("/Manager"),
                                                              QStringLiteral("io.github.librv.vdw.Manager"),
                                                              QStringLiteral("OpenWorkspace"));
        message.setArguments({QStringLiteral("two"), false});
        QDBusPendingCall call = QDBusConnection::sessionBus().asyncCall(message, 15000);
        QDBusPendingCallWatcher watcher(call);
        QVERIFY(QTest::qWaitFor(
            [&] {
                return call.isFinished();
            },
            15000));
        QVERIFY(QTest::qWaitFor(
            [&] {
                return process.state() == QProcess::NotRunning;
            },
            15000));
        QVERIFY(!call.isError());
        const QString direct = QDBusPendingReply<QString>(call).value();
        const QString viaCtl = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
        QCOMPARE(QStringList({direct, viaCtl}).count(QStringLiteral("opened")), 1);
        QCOMPARE(QStringList({direct, viaCtl}).count(QStringLiteral("busy")), 1);
        QTRY_VERIFY_WITH_TIMEOUT(windowsAtSlots(1, 3), 20000);
        QTest::qWait(1500);
        QVERIFY(windowsAtSlots(1, 3));
    }

    // Desktop is not empty now: busy, and nothing is launched.
    void testOpenBusy()
    {
        const Result r = ctl({QStringLiteral("open"), QStringLiteral("two")});
        QCOMPARE(r.out, QStringLiteral("busy"));
        QCOMPARE(r.exitCode, 0);
        QTest::qWait(3000);
        QVERIFY(windowsAtSlots(1, 3));
    }

    void testOpenForce()
    {
        const Result r = ctl({QStringLiteral("open"), QStringLiteral("two"), QStringLiteral("--force")});
        QCOMPARE(r.out, QStringLiteral("opened"));
        QTRY_VERIFY_WITH_TIMEOUT(windowsAtSlots(2, 6), 20000);
    }

    void testOpenUnknown()
    {
        const Result r = ctl({QStringLiteral("open"), QStringLiteral("nope"), QStringLiteral("--force")});
        QCOMPARE(r.exitCode, 1);
        QCOMPARE(r.out, QStringLiteral("error: unknown workspace"));
    }

    void testReload()
    {
        const Result r = ctl({QStringLiteral("reload")});
        QCOMPARE(r.exitCode, 0);
        QCOMPARE(ctl({QStringLiteral("list")}).exitCode, 0);
    }
};

QTEST_GUILESS_MAIN(IntegrationTest)

#include "integrationtest.moc"
