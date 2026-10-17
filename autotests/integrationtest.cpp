// Starts vdwd inside the nested KWin session (scripts/dev-session.sh) and
// checks vdwctl against it. Skips when not run inside that session, so it
// never touches the live session bus:
//   scripts/dev-session.sh run -- ctest --preset dev -R integrationtest

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

#include <memory>

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

    QTemporaryDir m_dir;
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
            "[Workspace][one]\nName=One\nOrder=0\nIcon=folder\n");
        cfg.close();

        m_daemon = std::make_unique<QProcess>();
        m_daemon->setProcessChannelMode(QProcess::ForwardedChannels);
        m_daemon->start(QStringLiteral(VDWD_PATH), {QStringLiteral("--config"), cfg.fileName()});
        QVERIFY(m_daemon->waitForStarted());

        auto *iface = QDBusConnection::sessionBus().interface();
        QTRY_VERIFY_WITH_TIMEOUT(iface->isServiceRegistered(QStringLiteral("io.github.librv.vdw")), 15000);
    }

    void cleanupTestCase()
    {
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

    void testOpenKnown()
    {
        const Result r = ctl({QStringLiteral("open"), QStringLiteral("two")});
        QCOMPARE(r.exitCode, 1);
        QCOMPARE(r.out, QStringLiteral("error: not implemented"));
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
