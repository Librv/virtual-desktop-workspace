// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "matcher.h"

#include <algorithm>

namespace Vdw
{

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
    return false;
}

bool Matcher::remove(int launchId)
{
    return m_pending.removeIf([launchId](const PendingLaunch &p) {
        return p.launchId == launchId;
    }) > 0;
}

QList<PendingLaunch> Matcher::expire(qint64 nowMs)
{
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
        const QString &appId = window.desktopFileName.isEmpty() ? window.resourceClass : window.desktopFileName;
        if (!appId.isEmpty()) {
            found = std::find_if(m_pending.begin(), m_pending.end(), [&](const PendingLaunch &p) {
                return p.appId == appId;
            });
        }
    }
    if (found == m_pending.end()) {
        return std::nullopt;
    }
    const PendingLaunch launch = *found;
    m_pending.erase(found);
    return launch;
}

}
