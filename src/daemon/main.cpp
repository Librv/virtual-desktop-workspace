// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "dbusnames.h"
#include "gesturecontroller.h"
#include "kwinscripts.h"
#include "launcher.h"
#include "manager.h"
#include "radialmenu.h"
#include "shortcuts.h"

#include "configwriter.h"

#include <KConfig>
#include <KDBusService>

#include <QCommandLineParser>
#include <QDBusConnection>
#include <QFile>
#include <QGuiApplication>
#include <QStandardPaths>

namespace
{
/// First start with the default config path: write the built-in "3 Konsoles"
/// workspace (the legacy layout) unless `vdwrc` already exists.
void writeDefaultConfigIfMissing()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/vdwrc");
    if (QFile::exists(path)) {
        return;
    }
    Vdw::Workspace workspace;
    workspace.id = QStringLiteral("3-konsoles");
    workspace.name = QStringLiteral("3 Konsoles");
    workspace.icon = QStringLiteral("utilities-terminal");
    for (const QRectF &geometry : {QRectF(0.5, 0, 0.5, 0.5), QRectF(0, 0.5, 0.5, 0.5), QRectF(0.5, 0.5, 0.5, 0.5)}) {
        workspace.slots.append({QStringLiteral("org.kde.konsole"), QString(), geometry});
    }
    KConfig config(path, KConfig::SimpleConfig);
    Vdw::ConfigWriter::write(config, {workspace}, workspace.id);
    config.sync();
}
}

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

    if (!parser.isSet(configOption)) {
        writeDefaultConfigIfMissing();
    }

    Vdw::KWinScripts scripts;
    Vdw::Launcher launcher;
    Vdw::Manager manager(parser.value(configOption), &scripts, &launcher);
    Vdw::ScriptApi script(&manager, &scripts);
    Vdw::Shortcuts shortcuts;
    RadialMenu menu;
    Vdw::GestureController controller(&manager, &shortcuts, &scripts, &menu);
    Vdw::DebugApi debug(&controller);

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

    // Registers the global shortcuts; only after the name is ours, so a second
    // instance never touches them.
    shortcuts.sync(manager.config());
    QObject::connect(&manager, &Vdw::Manager::configReloaded, &shortcuts, [&shortcuts, &manager] {
        shortcuts.sync(manager.config());
    });

    // After the name is ours: the placer calls back as soon as it is loaded.
    scripts.start();

    return app.exec();
}
