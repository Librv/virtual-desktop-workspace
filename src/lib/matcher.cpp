// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "matcher.h"

#include <algorithm>

namespace Vdw
{

namespace
{
const QString &appIdOf(const ObservedWindow &window)
{
    return window.desktopFileName.isEmpty() ? window.resourceClass : window.desktopFileName;
}

bool belongsTo(const ObservedWindow &window, const PendingLaunch &launch, qint64 closedPid)
{
    if (window.pid > 0 && (window.pid == launch.pid || window.pid == closedPid)) {
        return true;
    }
    const QString &appId = appIdOf(window);
    return !appId.isEmpty() && appId == launch.appId;
}
}

Matcher::Matcher(int timeoutMs)
    : m_timeoutMs(timeoutMs)
{
}

void Matcher::setTimeoutMs(int timeoutMs)
{
    m_timeoutMs = timeoutMs;
}

void Matcher::addPending(const PendingLaunch &launch)
{
    // Keep launch order even if a caller adds out of order.
    const auto pos = std::upper_bound(m_pending.begin(), m_pending.end(), launch, [](const PendingLaunch &a, const PendingLaunch &b) {
        return a.launchTimeMs < b.launchTimeMs;
    });
    m_pending.insert(pos, launch);
}

bool Matcher::setPid(int launchId, qint64 pid)
{
    for (PendingLaunch &p : m_pending) {
        if (p.launchId == launchId) {
            p.pid = pid;
            return true;
        }
    }
    for (Placed &p : m_placed) {
        if (p.launch.launchId == launchId) {
            p.launch.pid = pid;
            return true;
        }
    }
    for (Rearmed &r : m_rearmed) {
        if (r.launch.launchId == launchId) {
            r.launch.pid = pid;
            return true;
        }
    }
    return false;
}

bool Matcher::remove(int launchId)
{
    return m_pending.removeIf([launchId](const PendingLaunch &p) {
        return p.launchId == launchId;
    }) > 0;
}

void Matcher::prune(qint64 nowMs)
{
    m_placed.removeIf([nowMs](const Placed &p) {
        return nowMs - p.firstPlacedMs > HandoverWatchMs;
    });
    m_rearmed.removeIf([nowMs](const Rearmed &r) {
        return nowMs > r.deadlineMs;
    });
    m_unmatched.removeIf([nowMs](const ObservedWindow &w) {
        return nowMs - w.timeMs > HandoverGapMs;
    });
}

std::optional<Handover> Matcher::release(const QString &windowId, qint64 nowMs)
{
    prune(nowMs);
    // A closed window can no longer take over a slot.
    m_unmatched.removeIf([&](const ObservedWindow &w) {
        return w.internalId == windowId;
    });
    const auto placedIt = std::find_if(m_placed.begin(), m_placed.end(), [&](const Placed &p) {
        return p.windowId == windowId;
    });
    if (placedIt == m_placed.end()) {
        return std::nullopt;
    }
    const Placed placed = *placedIt;
    m_placed.erase(placedIt);

    // The newest remembered window of the launch takes the slot now.
    for (auto it = m_unmatched.rbegin(); it != m_unmatched.rend(); ++it) {
        if (belongsTo(*it, placed.launch, placed.pid)) {
            const Handover handover{it->internalId, placed.launch};
            m_placed.append({placed.launch, it->internalId, it->pid, placed.firstPlacedMs});
            m_unmatched.erase(std::next(it).base());
            return handover;
        }
    }
    m_rearmed.append({placed.launch, placed.pid, placed.firstPlacedMs, nowMs + HandoverGapMs});
    return std::nullopt;
}

QList<PendingLaunch> Matcher::expire(qint64 nowMs)
{
    prune(nowMs);
    QList<PendingLaunch> expired;
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (nowMs - it->launchTimeMs > m_timeoutMs) {
            expired.append(*it);
            it = m_pending.erase(it);
        } else {
            ++it;
        }
    }
    return expired;
}

std::optional<PendingLaunch> Matcher::claim(const ObservedWindow &window)
{
    expire(window.timeMs);

    auto found = m_pending.end();
    if (window.pid > 0) {
        found = std::find_if(m_pending.begin(), m_pending.end(), [&](const PendingLaunch &p) {
            return p.pid == window.pid;
        });
    }
    if (found == m_pending.end()) {
        const QString &appId = appIdOf(window);
        if (!appId.isEmpty()) {
            found = std::find_if(m_pending.begin(), m_pending.end(), [&](const PendingLaunch &p) {
                return p.appId == appId;
            });
        }
    }
    if (found != m_pending.end()) {
        const PendingLaunch launch = *found;
        m_pending.erase(found);
        m_placed.append({launch, window.internalId, window.pid, window.timeMs});
        return launch;
    }

    // No ordinary launch: a launch whose window just closed may want it.
    const auto rearmed = std::find_if(m_rearmed.begin(), m_rearmed.end(), [&](const Rearmed &r) {
        return belongsTo(window, r.launch, r.closedPid);
    });
    if (rearmed != m_rearmed.end()) {
        const Rearmed r = *rearmed;
        m_rearmed.erase(rearmed);
        m_placed.append({r.launch, window.internalId, window.pid, r.firstPlacedMs});
        return r.launch;
    }

    m_unmatched.append(window);
    return std::nullopt;
}

}
