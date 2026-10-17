#include "dbusnames.h"
#include "kwinscripts.h"
#include "launcher.h"
#include "manager.h"

#include <KDBusService>

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QGuiApplication>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // KDBusService derives its name from these: io.github.librv.vdw
    QGuiApplication::setApplicationName(QStringLiteral("vdw"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("librv.github.io"));
    QGuiApplication::setQuitLockEnabled(false); // windowless daemon

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Virtual Desktop Workspace daemon"));
    parser.addHelpOption();
    const QCommandLineOption configOption(QStringLiteral("config"), QStringLiteral("Use FILE instead of ~/.config/vdwrc."), QStringLiteral("FILE"));
    const QCommandLineOption debugOption(QStringLiteral("debug"), QStringLiteral("Register the /Debug D-Bus object."));
    parser.addOption(configOption);
    parser.addOption(debugOption);
    parser.process(app);

    Vdw::KWinScripts scripts;
    Vdw::Launcher launcher;
    Vdw::Manager manager(parser.value(configOption), &scripts, &launcher);
    Vdw::ScriptApi script(&manager, &scripts);
    Vdw::DebugApi debug;

    QDBusConnection bus = QDBusConnection::sessionBus();
    constexpr auto options = QDBusConnection::ExportAllSlots;
    if (!bus.registerObject(Vdw::DBusNames::managerPath, &manager, options) || !bus.registerObject(Vdw::DBusNames::scriptPath, &script, options)) {
        qCritical("vdwd: cannot register D-Bus objects");
        return 1;
    }
    if (parser.isSet(debugOption) && !bus.registerObject(Vdw::DBusNames::debugPath, &debug, options)) {
        qCritical("vdwd: cannot register the /Debug object");
        return 1;
    }

    // Claim the service name only after the objects exist, so a client that
    // reacts to the name appearing never sees UnknownObject. A second
    // instance exits here.
    KDBusService service(KDBusService::Unique);

    // After the name is ours: the placer calls back as soon as it is loaded.
    scripts.start();

    return app.exec();
}
