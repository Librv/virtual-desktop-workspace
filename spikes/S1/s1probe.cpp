// S1 spike probe: log what KGlobalAccel and KModifierKeyInfo report.
// Throwaway code, not part of the product.
//
// Usage: s1probe [--seconds N] [--shortcut "Ctrl+Meta+D"] [--name COMPONENT]
//                [--keep] [--autoload]
//
// Registers one global shortcut (component COMPONENT, default "vdw-s1-probe", action
// "vdw-s1-main") and prints one line per event:
//   <ms since start> active=<0|1>      globalShortcutActiveChanged
//   <ms since start> triggered         QAction::triggered (autoRepeat on)
//   <ms since start> mod <Key> <0|1>   KModifierKeyInfo::keyPressed
// Exits after N seconds (default 120) or on SIGINT/SIGTERM. On exit it
// removes its shortcut again (KGlobalAccel::removeAllShortcuts) so nothing
// is left behind in kglobalshortcutsrc unless --keep is given.

#include <KGlobalAccel>
#include <KModifierKeyInfo>

#include <QAction>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QKeySequence>
#include <QTimer>

#include <csignal>
#include <cstdio>

static QElapsedTimer s_clock;

static void logLine(const QString &text)
{
    std::printf("%7lld %s\n", static_cast<long long>(s_clock.elapsed()), qPrintable(text));
    std::fflush(stdout);
}

static QString keyName(Qt::Key key)
{
    switch (key) {
    case Qt::Key_Control:
        return QStringLiteral("Control");
    case Qt::Key_Meta:
        return QStringLiteral("Meta");
    case Qt::Key_Shift:
        return QStringLiteral("Shift");
    case Qt::Key_Alt:
        return QStringLiteral("Alt");
    case Qt::Key_AltGr:
        return QStringLiteral("AltGr");
    default:
        return QKeySequence(key).toString();
    }
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    s_clock.start();

    int seconds = 120;
    bool keep = false;
    bool autoload = false;
    QString name = QStringLiteral("vdw-s1-probe");
    QString shortcutText = QStringLiteral("Ctrl+Meta+D");
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == QLatin1String("--seconds") && i + 1 < args.size()) {
            seconds = args[++i].toInt();
        } else if (args[i] == QLatin1String("--shortcut") && i + 1 < args.size()) {
            shortcutText = args[++i];
        } else if (args[i] == QLatin1String("--name") && i + 1 < args.size()) {
            name = args[++i];
        } else if (args[i] == QLatin1String("--keep")) {
            keep = true;
        } else if (args[i] == QLatin1String("--autoload")) {
            autoload = true;
        }
    }

    app.setApplicationName(name);
    app.setApplicationDisplayName(name);
    logLine(QStringLiteral("platform=%1 isComponentActive check follows").arg(QGuiApplication::platformName()));

    auto *action = new QAction(&app);
    action->setObjectName(QStringLiteral("vdw-s1-main"));
    action->setText(QStringLiteral("S1 probe: main gesture"));
    action->setProperty("componentName", name);
    action->setProperty("componentDisplayName", name);

    const QList<QKeySequence> keys{QKeySequence::fromString(shortcutText)};
    KGlobalAccel::self()->setDefaultShortcut(action, keys, KGlobalAccel::NoAutoloading);
    // Without --autoload the key is forced to the given one (handy for tests).
    // With --autoload a key the user changed in System Settings is kept, which
    // is what the real daemon must do.
    const bool ok = KGlobalAccel::self()->setShortcut(action, keys, autoload ? KGlobalAccel::Autoloading : KGlobalAccel::NoAutoloading);
    const QList<QKeySequence> got = KGlobalAccel::self()->shortcut(action);
    QStringList gotText;
    for (const auto &k : got) {
        gotText << k.toString();
    }
    logLine(QStringLiteral("registered ok=%1 shortcut=%2 isComponentActive=%3")
                .arg(ok)
                .arg(gotText.join(QLatin1Char(',')))
                .arg(KGlobalAccel::isComponentActive(name)));

    QObject::connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutActiveChanged, &app, [action](QAction *a, bool active) {
        logLine(QStringLiteral("active=%1%2").arg(active).arg(a == action ? QString() : QStringLiteral(" (other action)")));
    });
    QObject::connect(action, &QAction::triggered, &app, [] {
        logLine(QStringLiteral("triggered"));
    });
    QObject::connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, &app, [](QAction *, const QKeySequence &seq) {
        logLine(QStringLiteral("globalShortcutChanged %1").arg(seq.toString()));
    });

    auto *mods = new KModifierKeyInfo(&app);
    QStringList known;
    for (Qt::Key k : mods->knownKeys()) {
        known << keyName(k);
    }
    logLine(QStringLiteral("KModifierKeyInfo knownKeys=%1").arg(known.join(QLatin1Char(','))));
    QObject::connect(mods, &KModifierKeyInfo::keyPressed, &app, [](Qt::Key key, bool pressed) {
        logLine(QStringLiteral("mod %1 %2").arg(keyName(key)).arg(pressed));
    });

    std::signal(SIGINT, [](int) {
        QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
    });
    std::signal(SIGTERM, [](int) {
        QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
    });
    QTimer::singleShot(seconds * 1000, &app, &QCoreApplication::quit);
    logLine(QStringLiteral("ready"));

    const int rc = app.exec();
    if (!keep) {
        KGlobalAccel::self()->removeAllShortcuts(action);
    }
    logLine(QStringLiteral("exit"));
    return rc;
}
