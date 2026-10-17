// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include <QObject>
#include <QString>

#include <functional>

namespace Vdw
{

class UiClient;

/// Starts apps from their .desktop id with extra arguments. The work is done
/// by the vdw-ui helper through KIO::ApplicationLauncherJob (docs/spikes/S4.md,
/// docs/spikes/S5.md).
class Launcher : public QObject
{
    Q_OBJECT

public:
    /// `ok` false means nothing was started and `error` says why; on success
    /// `pid` is the pid of the process KIO spawned.
    using Callback = std::function<void(bool ok, qint64 pid, const QString &error)>;

    explicit Launcher(UiClient *ui, QObject *parent = nullptr);

    /// `args` is one string split with KShell::splitArgs and appended to the
    /// service's `Exec`. `callback` is called exactly once.
    void launch(const QString &appId, const QString &args, Callback callback);

private:
    UiClient *m_ui;
};

}
