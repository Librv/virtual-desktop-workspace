// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

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
    int group = 0; ///< one id per workspace open
    int order = 0; ///< slot index within the open, 0 stacks lowest
};

/// A window the KWin script saw appear.
struct ObservedWindow {
    qint64 pid = 0;
    QString desktopFileName;
    QString resourceClass;
    QString internalId;
    qint64 timeMs = 0;
};

/// A window that takes over the slot of a closed window.
struct Handover {
    QString windowId; ///< internalId of the new window
    PendingLaunch launch; ///< the launch whose slot it takes
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
/// 4. Hand-over, for apps that show a startup window first (Discord, Steam):
///    the first window still gets the slot. If that window closes
///    (`release`) and another window of the same launch opens within
///    HandoverGapMs of the close, before or after it, the new window takes
///    the slot. "Of the same launch" means the same pid as the launch or as
///    the closed window, or the same app id as the launch. It only applies
///    while the close is within HandoverWatchMs of the first placement. A
///    replacement can be replaced again, but the time still counts from the
///    first placement. Windows that stay open get no hand-over. Ordinary
///    pending launches match before launches that wait for a hand-over.
class Matcher
{
public:
    static constexpr qint64 HandoverGapMs = 1000;
    static constexpr qint64 HandoverWatchMs = 300000;

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

    /// A window passed to `claim` closed at `nowMs`; call it for every such
    /// window, so a closed one never takes a slot. For a placed window:
    /// returns the window that takes its slot now, if one opened within
    /// HandoverGapMs before. Otherwise the launch waits HandoverGapMs for the
    /// next matching `claim`. Does nothing more for an unplaced window or one
    /// that closes after HandoverWatchMs.
    std::optional<Handover> release(const QString &windowId, qint64 nowMs);

    /// Drop and return the pending launches older than the timeout at
    /// `nowMs`. Also drops stale hand-over state, silently.
    QList<PendingLaunch> expire(qint64 nowMs);

    /// Launches that wait for their first window (not hand-over state).
    int pendingCount() const
    {
        return m_pending.size();
    }

private:
    /// A launch whose window is open.
    struct Placed {
        PendingLaunch launch;
        QString windowId;
        qint64 pid = 0;
        qint64 firstPlacedMs = 0;
    };
    /// A launch whose window closed, waiting for the next window.
    struct Rearmed {
        PendingLaunch launch;
        qint64 closedPid = 0;
        qint64 firstPlacedMs = 0;
        qint64 deadlineMs = 0;
    };

    void prune(qint64 nowMs);

    int m_timeoutMs;
    QList<PendingLaunch> m_pending; ///< in launch order (oldest first)
    QList<Placed> m_placed;
    QList<Rearmed> m_rearmed; ///< oldest first
    QList<ObservedWindow> m_unmatched; ///< windows that matched nothing, oldest first
};

}
