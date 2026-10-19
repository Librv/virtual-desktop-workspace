// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

// Test helper for integrationtest: behaves like Chromium for the part that
// matters to vdwd. The first process stays and owns the windows. Later
// processes hand their profile to it over a local socket and exit, so every
// window has the pid of the first process and the app id "helium".
//
//   fakebrowser --socket <path> [--profile-directory=<dir>] [--first-delay-ms <n>]
//
// The first window of the first process shows after a delay, a window of a
// forwarded profile shows at once. So a second launch gets its window first.

#include <QCoreApplication>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QRasterWindow>
#include <QTimer>

#include <memory>
#include <vector>

namespace
{

QString optionValue(const QStringList &args, const QString &name, const QString &fallback)
{
    const QString prefix = name + QLatin1Char('=');
    for (qsizetype i = 0; i < args.size(); ++i) {
        if (args.at(i).startsWith(prefix)) {
            return args.at(i).mid(prefix.size());
        }
        if (args.at(i) == name && i + 1 < args.size()) {
            return args.at(i + 1);
        }
    }
    return fallback;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication::setDesktopFileName(QStringLiteral("helium"));
    QGuiApplication app(argc, argv);

    const QStringList args = app.arguments();
    const QString profile = optionValue(args, QStringLiteral("--profile-directory"), QStringLiteral("Default"));
    const QString socketPath = optionValue(args, QStringLiteral("--socket"), QString());
    const int firstDelayMs = optionValue(args, QStringLiteral("--first-delay-ms"), QStringLiteral("1500")).toInt();
    if (socketPath.isEmpty()) {
        qCritical("fakebrowser: --socket is required");
        return 2;
    }

    // The lock makes "connect, else listen" atomic between instances that
    // start at once. It is held until this instance listens or has handed over.
    QLockFile lock(socketPath + QStringLiteral(".lock"));
    lock.setStaleLockTime(5000);
    if (!lock.tryLock(10000)) {
        qCritical("fakebrowser: cannot lock");
        return 2;
    }

    QLocalSocket socket;
    socket.connectToServer(socketPath);
    if (socket.waitForConnected(1000)) {
        socket.write(profile.toUtf8() + '\n');
        socket.waitForBytesWritten(1000);
        socket.disconnectFromServer();
        return 0;
    }

    std::vector<std::unique_ptr<QRasterWindow>> windows;
    auto show = [&windows](const QString &title) {
        auto window = std::make_unique<QRasterWindow>();
        window->setTitle(title);
        window->resize(300, 200);
        window->show();
        windows.push_back(std::move(window));
    };

    QLocalServer::removeServer(socketPath);
    QLocalServer server;
    if (!server.listen(socketPath)) {
        qCritical("fakebrowser: cannot listen: %s", qPrintable(server.errorString()));
        return 2;
    }
    lock.unlock();
    QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
        while (QLocalSocket *client = server.nextPendingConnection()) {
            QObject::connect(client, &QLocalSocket::readyRead, client, [client, show] {
                while (client->canReadLine()) {
                    show(QString::fromUtf8(client->readLine()).trimmed());
                }
            });
            QObject::connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
        }
    });
    QTimer::singleShot(firstDelayMs, &app, [show, profile] {
        show(profile);
    });
    return app.exec();
}
