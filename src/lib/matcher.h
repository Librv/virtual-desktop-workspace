#pragma once

#include <QList>
#include <QRect>
#include <QString>

#include <optional>

namespace Vdw
{

/// A launch whose window has not shown up yet.
struct PendingLaunch {
    int launchId = 0;
    qint64 pid = 0; ///< pid from the launcher job, 0 while unknown
    QString appId; ///< .desktop id of the launched app
    QRect rect; ///< target rect in pixels
    qint64 launchTimeMs = 0;
};

/// A window the KWin script saw appear.
struct ObservedWindow {
    qint64 pid = 0;
    QString desktopFileName;
    QString resourceClass;
    QString internalId;
    qint64 timeMs = 0;
};

/// Decides which pending launch an observed window belongs to
/// (docs/ARCHITECTURE.md, "Matching windows to launches"). Pure: the caller
/// passes all times, so it is unit-testable without a clock.
///
/// 1. A window whose pid equals the pid of a pending launch belongs to it.
/// 2. Otherwise the oldest pending launch with the same app id
///    (`desktopFileName`, or `resourceClass` when that is empty), for apps
///    that share one process.
/// 3. Launches expire after the placement timeout; a window that matches
///    nothing is left alone.
class Matcher
{
public:
    explicit Matcher(int timeoutMs = 10000);

    void setTimeoutMs(int timeoutMs);
    int timeoutMs() const
    {
        return m_timeoutMs;
    }

    void addPending(const PendingLaunch &launch);
    /// Set the job pid once the launcher reports it. False if the launch is gone.
    bool setPid(int launchId, qint64 pid);
    /// Forget a launch (it failed to start). False if it was not pending.
    bool remove(int launchId);

    /// Match a window and consume the launch. Launches that expired before
    /// `window.timeMs` are dropped first.
    std::optional<PendingLaunch> claim(const ObservedWindow &window);

    /// Drop and return the launches older than the timeout at `nowMs`.
    QList<PendingLaunch> expire(qint64 nowMs);

    int pendingCount() const
    {
        return m_pending.size();
    }

private:
    int m_timeoutMs;
    QList<PendingLaunch> m_pending; ///< in launch order (oldest first)
};

}
