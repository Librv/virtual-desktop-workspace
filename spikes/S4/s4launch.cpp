// Throwaway S4 spike: launch apps from .desktop ids (plus extra args) with
// KIO::ApplicationLauncherJob and print what we learn about each launch.
//
// Usage: s4launch [--token] [--token-window] [--gap MS] [--wait MS] ID[=ARGS]...
//   --token         request an xdg_activation_v1 token per launch (no window)
//   --token-window  same, but from a small helper window (needed for a token?)
//   --gap MS        delay between starting launches (default 0: all at once)
//   --wait MS       keep running this long after the last job finished
//
// Output lines: "s4 <ms-since-epoch> <event> key=value ...".

#include <KIO/ApplicationLauncherJob>
#include <KService>
#include <KWaylandExtras>
#include <KWindowSystem>

#include <QDateTime>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QTimer>
#include <QWindow>

#include <cstdio>

static void out(const QString &event, const QString &rest)
{
    std::printf("s4 %lld %s %s\n", static_cast<long long>(QDateTime::currentMSecsSinceEpoch()), qPrintable(event), qPrintable(rest));
    std::fflush(stdout);
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QStringList args = app.arguments().mid(1);

    bool wantToken = false;
    bool tokenWindow = false;
    int gap = 0;
    int wait = 3000;
    QStringList specs;
    for (int i = 0; i < args.size(); ++i) {
        const QString &a = args.at(i);
        if (a == QLatin1String("--token")) {
            wantToken = true;
        } else if (a == QLatin1String("--token-window")) {
            wantToken = true;
            tokenWindow = true;
        } else if (a == QLatin1String("--gap")) {
            gap = args.value(++i).toInt();
        } else if (a == QLatin1String("--wait")) {
            wait = args.value(++i).toInt();
        } else {
            specs << a;
        }
    }
    if (specs.isEmpty()) {
        std::fprintf(stderr, "usage: s4launch [--token] [--token-window] [--gap MS] [--wait MS] ID[=ARGS]...\n");
        return 2;
    }
    out(QStringLiteral("platform"), QStringLiteral("wayland=%1 self_pid=%2").arg(KWindowSystem::isPlatformWayland()).arg(QCoreApplication::applicationPid()));

    QObject::connect(&app, &QCoreApplication::aboutToQuit, &app, [] {
        out(QStringLiteral("quit"), QString());
    });
    app.setQuitOnLastWindowClosed(false);
    // KJob holds a QEventLoopLocker: without this the app quits when the first job ends.
    app.setQuitLockEnabled(false);
    QWindow *helper = nullptr;
    if (tokenWindow) {
        helper = new QWindow;
        helper->setTitle(QStringLiteral("s4launch-helper"));
        helper->resize(10, 10);
        helper->show();
    }

    int pending = specs.size();
    auto finishedOne = [&] {
        if (--pending == 0) {
            QTimer::singleShot(wait, &app, &QCoreApplication::quit);
        }
    };

    auto startJob = [&](int index, const QString &spec) {
        const QString id = spec.section(QLatin1Char('='), 0, 0);
        const QString extra = spec.section(QLatin1Char('='), 1);
        KService::Ptr orig = KService::serviceByDesktopName(id);
        if (!orig) {
            out(QStringLiteral("error"), QStringLiteral("index=%1 id=%2 msg=service-not-found").arg(index).arg(id));
            finishedOne();
            return;
        }
        KService::Ptr service(new KService(*orig));
        if (!extra.isEmpty()) {
            service->setExec(orig->exec() + QLatin1Char(' ') + extra);
        }

        auto run = [&, index, id, service](const QString &token) {
            auto *job = new KIO::ApplicationLauncherJob(service);
            if (!token.isEmpty()) {
                job->setStartupId(token.toUtf8());
            }
            out(QStringLiteral("launch"),
                QStringLiteral("index=%1 id=%2 exec=\"%3\" token=%4").arg(index).arg(id, service->exec(), token.isEmpty() ? QStringLiteral("-") : token));
            QObject::connect(job, &KJob::result, &app, [&, job, index, id] {
                QStringList pids;
                for (qint64 p : job->pids()) {
                    pids << QString::number(p);
                }
                out(QStringLiteral("result"),
                    QStringLiteral("index=%1 id=%2 error=%3 pid=%4 pids=%5 msg=\"%6\"")
                        .arg(index)
                        .arg(id)
                        .arg(job->error())
                        .arg(job->pid())
                        .arg(pids.join(QLatin1Char(',')), job->errorString()));
                finishedOne();
            });
            job->start();
        };

        if (!wantToken) {
            run(QString());
            return;
        }
        // appId for the token: the desktop id is what KIO itself passes.
        auto future = helper ? KWaylandExtras::xdgActivationToken(helper, id) : KWaylandExtras::xdgActivationToken(nullptr, 0, id);
        auto *watcher = new QFutureWatcher<QString>(&app);
        QObject::connect(watcher, &QFutureWatcher<QString>::finished, &app, [watcher, run] {
            run(watcher->result());
            watcher->deleteLater();
        });
        watcher->setFuture(future);
    };

    for (int i = 0; i < specs.size(); ++i) {
        // Copy the spec: the lambda outlives this loop iteration.
        const QString spec = specs.at(i);
        // With a helper window, give it a moment to be mapped first.
        const int base = helper ? 500 : 0;
        QTimer::singleShot(base + i * gap, &app, [&startJob, i, spec] {
            startJob(i, spec);
        });
    }
    return app.exec();
}
