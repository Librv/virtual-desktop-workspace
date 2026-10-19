// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

// Test app for integrationtest: shows a startup window and a main window, like
// Discord (updater) and Steam (loading window) do.
//
//   startupwindowhelper <app id> <settings file>
//
// The settings file holds one line, "<mode> <ms>", and the helper reads it at
// start, so the test can change the behaviour without a new .desktop file:
//   overlap <ms>  the main window opens, the startup window closes <ms> later
//   gap <ms>      the startup window closes, the main window opens <ms> later
// The first step runs 300 ms after start. Titles: "vdw-startup", "vdw-main".

#include <QFile>
#include <QGuiApplication>
#include <QRasterWindow>
#include <QTimer>

#include <memory>

namespace
{
constexpr int firstStepMs = 300;

std::unique_ptr<QRasterWindow> makeWindow(const QString &title)
{
    auto window = std::make_unique<QRasterWindow>();
    window->setTitle(title);
    window->resize(400, 300);
    window->show();
    return window;
}
}

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // The test closes the windows: the app must outlive the step in which the
    // startup window is the only one that closes.
    app.setQuitOnLastWindowClosed(false);
    if (app.arguments().size() != 3) {
        return 2;
    }
    QGuiApplication::setDesktopFileName(app.arguments().at(1));
    QFile file(app.arguments().at(2));
    if (!file.open(QIODevice::ReadOnly)) {
        return 2;
    }
    const QList<QByteArray> words = file.readLine().simplified().split(' ');
    if (words.size() != 2) {
        return 2;
    }
    const bool overlap = words.at(0) == "overlap";
    const int ms = words.at(1).toInt();

    std::unique_ptr<QRasterWindow> startup = makeWindow(QStringLiteral("vdw-startup"));
    std::unique_ptr<QRasterWindow> main;
    QTimer::singleShot(firstStepMs, &app, [&] {
        if (overlap) {
            main = makeWindow(QStringLiteral("vdw-main"));
            QTimer::singleShot(ms, &app, [&] {
                startup.reset();
            });
        } else {
            startup.reset();
            QTimer::singleShot(ms, &app, [&] {
                main = makeWindow(QStringLiteral("vdw-main"));
            });
        }
    });
    // The test closes the main window through KWin; quit then.
    QObject::connect(&app, &QGuiApplication::lastWindowClosed, &app, [&] {
        if (main) {
            QGuiApplication::quit();
        }
    });
    return app.exec();
}
