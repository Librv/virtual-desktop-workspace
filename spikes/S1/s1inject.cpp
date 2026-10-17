// S1 spike injector: press and release keys through KWin's
// org_kde_kwin_fake_input protocol. Throwaway code.
//
// ONLY run this against the nested dev session
// (scripts/dev-session.sh run -- s1inject ...). Against the live session it
// would type into the real desktop (and KWin refuses it there anyway unless
// the binary is authorized through a .desktop file).
//
// Usage: s1inject STEP...
//   +KEY    press KEY      (ctrl, meta, shift, alt, d, esc)
//   -KEY    release KEY
//   NUMBER  wait NUMBER milliseconds
// Example (tap): s1inject 300 +ctrl +meta +d 80 -d -meta -ctrl 300

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/fakeinput.h>
#include <KWayland/Client/registry.h>

#include <QElapsedTimer>
#include <QGuiApplication>
#include <QHash>
#include <QTimer>

#include <cstdio>
#include <functional>
#include <memory>
#include <linux/input-event-codes.h>

using namespace KWayland::Client;

static QElapsedTimer s_clock;

static void logLine(const QString &text)
{
    std::printf("inject %7lld %s\n", static_cast<long long>(s_clock.elapsed()), qPrintable(text));
    std::fflush(stdout);
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    s_clock.start();

    const QHash<QString, quint32> codes{
        {QStringLiteral("ctrl"), KEY_LEFTCTRL},
        {QStringLiteral("meta"), KEY_LEFTMETA},
        {QStringLiteral("shift"), KEY_LEFTSHIFT},
        {QStringLiteral("alt"), KEY_LEFTALT},
        {QStringLiteral("d"), KEY_D},
        {QStringLiteral("esc"), KEY_ESC},
    };
    const QStringList steps = app.arguments().mid(1);

    auto *connection = ConnectionThread::fromApplication(&app);
    if (!connection) {
        logLine(QStringLiteral("error: not a Wayland session"));
        return 2;
    }
    auto *registry = new Registry(&app);
    registry->create(connection);
    FakeInput *fake = nullptr;

    // Runs the steps one after another; waits are timers so the event loop
    // keeps dispatching.
    auto runStep = std::make_shared<std::function<void(int)>>();
    *runStep = [&, runStep](int i) {
        if (i >= steps.size()) {
            logLine(QStringLiteral("done"));
            // Give KWin a moment to process before the device goes away.
            QTimer::singleShot(200, &app, &QCoreApplication::quit);
            return;
        }
        const QString &s = steps[i];
        bool isNumber = false;
        const int ms = s.toInt(&isNumber);
        if (isNumber) {
            QTimer::singleShot(ms, &app, [runStep, i] {
                (*runStep)(i + 1);
            });
            return;
        }
        const QChar op = s.at(0);
        const QString name = s.mid(1).toLower();
        if ((op != QLatin1Char('+') && op != QLatin1Char('-')) || !codes.contains(name)) {
            logLine(QStringLiteral("error: bad step %1").arg(s));
            app.exit(2);
            return;
        }
        if (op == QLatin1Char('+')) {
            fake->requestKeyboardKeyPress(codes.value(name));
        } else {
            fake->requestKeyboardKeyRelease(codes.value(name));
        }
        connection->flush();
        logLine(s);
        (*runStep)(i + 1);
    };

    QObject::connect(registry, &Registry::fakeInputAnnounced, &app, [&](quint32 name, quint32 version) {
        logLine(QStringLiteral("fake input announced name=%1 version=%2").arg(name).arg(version));
        fake = registry->createFakeInput(name, version, &app);
        fake->authenticate(QStringLiteral("vdw-s1-inject"), QStringLiteral("S1 spike key injection in nested session"));
        connection->flush();
        // Let KWin create the device before sending keys.
        QTimer::singleShot(300, &app, [runStep] {
            (*runStep)(0);
        });
    });
    QObject::connect(registry, &Registry::interfacesAnnounced, &app, [&] {
        if (!fake) {
            logLine(QStringLiteral("error: org_kde_kwin_fake_input not announced to this client (not authorized?)"));
            app.exit(3);
        }
    });
    registry->setup();
    connection->flush();
    return app.exec();
}
