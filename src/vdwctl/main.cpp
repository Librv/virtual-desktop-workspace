// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "dbusnames.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QTextStream>

namespace
{
int usage()
{
    QTextStream(stderr) << "Usage: vdwctl list\n"
                           "       vdwctl open <id> [--force]\n"
                           "       vdwctl capture <name>\n"
                           "       vdwctl reload\n";
    return 2;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments().mid(1);
    if (args.isEmpty()) {
        return usage();
    }

    const QString &command = args.at(0);
    QString method;
    QVariantList callArgs;
    if (command == QLatin1String("list") && args.size() == 1) {
        method = QStringLiteral("ListWorkspaces");
    } else if (command == QLatin1String("open") && (args.size() == 2 || (args.size() == 3 && args.at(2) == QLatin1String("--force")))) {
        method = QStringLiteral("OpenWorkspace");
        callArgs = {args.at(1), args.size() == 3};
    } else if (command == QLatin1String("capture") && args.size() == 2) {
        method = QStringLiteral("CaptureCurrentDesktop");
        callArgs = {args.at(1)};
    } else if (command == QLatin1String("reload") && args.size() == 1) {
        method = QStringLiteral("ReloadConfig");
    } else {
        return usage();
    }

    QDBusMessage message = QDBusMessage::createMethodCall(Vdw::DBusNames::service, Vdw::DBusNames::managerPath, Vdw::DBusNames::managerInterface, method);
    message.setArguments(callArgs);
    const QDBusMessage reply = QDBusConnection::sessionBus().call(message, QDBus::Block, 10000);

    QTextStream err(stderr);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        err << "vdwctl: D-Bus error: " << reply.errorName() << ": " << reply.errorMessage() << "\n";
        return 1;
    }
    QString text;
    if (reply.arguments().isEmpty()) {
        text = QStringLiteral("ok");
    } else {
        text = reply.arguments().at(0).toString();
    }
    QTextStream(stdout) << text << "\n";
    return text.startsWith(QLatin1String("error:")) ? 1 : 0;
}
