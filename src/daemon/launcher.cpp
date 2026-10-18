// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "launcher.h"

#include "uiclient.h"

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(VDW_DAEMON)

namespace Vdw
{

Launcher::Launcher(UiClient *ui, QObject *parent)
    : QObject(parent)
    , m_ui(ui)
{
}

void Launcher::launch(const QString &appId, const QString &args, Callback callback)
{
    m_ui->launch(appId, args, [appId, callback = std::move(callback)](bool ok, qint64 pid, const QString &error) {
        if (ok) {
            qCInfo(VDW_DAEMON) << "launched" << appId << "pid" << pid;
        }
        callback(ok, pid, error);
    });
}

}
