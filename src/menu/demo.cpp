// SPDX-License-Identifier: GPL-2.0-or-later
// Demo for nested-session checks (not installed). Shows the radial menu and
// reads commands from stdin, one per line:
//   show   map the menu again
//   select print "MENU selected <id>" (or "MENU selected -") for the last pointer position
//   hide   unmap the menu
//   quit
// Output lines start with "MENU " so tests can grep them.
#include "radialmenu.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QScreen>
#include <QSocketNotifier>

#include <cstdio>
#include <unistd.h>

static void out(const QString &line)
{
    std::printf("MENU %s\n", qPrintable(line));
    std::fflush(stdout);
}

// "id:Name:icon,..."
static QVector<RadialMenu::Item> parseItems(const QString &spec)
{
    QVector<RadialMenu::Item> items;
    for (const QString &entry : spec.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QStringList parts = entry.split(QLatin1Char(':'));
        items.append({parts.value(0), parts.value(1, parts.value(0)), parts.value(2, QStringLiteral("virtual-desktops"))});
    }
    return items;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("vdw-menu-demo"));
    app.setDesktopFileName(QStringLiteral("vdw-menu-demo"));
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption centerOpt(QStringLiteral("center"), QStringLiteral("Menu center as x,y (default: screen center)."), QStringLiteral("x,y"));
    const QCommandLineOption itemsOpt(QStringLiteral("items"),
                                      QStringLiteral("Comma-separated id:Name:icon list; empty for the no-workspaces hint."),
                                      QStringLiteral("list"),
                                      QStringLiteral("dev:Development:applications-development,web:Web:internet-web-browser,"
                                                     "files:Files:system-file-manager,mail:Mail:internet-mail,term:Terminals:utilities-terminal"));
    parser.addOptions({centerOpt, itemsOpt});
    parser.process(app);

    QPoint center;
    if (parser.isSet(centerOpt)) {
        const QStringList xy = parser.value(centerOpt).split(QLatin1Char(','));
        center = QPoint(xy.value(0).toInt(), xy.value(1).toInt());
    } else if (QScreen *screen = QGuiApplication::primaryScreen()) {
        center = screen->geometry().center();
    }
    const QVector<RadialMenu::Item> items = parseItems(parser.value(itemsOpt));

    RadialMenu menu;
    menu.show(center, items);
    out(QStringLiteral("shown center=%1,%2 items=%3").arg(center.x()).arg(center.y()).arg(items.size()));

    QSocketNotifier notifier(STDIN_FILENO, QSocketNotifier::Read);
    QObject::connect(&notifier, &QSocketNotifier::activated, &app, [&] {
        char buf[256];
        const ssize_t n = read(STDIN_FILENO, buf, sizeof buf - 1);
        if (n <= 0) {
            notifier.setEnabled(false);
            return;
        }
        buf[n] = 0;
        for (const QString &raw : QString::fromLocal8Bit(buf).split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QString cmd = raw.trimmed();
            if (cmd == QLatin1String("show")) {
                menu.show(center, items);
                out(QStringLiteral("shown"));
            } else if (cmd == QLatin1String("select")) {
                const QString id = menu.selectedId();
                out(QStringLiteral("selected %1").arg(id.isEmpty() ? QStringLiteral("-") : id));
            } else if (cmd == QLatin1String("hide")) {
                menu.hide();
                out(QStringLiteral("hidden"));
            } else if (cmd == QLatin1String("quit")) {
                QCoreApplication::quit();
            } else {
                out(QStringLiteral("unknown command '%1'").arg(cmd));
            }
        }
    });

    return app.exec();
}
