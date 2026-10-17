#pragma once

#include <QObject>
#include <QString>

#include <functional>

namespace Vdw
{

/// Starts apps from their .desktop id with extra arguments, through
/// KIO::ApplicationLauncherJob (docs/spikes/S4.md).
class Launcher : public QObject
{
    Q_OBJECT

public:
    /// `ok` false means nothing was started and `error` says why; on success
    /// `pid` is the pid of the process KIO spawned.
    using Callback = std::function<void(bool ok, qint64 pid, const QString &error)>;

    explicit Launcher(QObject *parent = nullptr);

    /// `args` is one string split with KShell::splitArgs and appended to the
    /// service's `Exec`. `callback` is called exactly once (possibly before
    /// launch() returns, for an unknown app).
    void launch(const QString &appId, const QString &args, Callback callback);
};

}
