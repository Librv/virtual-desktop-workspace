// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

// Starts vdwd inside the nested KWin session (scripts/dev-session.sh) and
// checks vdwctl, the /Debug gesture input and (when the helpers are built)
// real key events against it. Skips when not run inside that session, so it
// never touches the live session bus:
//   scripts/dev-session.sh run -- ctest --preset dev -R integrationtest

#include "layout.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusReply>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QTest>

#include <unistd.h>

#include <memory>

namespace
{
const QString testService = QStringLiteral("org.kde.vdw.T2test");

// One key sequence of kglobalaccel on the wire: `(ai)`.
struct KeySequence {
    QList<int> keys;
};

QDBusArgument &operator<<(QDBusArgument &argument, const KeySequence &sequence)
{
    argument.beginStructure();
    argument << sequence.keys;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, KeySequence &sequence)
{
    argument.beginStructure();
    argument >> sequence.keys;
    argument.endStructure();
    return argument;
}

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

Q_DECLARE_METATYPE(KeySequence)

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

    // Calls a method of the /Debug object (never the live session: the bus was
    // checked in initTestCase).
    void debugCall(const QString &method, const QVariantList &args)
    {
        QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("io.github.librv.vdw"),
                                                              QStringLiteral("/Debug"),
                                                              QStringLiteral("io.github.librv.vdw.Debug"),
                                                              method);
        message.setArguments(args);
        const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 5000);
        QVERIFY2(reply.type() == QDBusMessage::ReplyMessage, qPrintable(reply.errorMessage()));
    }
    void tap(const QString &shortcut)
    {
        debugCall(QStringLiteral("Press"), {shortcut});
        QTest::qWait(60);
        debugCall(QStringLiteral("Release"), {shortcut});
    }

    // Number of KWin windows (stacking order, so layer-shell surfaces too)
    // whose resource class is `cls`.
    int windowsOfClass(const QString &cls)
    {
        m_receiver.reports.clear();
        const QString source = QStringLiteral(
            "callDBus('org.kde.vdw.T2test', '/T2', 'org.kde.vdw.T2test', 'Report',\n"
            "  JSON.stringify(workspace.stackingOrder.map(function(w) { return w.resourceClass; })));\n");
        if (!runScript(QStringLiteral("vdw-t3b-classes"), source)
            || !QTest::qWaitFor(
                [this] {
                    return !m_receiver.reports.isEmpty();
                },
                5000)) {
            return -1;
        }
        int count = 0;
        for (const QJsonValue &v : QJsonDocument::fromJson(m_receiver.reports.last().toUtf8()).array()) {
            count += v.toString() == cls;
        }
        return count;
    }

    // Frame geometries of the KWin windows whose resource class is `cls`.
    // Empty on a script failure, so check the count before the rectangles.
    QList<QRectF> geometriesOfClass(const QString &cls)
    {
        m_receiver.reports.clear();
        const QString source = QStringLiteral(
            "callDBus('org.kde.vdw.T2test', '/T2', 'org.kde.vdw.T2test', 'Report',\n"
            "  JSON.stringify(workspace.stackingOrder.map(function(w) { return [w.resourceClass, w.frameGeometry.x, w.frameGeometry.y, "
            "w.frameGeometry.width, w.frameGeometry.height]; })));\n");
        QList<QRectF> rects;
        if (!runScript(QStringLiteral("vdw-toast-geometries"), source)
            || !QTest::qWaitFor(
                [this] {
                    return !m_receiver.reports.isEmpty();
                },
                5000)) {
            return rects;
        }
        for (const QJsonValue &v : QJsonDocument::fromJson(m_receiver.reports.last().toUtf8()).array()) {
            const QJsonArray a = v.toArray();
            if (a.at(0).toString() == cls) {
                rects.append(QRectF(a.at(1).toDouble(), a.at(2).toDouble(), a.at(3).toDouble(), a.at(4).toDouble()));
            }
        }
        return rects;
    }

    // True if exactly one vdw-ui surface is up and it sits at the top, centered.
    bool toastAtTopCenter()
    {
        const QList<QRectF> rects = geometriesOfClass(QStringLiteral("vdw-ui"));
        if (rects.size() != 1) {
            return false;
        }
        const QRect area = snapshot().area;
        const QRectF r = rects.first();
        return r.top() < area.top() + 100 && qAbs(r.center().x() - area.center().x()) < 5 && r.width() < area.width() / 2;
    }

    // Pids of the KWin windows whose resource class is `cls`.
    QList<qint64> pidsOfClass(const QString &cls)
    {
        m_receiver.reports.clear();
        const QString source = QStringLiteral(
            "callDBus('org.kde.vdw.T2test', '/T2', 'org.kde.vdw.T2test', 'Report',\n"
            "  JSON.stringify(workspace.stackingOrder.map(function(w) { return [w.resourceClass, w.pid]; })));\n");
        QList<qint64> pids;
        if (!runScript(QStringLiteral("vdw-pids"), source)
            || !QTest::qWaitFor(
                [this] {
                    return !m_receiver.reports.isEmpty();
                },
                5000)) {
            return pids;
        }
        for (const QJsonValue &v : QJsonDocument::fromJson(m_receiver.reports.last().toUtf8()).array()) {
            const QJsonArray a = v.toArray();
            if (a.at(0).toString() == cls) {
                pids.append(static_cast<qint64>(a.at(1).toDouble()));
            }
        }
        return pids;
    }

    // The `a(ai)` of kglobalaccel for at most one chord.
    static QList<KeySequence> keysArgument(int chord)
    {
        return chord == 0 ? QList<KeySequence>() : QList<KeySequence>{{{chord, 0, 0, 0}}};
    }

    // Calls kglobalaccel of the nested session about the "main" action.
    QDBusMessage accelCall(const QString &method, const QVariantList &extra)
    {
        QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"),
                                                              QStringLiteral("/kglobalaccel"),
                                                              QStringLiteral("org.kde.KGlobalAccel"),
                                                              method);
        const QStringList id = {QStringLiteral("vdw"),
                                QStringLiteral("main"),
                                QStringLiteral("Virtual Desktop Workspace"),
                                QStringLiteral("Open default workspace, hold for the workspace menu")};
        message.setArguments(QVariantList{id} + extra);
        return QDBusConnection::sessionBus().call(message, QDBus::Block, 5000);
    }

    // First chord of the "main" action's first key sequence, -1 on error.
    int mainChord()
    {
        const QDBusMessage reply = accelCall(QStringLiteral("shortcutKeys"), {});
        if (reply.type() != QDBusMessage::ReplyMessage) {
            return -1;
        }
        const QList<KeySequence> keys = qdbus_cast<QList<KeySequence>>(reply.arguments().at(0));
        const int chord = keys.isEmpty() ? 0 : keys.first().keys.value(0);
        return chord;
    }

    bool closeAll()
    {
        runScript(QStringLiteral("vdw-t3b-close"),
                  QStringLiteral("workspace.windowList().forEach(function(w) { if (w.normalWindow && w.minimizable) { w.closeWindow(); } });\n"));
        return QTest::qWaitFor(
            [this] {
                return snapshot().windows.isEmpty();
            },
            15000);
    }

    // True if the desktop holds exactly one window and it covers the area.
    bool oneWindowFillsArea()
    {
        const Snapshot snap = snapshot();
        return snap.windows.size() == 1 && near(snap.windows.first().rect, snap.area);
    }

    // Starts a helper binary of the nested session; null if it cannot start.
    std::unique_ptr<QProcess> startHelper(const QString &path, const QStringList &args)
    {
        auto process = std::make_unique<QProcess>();
        process->setProcessChannelMode(QProcess::ForwardedChannels);
        process->start(path, args);
        if (!process->waitForStarted()) {
            return nullptr;
        }
        return process;
    }

    // Writes the .desktop file that lets KWin hand org_kde_kwin_fake_input to
    // `exe`, into the nested XDG_DATA_HOME only (S1).
    bool authorizeFakeInput(const QString &exe, const QString &name)
    {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        if (!dir.contains(QLatin1String("/vdw"))) {
            return false;
        }
        QDir().mkpath(dir + QStringLiteral("/applications"));
        QFile file(dir + QStringLiteral("/applications/") + name + QStringLiteral(".desktop"));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        file.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=%1\nExec=%2\nNoDisplay=true\n"
                                  "X-KDE-Wayland-Interfaces=org_kde_kwin_fake_input\n")
                       .arg(name, exe)
                       .toUtf8());
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
        qDBusRegisterMetaType<KeySequence>();
        qDBusRegisterMetaType<QList<KeySequence>>();
        QVERIFY(m_dir.isValid());

        QFile cfg(m_dir.filePath(QStringLiteral("vdwrc")));
        QVERIFY(cfg.open(QIODevice::WriteOnly));
        cfg.write(
            "[General]\nDefaultWorkspace=two\n\n"
            "[Workspace][two]\nName=Two\nOrder=1\n\n"
            "[Workspace][two][Slot][0]\nApp=org.kde.konsole\nGeometry=0,0,0.5,1\n\n"
            "[Workspace][two][Slot][1]\nApp=org.kde.konsole\nGeometry=0.5,0,0.5,0.5\n\n"
            "[Workspace][two][Slot][2]\nApp=org.kde.konsole\nGeometry=0.5,0.5,0.5,0.5\n\n"
            "[Workspace][one]\nName=One\nOrder=0\nIcon=folder\n\n"
            "[Workspace][one][Slot][0]\nApp=org.kde.konsole\nGeometry=0,0,1,1\n");
        cfg.close();

        auto bus = QDBusConnection::sessionBus();
        QVERIFY(bus.registerObject(QStringLiteral("/T2"), &m_receiver, QDBusConnection::ExportAllSlots));
        QVERIFY(bus.registerService(testService));

        m_daemon = std::make_unique<QProcess>();
        m_daemon->setProcessChannelMode(QProcess::ForwardedChannels);
        m_daemon->start(QStringLiteral(VDWD_PATH), {QStringLiteral("--config"), cfg.fileName(), QStringLiteral("--debug")});
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

    // Launched apps must not hold vdw-ui's stdout, which is the pipe to vdwd:
    // vdw-ui gives them its stderr, which is vdwd's (the test's stderr may be a
    // pipe too, so compare with that rather than checking for "pipe:").
    void testLaunchedAppStdoutIsVdwdStderr()
    {
        // readlink, since QFileInfo would resolve "pipe:[1]" against the directory.
        const auto fdTarget = [](qint64 pid, int fd) {
            char buffer[256];
            const ssize_t length = ::readlink(QStringLiteral("/proc/%1/fd/%2").arg(pid).arg(fd).toLocal8Bit().constData(), buffer, sizeof(buffer) - 1);
            return length > 0 ? QString::fromLocal8Bit(buffer, length) : QString();
        };
        const QString daemonStderr = fdTarget(m_daemon->processId(), 2);
        QVERIFY(!daemonStderr.isEmpty());
        const QList<qint64> pids = pidsOfClass(QStringLiteral("org.kde.konsole"));
        QVERIFY(!pids.isEmpty());
        for (const qint64 pid : pids) {
            const QString target = fdTarget(pid, 1);
            QVERIFY2(target == daemonStderr, qPrintable(QStringLiteral("pid %1: stdout %2, vdwd stderr %3").arg(pid).arg(target, daemonStderr)));
        }
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

    // ---- gesture input through /Debug (T3b) ----

    // Tap of the main shortcut on an empty desktop opens the default workspace.
    void testTapOpensDefault()
    {
        QVERIFY(closeAll());
        debugCall(QStringLiteral("ModifiersHeld"), {true});
        tap(QStringLiteral("main"));
        QTRY_VERIFY_WITH_TIMEOUT(windowsAtSlots(1, 3), 20000);
        debugCall(QStringLiteral("ModifiersHeld"), {false});
    }

    // Busy desktop: the tap opens nothing; a second press with the modifiers
    // held confirms (force) and the windows appear next to the old ones.
    void testBusyThenConfirm()
    {
        QVERIFY(windowsAtSlots(1, 3));
        debugCall(QStringLiteral("ModifiersHeld"), {true});
        tap(QStringLiteral("main"));
        // After the busy answer, inside the confirm timeout (2 s).
        QTest::qWait(800);
        QVERIFY(windowsAtSlots(1, 3));
        tap(QStringLiteral("main"));
        QTRY_VERIFY_WITH_TIMEOUT(windowsAtSlots(2, 6), 20000);
        debugCall(QStringLiteral("ModifiersHeld"), {false});
    }

    // Releasing the modifiers between the two taps cancels the confirm.
    void testModifiersReleasedCancelsConfirm()
    {
        QVERIFY(windowsAtSlots(2, 6));
        debugCall(QStringLiteral("ModifiersHeld"), {true});
        tap(QStringLiteral("main"));
        QTest::qWait(1000);
        debugCall(QStringLiteral("ModifiersHeld"), {false});
        tap(QStringLiteral("main"));
        QTest::qWait(3000);
        QVERIFY(windowsAtSlots(2, 6));
    }

    // A busy tap shows the toast at the top center; releasing the modifiers
    // hides it and the next tap (modifiers up) is cancelled without a toast.
    void testBusyShowsToastAndModifierReleaseHidesIt()
    {
        QVERIFY(windowsAtSlots(2, 6));
        QCOMPARE(windowsOfClass(QStringLiteral("vdw-ui")), 0);
        debugCall(QStringLiteral("ModifiersHeld"), {true});
        tap(QStringLiteral("main"));
        QTRY_VERIFY_WITH_TIMEOUT(toastAtTopCenter(), 1800);
        debugCall(QStringLiteral("ModifiersHeld"), {false});
        QTRY_COMPARE_WITH_TIMEOUT(windowsOfClass(QStringLiteral("vdw-ui")), 0, 10000);
        tap(QStringLiteral("main"));
        QTest::qWait(1000);
        QCOMPARE(windowsOfClass(QStringLiteral("vdw-ui")), 0);
        QVERIFY(windowsAtSlots(2, 6));
    }

    // The toast also goes away by itself with the confirm timeout.
    void testToastHidesOnTimeout()
    {
        debugCall(QStringLiteral("ModifiersHeld"), {true});
        tap(QStringLiteral("main"));
        QTRY_VERIFY_WITH_TIMEOUT(toastAtTopCenter(), 1800);
        QTRY_COMPARE_WITH_TIMEOUT(windowsOfClass(QStringLiteral("vdw-ui")), 0, 10000);
        debugCall(QStringLiteral("ModifiersHeld"), {false});
        QVERIFY(windowsAtSlots(2, 6));
    }

    // Holding the main shortcut shows the menu overlay; releasing it over the
    // dead zone cancels and opens nothing.
    void testHoldShowsMenuAndCancels()
    {
        QCOMPARE(windowsOfClass(QStringLiteral("vdw-ui")), 0);
        debugCall(QStringLiteral("Press"), {QStringLiteral("main")});
        QTRY_COMPARE_WITH_TIMEOUT(windowsOfClass(QStringLiteral("vdw-ui")), 1, 10000);
        debugCall(QStringLiteral("Release"), {QStringLiteral("main")});
        QTRY_COMPARE_WITH_TIMEOUT(windowsOfClass(QStringLiteral("vdw-ui")), 0, 10000);
        QTest::qWait(2000);
        QVERIFY(windowsAtSlots(2, 6));
    }

    // The helper is short-lived: no vdw-ui process of this build once a
    // gesture (menu) and a toast are over.
    void testNoHelperLeftAfterGesture()
    {
        const auto helperRunning = [] {
            // pgrep -f on the full path: never matches an installed vdw-ui of the live session.
            QProcess pgrep;
            pgrep.setStandardOutputFile(QProcess::nullDevice());
            pgrep.start(QStringLiteral("pgrep"), {QStringLiteral("-f"), QStringLiteral(VDWUI_PATH)});
            return pgrep.waitForFinished() && pgrep.exitCode() == 0;
        };
        QVERIFY(!helperRunning());
        debugCall(QStringLiteral("Press"), {QStringLiteral("main")});
        QTRY_COMPARE_WITH_TIMEOUT(windowsOfClass(QStringLiteral("vdw-ui")), 1, 10000);
        QVERIFY(helperRunning());
        debugCall(QStringLiteral("Release"), {QStringLiteral("main")});
        QTRY_VERIFY_WITH_TIMEOUT(!helperRunning(), 10000);
    }

    // A workspace shortcut opens its own workspace and never shows the menu,
    // even when held longer than the hold threshold.
    void testWorkspaceShortcut()
    {
        QVERIFY(closeAll());
        debugCall(QStringLiteral("Press"), {QStringLiteral("one")});
        QTest::qWait(700);
        QCOMPARE(windowsOfClass(QStringLiteral("vdw-ui")), 0);
        debugCall(QStringLiteral("Release"), {QStringLiteral("one")});
        QTRY_VERIFY_WITH_TIMEOUT(oneWindowFillsArea(), 20000);
        QTest::qWait(1500);
        QVERIFY(oneWindowFillsArea());
    }

    // ---- real key events (needs spikes/S1 and S2 built) ----

    // Ctrl+Meta+D tap through org_kde_kwin_fake_input, kglobalaccel, vdwd.
    void testRealKeyTap()
    {
        const QString inject = QStringLiteral(S1INJECT_PATH);
        if (!QFileInfo::exists(inject)) {
            QSKIP("spikes/S1/build/s1inject is not built");
        }
        if (!authorizeFakeInput(inject, QStringLiteral("vdw-s1-inject"))) {
            QSKIP("cannot authorize the injector in the nested XDG_DATA_HOME");
        }
        QVERIFY(closeAll());
        const QStringList steps = {QStringLiteral("300"),
                                   QStringLiteral("+ctrl"),
                                   QStringLiteral("30"),
                                   QStringLiteral("+meta"),
                                   QStringLiteral("30"),
                                   QStringLiteral("+d"),
                                   QStringLiteral("80"),
                                   QStringLiteral("-d"),
                                   QStringLiteral("30"),
                                   QStringLiteral("-meta"),
                                   QStringLiteral("30"),
                                   QStringLiteral("-ctrl"),
                                   QStringLiteral("100")};
        auto process = startHelper(inject, steps);
        QVERIFY(process);
        QVERIFY(process->waitForFinished(15000));
        QCOMPARE(process->exitCode(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(windowsAtSlots(1, 3), 20000);
    }

    // Best effort: hold Ctrl+Meta+D, the overlay appears, move the pointer
    // onto the first slice (straight up from the center), release: opens "one".
    void testRealKeyMenu()
    {
        const QString inject = QStringLiteral(S1INJECT_PATH);
        const QString pointer = QStringLiteral(S2FAKEINPUT_PATH);
        if (!QFileInfo::exists(inject) || !QFileInfo::exists(pointer)) {
            QSKIP("spikes/S1 or spikes/S2 helpers are not built");
        }
        if (!authorizeFakeInput(inject, QStringLiteral("vdw-s1-inject")) || !authorizeFakeInput(pointer, QStringLiteral("vdw-s2-fakeinput"))) {
            QSKIP("cannot authorize the helpers in the nested XDG_DATA_HOME");
        }
        QVERIFY(closeAll());
        const QRect area = snapshot().area;
        const int cx = area.center().x();
        const int cy = area.center().y();
        const auto moveTo = [&](int x, int y) {
            auto p = startHelper(pointer, {QStringLiteral("abs"), QString::number(x), QString::number(y)});
            return p && p->waitForFinished(10000) && p->exitCode() == 0;
        };
        QVERIFY(moveTo(cx, cy));

        const QStringList steps = {QStringLiteral("300"),
                                   QStringLiteral("+ctrl"),
                                   QStringLiteral("30"),
                                   QStringLiteral("+meta"),
                                   QStringLiteral("30"),
                                   QStringLiteral("+d"),
                                   QStringLiteral("9000"),
                                   QStringLiteral("-d"),
                                   QStringLiteral("30"),
                                   QStringLiteral("-meta"),
                                   QStringLiteral("30"),
                                   QStringLiteral("-ctrl"),
                                   QStringLiteral("100")};
        auto keys = startHelper(inject, steps);
        QVERIFY(keys);
        QTRY_COMPARE_WITH_TIMEOUT(windowsOfClass(QStringLiteral("vdw-ui")), 1, 10000);
        QVERIFY(moveTo(cx, cy - 120));
        QVERIFY(moveTo(cx, cy - 200));
        QVERIFY(keys->waitForFinished(20000));
        QCOMPARE(keys->exitCode(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(oneWindowFillsArea(), 20000);
        QTRY_COMPARE_WITH_TIMEOUT(windowsOfClass(QStringLiteral("vdw-ui")), 0, 10000);
    }

    // Best effort: a menu pick on a busy desktop shows the toast, and letting
    // go of Ctrl+Meta cancels it (the pick itself is released with D first).
    void testRealKeyMenuBusyCancelledByModifierRelease()
    {
        const QString inject = QStringLiteral(S1INJECT_PATH);
        const QString pointer = QStringLiteral(S2FAKEINPUT_PATH);
        if (!QFileInfo::exists(inject) || !QFileInfo::exists(pointer)) {
            QSKIP("spikes/S1 or spikes/S2 helpers are not built");
        }
        if (!authorizeFakeInput(inject, QStringLiteral("vdw-s1-inject")) || !authorizeFakeInput(pointer, QStringLiteral("vdw-s2-fakeinput"))) {
            QSKIP("cannot authorize the helpers in the nested XDG_DATA_HOME");
        }
        QVERIFY(oneWindowFillsArea());
        const QRect area = snapshot().area;
        const int cx = area.center().x();
        const int cy = area.center().y();
        const auto moveTo = [&](int x, int y) {
            auto p = startHelper(pointer, {QStringLiteral("abs"), QString::number(x), QString::number(y)});
            return p && p->waitForFinished(10000) && p->exitCode() == 0;
        };
        QVERIFY(moveTo(cx, cy));

        const QStringList steps = {QStringLiteral("300"),
                                   QStringLiteral("+ctrl"),
                                   QStringLiteral("30"),
                                   QStringLiteral("+meta"),
                                   QStringLiteral("30"),
                                   QStringLiteral("+d"),
                                   QStringLiteral("9000"),
                                   QStringLiteral("-d"),
                                   QStringLiteral("1500"),
                                   QStringLiteral("-meta"),
                                   QStringLiteral("30"),
                                   QStringLiteral("-ctrl"),
                                   QStringLiteral("100")};
        auto keys = startHelper(inject, steps);
        QVERIFY(keys);
        QTRY_COMPARE_WITH_TIMEOUT(windowsOfClass(QStringLiteral("vdw-ui")), 1, 10000);
        QVERIFY(moveTo(cx, cy - 120));
        QVERIFY(moveTo(cx, cy - 200));
        QTRY_VERIFY_WITH_TIMEOUT(toastAtTopCenter(), 20000);
        QVERIFY(keys->waitForFinished(20000));
        QCOMPARE(keys->exitCode(), 0);
        QTRY_COMPARE_WITH_TIMEOUT(windowsOfClass(QStringLiteral("vdw-ui")), 0, 10000);
        QVERIFY(oneWindowFillsArea());
    }

    // ---- capture (T6) ----

    void testCaptureErrors()
    {
        QVERIFY(closeAll());
        Result r = ctl({QStringLiteral("capture"), QStringLiteral("")});
        QCOMPARE(r.out, QStringLiteral("error: a name is required"));
        QCOMPARE(r.exitCode, 1);
        r = ctl({QStringLiteral("capture"), QStringLiteral("   ")});
        QCOMPARE(r.out, QStringLiteral("error: a name is required"));
        r = ctl({QStringLiteral("capture"), QStringLiteral("Nothing")});
        QCOMPARE(r.out, QStringLiteral("error: no windows to capture"));
        QCOMPARE(r.exitCode, 1);
    }

    // Open "two" (3 Konsoles), capture it, close everything, open the
    // captured workspace: the windows land where the originals were.
    void testCaptureRoundTrip()
    {
        QVERIFY(closeAll());
        QCOMPARE(ctl({QStringLiteral("open"), QStringLiteral("two")}).out, QStringLiteral("opened"));
        QTRY_VERIFY_WITH_TIMEOUT(windowsAtSlots(1, 3), 20000);
        QTest::qWait(1000);
        const Snapshot before = snapshot();

        const Result captured = ctl({QStringLiteral("capture"), QStringLiteral("Captured")});
        QCOMPARE(captured.exitCode, 0);
        QCOMPARE(captured.out, QStringLiteral("captured"));

        // The new workspace is in the list, after the others, with the icon of
        // the first slot's app and three slots.
        const QJsonArray list = QJsonDocument::fromJson(ctl({QStringLiteral("list")}).out.toUtf8()).array();
        QCOMPARE(list.size(), 3);
        const QJsonObject added = list.last().toObject();
        QCOMPARE(added[QStringLiteral("id")].toString(), QStringLiteral("captured"));
        QCOMPARE(added[QStringLiteral("name")].toString(), QStringLiteral("Captured"));
        QVERIFY(!added[QStringLiteral("icon")].toString().isEmpty());
        QCOMPARE(added[QStringLiteral("slots")].toArray().size(), 3);

        // A second capture with the same name gets a unique id.
        QCOMPARE(ctl({QStringLiteral("capture"), QStringLiteral("Captured")}).out, QStringLiteral("captured-2"));

        QVERIFY(closeAll());
        QCOMPARE(ctl({QStringLiteral("open"), QStringLiteral("captured")}).out, QStringLiteral("opened"));
        QTRY_VERIFY_WITH_TIMEOUT(snapshot().windows.size() == 3, 20000);
        QTest::qWait(1500);
        QList<Window> free = snapshot().windows;
        QCOMPARE(free.size(), 3);
        for (const Window &w : before.windows) {
            bool found = false;
            for (int i = 0; i < free.size(); ++i) {
                if (near(free[i].rect, w.rect)) {
                    free.removeAt(i);
                    found = true;
                    break;
                }
            }
            QVERIFY2(found, qPrintable(QStringLiteral("no window near %1,%2 %3x%4").arg(w.rect.x()).arg(w.rect.y()).arg(w.rect.width()).arg(w.rect.height())));
        }
    }

    // A key the user picked in System Settings survives a vdwd restart: vdwd
    // registers its default with IsDefault|NoAutoloading and must not
    // overwrite the active key.
    void testUserShortcutSurvivesRestart()
    {
        const int original = (Qt::CTRL | Qt::META | Qt::Key_D).toCombined();
        const int user = (Qt::CTRL | Qt::META | Qt::Key_F).toCombined();
        QCOMPARE(mainChord(), original);
        QCOMPARE(accelCall(QStringLiteral("setForeignShortcutKeys"), {QVariant::fromValue(keysArgument(user))}).type(), QDBusMessage::ReplyMessage);
        QCOMPARE(mainChord(), user);
        // Restore the default on every exit, also a failed QVERIFY, so later
        // tests can press Ctrl+Meta+D.
        const auto restore = qScopeGuard([this, original] {
            accelCall(QStringLiteral("setForeignShortcutKeys"), {QVariant::fromValue(keysArgument(original))});
        });

        m_daemon->terminate();
        QVERIFY(m_daemon->waitForFinished(5000));
        auto *iface = QDBusConnection::sessionBus().interface();
        QTRY_VERIFY_WITH_TIMEOUT(!iface->isServiceRegistered(QStringLiteral("io.github.librv.vdw")), 5000);
        m_daemon = std::make_unique<QProcess>();
        m_daemon->setProcessChannelMode(QProcess::ForwardedChannels);
        m_daemon->start(QStringLiteral(VDWD_PATH), {QStringLiteral("--config"), m_dir.filePath(QStringLiteral("vdwrc")), QStringLiteral("--debug")});
        QVERIFY(m_daemon->waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(iface->isServiceRegistered(QStringLiteral("io.github.librv.vdw")), 15000);
        // Registration is asynchronous: give it time to (wrongly) overwrite.
        QTest::qWait(2000);
        QCOMPARE(mainChord(), user);
    }

    // Runs last: replaces the daemon with one that has no vdwrc (in its own
    // XDG_CONFIG_HOME) and checks the built-in starting workspace.
    void testDefaultConfigOnFirstStart()
    {
        m_daemon->terminate();
        QVERIFY(m_daemon->waitForFinished(5000));
        auto *iface = QDBusConnection::sessionBus().interface();
        QTRY_VERIFY_WITH_TIMEOUT(!iface->isServiceRegistered(QStringLiteral("io.github.librv.vdw")), 5000);

        QTemporaryDir configHome;
        QVERIFY(configHome.isValid());
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("XDG_CONFIG_HOME"), configHome.path());
        m_daemon = std::make_unique<QProcess>();
        m_daemon->setProcessChannelMode(QProcess::ForwardedChannels);
        m_daemon->setProcessEnvironment(env);
        m_daemon->start(QStringLiteral(VDWD_PATH), {});
        QVERIFY(m_daemon->waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(iface->isServiceRegistered(QStringLiteral("io.github.librv.vdw")), 15000);

        QVERIFY(QFileInfo::exists(configHome.filePath(QStringLiteral("vdwrc"))));
        const Result r = ctl({QStringLiteral("list")});
        QCOMPARE(r.exitCode, 0);
        const QJsonArray array = QJsonDocument::fromJson(r.out.toUtf8()).array();
        QCOMPARE(array.size(), 1);
        const QJsonObject ws = array[0].toObject();
        QCOMPARE(ws[QStringLiteral("id")].toString(), QStringLiteral("3-konsoles"));
        QCOMPARE(ws[QStringLiteral("name")].toString(), QStringLiteral("3 Konsoles"));
        QVERIFY(ws[QStringLiteral("isDefault")].toBool());
        QCOMPARE(ws[QStringLiteral("slots")].toArray().size(), 3);
        // The legacy layout: top-right, bottom-left, bottom-right quadrants.
        const QJsonArray slots = ws[QStringLiteral("slots")].toArray();
        const QList<QList<double>> expected = {{0.5, 0, 0.5, 0.5}, {0, 0.5, 0.5, 0.5}, {0.5, 0.5, 0.5, 0.5}};
        for (int i = 0; i < 3; ++i) {
            const QJsonObject slot = slots[i].toObject();
            QCOMPARE(slot[QStringLiteral("app")].toString(), QStringLiteral("org.kde.konsole"));
            const QJsonObject g = slot[QStringLiteral("geometry")].toObject();
            const QList<double> actual = {g[QStringLiteral("x")].toDouble(),
                                          g[QStringLiteral("y")].toDouble(),
                                          g[QStringLiteral("width")].toDouble(),
                                          g[QStringLiteral("height")].toDouble()};
            QCOMPARE(actual, expected[i]);
        }
    }
};

QTEST_GUILESS_MAIN(IntegrationTest)

#include "integrationtest.moc"
