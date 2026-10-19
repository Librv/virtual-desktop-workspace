// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include <QHash>
#include <QList>
#include <QRect>
#include <QString>
#include <QStringList>

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
    bool resizeable = true; ///< fixed-size windows are startup windows: never placed
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
/// 5. A window that is not resizeable is a startup window (Discord updater,
///    Steam loading window): it is never placed, on any path, and it does not
///    consume a launch. If it matches a launch (same rules as 1 and 2, also a
///    launch that waits for a hand-over) it holds that launch: the launch does
///    not expire while the window is open, up to HandoverWatchMs after the
///    launch time. When the window closes (`release`), the launch waits
///    HandoverGapMs more for the main window, but never ends earlier than its
///    normal timeout. Several fixed-size windows in a row each hold the launch.
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
    /// `window.timeMs` are dropped first. A fixed-size window never consumes a
    /// launch: it holds a matching one and returns nothing; `newlyHeld` is set
    /// to the launch when this window starts holding it (for logging).
    std::optional<PendingLaunch> claim(const ObservedWindow &window, std::optional<PendingLaunch> *newlyHeld = nullptr);

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

    /// True while `launchId` is pending (not claimed, expired or removed). A
    /// held launch is pending.
    bool contains(int launchId) const;

    /// Launches that wait for their first window (not hand-over state). A
    /// held launch counts.
    int pendingCount() const
    {
        return m_pending.size();
    }

    /// The time after which `expire` returns the next pending launch (it
    /// expires once `nowMs` is greater than this), or nothing without pending
    /// launches. Held launches move it, so call it again after each change.
    std::optional<qint64> nextDeadlineMs() const;

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

    /// Fixed-size windows that hold a launch.
    struct Hold {
        QStringList openWindowIds;
        qint64 graceEndMs = 0; ///< last close + HandoverGapMs
    };

    void prune(qint64 nowMs);
    /// Drop the holds of launches that are no longer pending or re-armed.
    void purgeHolds();
    /// When a launch ends: `baseMs` or, if held, later.
    qint64 deadlineWithHold(const PendingLaunch &launch, qint64 baseMs) const;
    qint64 pendingDeadline(const PendingLaunch &launch) const;
    /// The launch a window belongs to, pending first, else re-armed. Null if none.
    const PendingLaunch *findLaunch(const ObservedWindow &window) const;

    int m_timeoutMs;
    QList<PendingLaunch> m_pending; ///< in launch order (oldest first)
    QList<Placed> m_placed;
    QList<Rearmed> m_rearmed; ///< oldest first
    QHash<int, Hold> m_holds; ///< by launch id, for pending and re-armed launches
    QList<ObservedWindow> m_unmatched; ///< windows that matched nothing, oldest first
};

}
