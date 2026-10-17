// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "launcher.h"

#include "execargs.h"

#include <KIO/ApplicationLauncherJob>
#include <KService>
#include <KShell>

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(VDW_DAEMON)

namespace Vdw
{

Launcher::Launcher(QObject *parent)
    : QObject(parent)
{
}

void Launcher::launch(const QString &appId, const QString &args, Callback callback)
{
    KService::Ptr original = KService::serviceByDesktopName(appId);
    if (!original) {
        callback(false, 0, QStringLiteral("unknown app %1").arg(appId));
        return;
    }

    KService::Ptr service(new KService(*original));
    if (!args.trimmed().isEmpty()) {
        KShell::Errors status = KShell::NoError;
        const QStringList split = KShell::splitArgs(args, KShell::AbortOnMeta | KShell::TildeExpand, &status);
        if (status != KShell::NoError) {
            callback(false, 0, QStringLiteral("cannot parse the arguments of %1: %2").arg(appId, args));
            return;
        }
        // The Exec line is parsed again by KIO: quote each argument so it
        // stays one argument.
        service->setExec(original->exec() + QLatin1Char(' ') + quoteExecArgs(split));
    }

    auto *job = new KIO::ApplicationLauncherJob(service, this);
    connect(job, &KJob::result, this, [job, appId, callback = std::move(callback)] {
        if (job->error()) {
            callback(false, 0, job->errorString());
            return;
        }
        qCInfo(VDW_DAEMON) << "launched" << appId << "pid" << job->pid();
        callback(true, job->pid(), QString());
    });
    job->start();
}

}
