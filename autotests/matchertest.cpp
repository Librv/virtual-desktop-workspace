// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "matcher.h"

#include <QTest>

using namespace Vdw;

namespace
{
constexpr int Timeout = 10000;

PendingLaunch launch(int id, qint64 pid, const QString &app, qint64 timeMs = 0)
{
    return {id, pid, app, QRect(id * 100, 0, 100, 100), timeMs};
}

ObservedWindow window(qint64 pid, const QString &app, qint64 timeMs = 100, const QString &resourceClass = QString())
{
    return {pid, app, resourceClass, QStringLiteral("{win-%1}").arg(pid), timeMs};
}

ObservedWindow windowWithId(const QString &id, qint64 pid, const QString &app, qint64 timeMs)
{
    return {pid, app, QString(), id, timeMs};
}

ObservedWindow fixedWindow(const QString &id, qint64 pid, const QString &app, qint64 timeMs)
{
    return {pid, app, QString(), id, timeMs, false};
}

QString handoverId(Matcher &m, const QString &closedId, qint64 nowMs)
{
    const auto h = m.release(closedId, nowMs);
    return h ? h->windowId : QString();
}

int claimedId(Matcher &m, const ObservedWindow &w)
{
    const auto c = m.claim(w);
    return c ? c->launchId : -1;
}

const QString Konsole = QStringLiteral("org.kde.konsole");
const QString Dolphin = QStringLiteral("org.kde.dolphin");
const QString Discord = QStringLiteral("discord");
const QString Steam = QStringLiteral("steam");
}

class MatcherTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // S4: windows of three simultaneous launches appear in another order.
    void testOutOfOrderArrivalMatchesByPid()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 101, Konsole));
        m.addPending(launch(1, 102, Konsole));
        m.addPending(launch(2, 103, Konsole));
        QCOMPARE(claimedId(m, window(102, Konsole)), 1);
        QCOMPARE(claimedId(m, window(101, Konsole)), 0);
        QCOMPARE(claimedId(m, window(103, Konsole)), 2);
        QCOMPARE(m.pendingCount(), 0);
    }

    // The pid wins over an older pending launch of the same app.
    void testPidBeatsOlderSameApp()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 101, Konsole, 0));
        m.addPending(launch(1, 102, Konsole, 10));
        QCOMPARE(claimedId(m, window(102, Konsole)), 1);
        QCOMPARE(claimedId(m, window(101, Konsole)), 0);
    }

    // S4: single-process Konsole. No window has a job pid; launch order.
    void testSharedProcessFallsBackToOldestSameApp()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 201, Konsole, 0));
        m.addPending(launch(1, 202, Konsole, 5));
        m.addPending(launch(2, 203, Konsole, 10));
        QCOMPARE(claimedId(m, window(999, Konsole)), 0);
        QCOMPARE(claimedId(m, window(999, Konsole)), 1);
        QCOMPARE(claimedId(m, window(999, Konsole)), 2);
        QCOMPARE(claimedId(m, window(999, Konsole)), -1);
    }

    // S4: shared process where the window pid equals one of the job pids.
    void testSharedProcessWhereOnePidMatches()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 301, Konsole, 0));
        m.addPending(launch(1, 302, Konsole, 5));
        m.addPending(launch(2, 303, Konsole, 10));
        // All three windows belong to process 302.
        QCOMPARE(claimedId(m, window(302, Konsole)), 1);
        QCOMPARE(claimedId(m, window(302, Konsole)), 0);
        QCOMPARE(claimedId(m, window(302, Konsole)), 2);
    }

    // S4: an unrelated window of the same app opened before our launch
    // existed has nothing to steal.
    void testUnrelatedWindowBeforeLaunchIsIgnored()
    {
        Matcher m(Timeout);
        QCOMPARE(claimedId(m, window(500, Konsole, 50)), -1);
        m.addPending(launch(0, 401, Konsole, 100));
        QCOMPARE(claimedId(m, window(401, Konsole, 200)), 0);
    }

    // Known limit (S4, accepted): an unrelated window of the same app that
    // arrives while launches are pending takes the oldest one, but never one
    // that a later pid match still needs once that window is there.
    void testUnrelatedWindowDuringLaunchesTakesOldestOnly()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 601, Konsole, 0));
        m.addPending(launch(1, 602, Konsole, 5));
        QCOMPARE(claimedId(m, window(777, Konsole, 50)), 0);
        // Launch 1 is still claimable by its own pid.
        QCOMPARE(claimedId(m, window(602, Konsole, 60)), 1);
        // Launch 0's own window now finds nothing and is left alone.
        QCOMPARE(claimedId(m, window(601, Konsole, 70)), -1);
    }

    void testOtherAppIsNotMatched()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 701, Konsole));
        QCOMPARE(claimedId(m, window(800, Dolphin)), -1);
        QCOMPARE(m.pendingCount(), 1);
    }

    void testResourceClassWhenDesktopFileNameEmpty()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 901, Konsole));
        QCOMPARE(claimedId(m, window(800, QString(), 100, Konsole)), 0);
    }

    void testEmptyAppIdMatchesNothing()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 901, Konsole));
        QCOMPARE(claimedId(m, window(800, QString(), 100, QString())), -1);
    }

    void testPidZeroIsNotAPidMatch()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 0, Konsole, 0)); // pid not known yet
        m.addPending(launch(1, 0, Dolphin, 1));
        // A window with unknown pid 0 must not match "pid 0" of a launch; it
        // is matched by app id like any other.
        QCOMPARE(claimedId(m, window(0, Dolphin)), 1);
    }

    void testPidSetLater()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 0, Konsole, 0));
        m.addPending(launch(1, 0, Konsole, 1));
        QVERIFY(m.setPid(1, 1002));
        QVERIFY(!m.setPid(42, 1));
        QCOMPARE(claimedId(m, window(1002, Konsole)), 1);
    }

    void testRemove()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 1, Konsole));
        QVERIFY(m.remove(0));
        QVERIFY(!m.remove(0));
        QCOMPARE(claimedId(m, window(1, Konsole)), -1);
    }

    void testClaimedOnlyOnce()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 1001, Konsole));
        QCOMPARE(claimedId(m, window(1001, Konsole)), 0);
        QCOMPARE(claimedId(m, window(1001, Konsole)), -1);
    }

    void testClaimCarriesRect()
    {
        Matcher m(Timeout);
        PendingLaunch l = launch(3, 5, Konsole);
        l.rect = QRect(10, 20, 30, 40);
        m.addPending(l);
        const auto c = m.claim(window(5, Konsole));
        QVERIFY(c);
        QCOMPARE(c->rect, QRect(10, 20, 30, 40));
        QCOMPARE(c->appId, Konsole);
    }

    // S4: Kate handing a file to a running instance opens no window.
    void testNoWindowAtAllExpires()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 1101, QStringLiteral("org.kde.kate"), 1000));
        QVERIFY(m.expire(1000 + Timeout).isEmpty()); // exactly at the limit: still pending
        QCOMPARE(m.pendingCount(), 1);
        const auto expired = m.expire(1000 + Timeout + 1);
        QCOMPARE(expired.size(), 1);
        QCOMPARE(expired.first().launchId, 0);
        QCOMPARE(m.pendingCount(), 0);
        QVERIFY(m.expire(99999).isEmpty());
    }

    void testWindowAfterExpiryIsNotMatched()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 1201, Konsole, 0));
        QCOMPARE(claimedId(m, window(1201, Konsole, Timeout + 1)), -1);
        QCOMPARE(m.pendingCount(), 0);
    }

    void testWindowJustInTimeIsMatched()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 1301, Konsole, 0));
        QCOMPARE(claimedId(m, window(1301, Konsole, Timeout)), 0);
    }

    void testExpiryOnlyDropsOldEntries()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 1, Konsole, 0));
        m.addPending(launch(1, 2, Konsole, 8000));
        const auto expired = m.expire(12000);
        QCOMPARE(expired.size(), 1);
        QCOMPARE(expired.first().launchId, 0);
        QCOMPARE(m.pendingCount(), 1);
    }

    void testConfigurableTimeout()
    {
        Matcher m(500);
        QCOMPARE(m.timeoutMs(), 500);
        m.addPending(launch(0, 1, Konsole, 0));
        m.setTimeoutMs(2000);
        QVERIFY(m.expire(1500).isEmpty());
        QCOMPARE(m.expire(2001).size(), 1);
    }

    void testOutOfOrderAddKeepsLaunchOrder()
    {
        Matcher m(Timeout);
        m.addPending(launch(1, 2, Konsole, 10));
        m.addPending(launch(0, 1, Konsole, 0));
        QCOMPARE(claimedId(m, window(999, Konsole, 20)), 0);
    }

    // Discord: the main window opens before the updater closes.
    void testHandoverWindowOpenedBeforeClose()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Discord, 0));
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("updater"), 100, Discord, 500)), 0);
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("main"), 100, Discord, 2500)), -1);
        const auto h = m.release(QStringLiteral("updater"), 2600);
        QVERIFY(h);
        QCOMPARE(h->windowId, QStringLiteral("main"));
        QCOMPARE(h->launch.launchId, 0);
        QCOMPARE(h->launch.rect, QRect(0, 0, 100, 100));
    }

    // The window opened exactly HandoverGapMs before the close still counts.
    void testHandoverGapBoundaryBeforeClose()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Discord, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Discord, 0));
        claimedId(m, windowWithId(QStringLiteral("b"), 100, Discord, 2000));
        QCOMPARE(handoverId(m, QStringLiteral("a"), 3000), QStringLiteral("b"));
    }

    // Steam: the loading window closes, a window of another pid follows.
    void testHandoverWindowOpenedAfterClose()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("loading"), 100, Steam, 500)), 0);
        QVERIFY(!m.release(QStringLiteral("loading"), 3000));
        const auto c = m.claim(windowWithId(QStringLiteral("main"), 200, Steam, 3300));
        QVERIFY(c);
        QCOMPARE(c->launchId, 0);
        QCOMPARE(c->rect, QRect(0, 0, 100, 100));
    }

    // A second startup window that already closed does not take the slot;
    // the window that opens after the close does.
    void testClosedUnmatchedWindowGetsNoHandover()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Discord, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Discord, 0));
        claimedId(m, windowWithId(QStringLiteral("b"), 100, Discord, 1000));
        QVERIFY(!m.release(QStringLiteral("b"), 1200));
        QCOMPARE(handoverId(m, QStringLiteral("a"), 1500), QString());
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("main"), 100, Discord, 1800)), 0);
    }

    void testHandoverWindowMoreThanGapBeforeCloseIsIgnored()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Discord, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Discord, 0));
        claimedId(m, windowWithId(QStringLiteral("b"), 100, Discord, 1999));
        QCOMPARE(handoverId(m, QStringLiteral("a"), 3000), QString());
    }

    void testHandoverWindowMoreThanGapAfterCloseIsIgnored()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Steam, 0));
        QVERIFY(!m.release(QStringLiteral("a"), 3000));
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("b"), 200, Steam, 4001)), -1);
        // Exactly at the limit it still matches.
        Matcher m2(Timeout);
        m2.addPending(launch(0, 100, Steam, 0));
        claimedId(m2, windowWithId(QStringLiteral("a"), 100, Steam, 0));
        m2.release(QStringLiteral("a"), 3000);
        QCOMPARE(claimedId(m2, windowWithId(QStringLiteral("b"), 200, Steam, 4000)), 0);
    }

    void testCloseAfterWatchTimeDoesNotRearm()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Steam, 1000));
        QVERIFY(!m.release(QStringLiteral("a"), 1000 + Matcher::HandoverWatchMs + 1));
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("b"), 200, Steam, 1000 + Matcher::HandoverWatchMs + 2)), -1);
    }

    void testCloseAtWatchTimeStillRearms()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Steam, 1000));
        const qint64 close = 1000 + Matcher::HandoverWatchMs;
        QVERIFY(!m.release(QStringLiteral("a"), close));
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("b"), 200, Steam, close + 10)), 0);
    }

    // The watch time counts from the first placement, not from a hand-over.
    void testChainedHandoverCountsFromFirstPlacement()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Discord, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Discord, 0));
        // First hand-over, 4 min in.
        claimedId(m, windowWithId(QStringLiteral("b"), 100, Discord, 240000));
        QCOMPARE(handoverId(m, QStringLiteral("a"), 240100), QStringLiteral("b"));
        // Second hand-over, 4.9 min in: still watched.
        claimedId(m, windowWithId(QStringLiteral("c"), 100, Discord, 294000));
        QCOMPARE(handoverId(m, QStringLiteral("b"), 294500), QStringLiteral("c"));
        // Third close at 5 min + 1 ms after the first placement: no hand-over.
        claimedId(m, windowWithId(QStringLiteral("d"), 100, Discord, 300000));
        QCOMPARE(handoverId(m, QStringLiteral("c"), 300001), QString());
    }

    void testChainedHandoverByClaimAfterClose()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Steam, 0));
        m.release(QStringLiteral("a"), 100000);
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("b"), 200, Steam, 100100)), 0);
        m.release(QStringLiteral("b"), 200000);
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("c"), 300, Steam, 200100)), 0);
        // 300001 ms after the first placement.
        QVERIFY(!m.release(QStringLiteral("c"), 300001));
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("d"), 400, Steam, 300100)), -1);
    }

    // An app that keeps two windows open: the second one gets nothing.
    void testTwoOpenWindowsGetNoHandover()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Konsole, 0));
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("a"), 100, Konsole, 100)), 0);
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("b"), 100, Konsole, 200)), -1);
        // Closing an unplaced window does nothing.
        QVERIFY(!m.release(QStringLiteral("b"), 5000));
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("c"), 100, Konsole, 5100)), -1);
    }

    void testOrdinaryPendingBeatsRearmed()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Steam, 0));
        m.addPending(launch(1, 0, Steam, 1000));
        m.release(QStringLiteral("a"), 2000);
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("b"), 200, Steam, 2100)), 1);
        // The re-armed launch is still waiting for the next one.
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("c"), 300, Steam, 2200)), 0);
    }

    // The launcher reports the pid after the window was claimed.
    void testSetPidAfterClaimUsedByHandover()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 0, Discord, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Discord, 0));
        QVERIFY(m.setPid(0, 555));
        // Not the app id, not the closed pid: only the launch pid links it.
        claimedId(m, windowWithId(QStringLiteral("b"), 555, QStringLiteral("other"), 1000));
        QCOMPARE(handoverId(m, QStringLiteral("a"), 1100), QStringLiteral("b"));
    }

    void testSetPidAfterCloseUsedByClaim()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 0, Discord, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Discord, 0));
        m.release(QStringLiteral("a"), 1000);
        QVERIFY(m.setPid(0, 555));
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("b"), 555, QStringLiteral("other"), 1100)), 0);
    }

    void testHandoverUnrelatedWindowIsIgnored()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Discord, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Discord, 0));
        claimedId(m, windowWithId(QStringLiteral("x"), 900, Dolphin, 1500));
        QCOMPARE(handoverId(m, QStringLiteral("a"), 2000), QString());
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("y"), 901, Dolphin, 2100)), -1);
    }

    void testHandoverPicksNewestWindow()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Discord, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Discord, 0));
        claimedId(m, windowWithId(QStringLiteral("b"), 100, Discord, 1500));
        claimedId(m, windowWithId(QStringLiteral("c"), 100, Discord, 1600));
        QCOMPARE(handoverId(m, QStringLiteral("a"), 2000), QStringLiteral("c"));
    }

    void testReleaseUnknownWindow()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Discord, 0));
        QVERIFY(!m.release(QStringLiteral("nope"), 100));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Discord, 0));
        QVERIFY(!m.release(QStringLiteral("nope"), 100));
        // A window hands over once: a second close of it is unknown.
        QVERIFY(!m.release(QStringLiteral("a"), 200));
        QVERIFY(!m.release(QStringLiteral("a"), 300));
    }

    // Discord: the fixed-size updater is never placed, the resizable main window is.
    void testFixedSizeWindowNeverClaims()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Discord, 0));
        QCOMPARE(claimedId(m, fixedWindow(QStringLiteral("updater"), 100, Discord, 100)), -1);
        QCOMPARE(m.pendingCount(), 1);
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("main"), 100, Discord, 2000)), 0);
        QCOMPARE(m.pendingCount(), 0);
        QVERIFY(!m.release(QStringLiteral("updater"), 2100));
    }

    void testFixedSizeWindowMatchingNothingIsLeftAlone()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Konsole, 0));
        QCOMPARE(claimedId(m, fixedWindow(QStringLiteral("x"), 5, Dolphin, 100)), -1);
        QVERIFY(m.nextDeadlineMs());
        QCOMPARE(*m.nextDeadlineMs(), qint64(Timeout));
    }

    // Steam: two fixed-size windows in a row, then the main one.
    void testTwoFixedSizeWindowsThenMain()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        QCOMPARE(claimedId(m, fixedWindow(QStringLiteral("update"), 100, Steam, 100)), -1);
        QVERIFY(!m.release(QStringLiteral("update"), 3000));
        QCOMPARE(claimedId(m, fixedWindow(QStringLiteral("loading"), 100, Steam, 3500)), -1);
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("main"), 100, Steam, 5700)), 0);
        // The loading window closing after the claim changes nothing.
        QVERIFY(!m.release(QStringLiteral("loading"), 6200));
        QCOMPARE(m.pendingCount(), 0);
    }

    void testHoldOutlivesTimeout()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, fixedWindow(QStringLiteral("update"), 100, Steam, 100));
        QVERIFY(m.expire(60000).isEmpty());
        QCOMPARE(m.pendingCount(), 1);
        QCOMPARE(*m.nextDeadlineMs(), Matcher::HandoverWatchMs);
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("main"), 100, Steam, 120000)), 0);
    }

    void testHoldPastWatchTimeExpires()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, fixedWindow(QStringLiteral("update"), 100, Steam, 100));
        QVERIFY(m.expire(Matcher::HandoverWatchMs).isEmpty());
        const auto expired = m.expire(Matcher::HandoverWatchMs + 1);
        QCOMPARE(expired.size(), 1);
        QCOMPARE(expired.first().launchId, 0);
        QCOMPARE(m.pendingCount(), 0);
        QVERIFY(!m.nextDeadlineMs());
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("main"), 100, Steam, Matcher::HandoverWatchMs + 2)), -1);
    }

    void testHolderClosesThenMainWithinGapClaims()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, fixedWindow(QStringLiteral("update"), 100, Steam, 100));
        QVERIFY(!m.release(QStringLiteral("update"), 30000));
        QVERIFY(m.expire(30000 + Matcher::HandoverGapMs).isEmpty());
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("main"), 100, Steam, 30000 + Matcher::HandoverGapMs)), 0);
    }

    void testHolderClosesThenMainAfterGapDoesNotClaim()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, fixedWindow(QStringLiteral("update"), 100, Steam, 100));
        QVERIFY(!m.release(QStringLiteral("update"), 30000));
        const auto expired = m.expire(30000 + Matcher::HandoverGapMs + 1);
        QCOMPARE(expired.size(), 1);
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("main"), 100, Steam, 40000)), -1);
    }

    // The grace after a close never ends the launch before its normal timeout.
    void testHolderClosesEarlyKeepsNormalTimeout()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Discord, 0));
        claimedId(m, fixedWindow(QStringLiteral("updater"), 100, Discord, 100));
        QVERIFY(!m.release(QStringLiteral("updater"), 500));
        QCOMPARE(*m.nextDeadlineMs(), qint64(Timeout));
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("main"), 100, Discord, 9000)), 0);
    }

    void testSecondHolderRestartsHold()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, fixedWindow(QStringLiteral("a"), 100, Steam, 100));
        m.release(QStringLiteral("a"), 30000);
        claimedId(m, fixedWindow(QStringLiteral("b"), 100, Steam, 30500));
        QVERIFY(m.expire(50000).isEmpty());
        m.release(QStringLiteral("b"), 50000);
        QVERIFY(m.expire(50000 + Matcher::HandoverGapMs).isEmpty());
        QCOMPARE(m.expire(50000 + Matcher::HandoverGapMs + 1).size(), 1);
    }

    void testNewlyHeldReportedOnce()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        std::optional<PendingLaunch> held;
        m.claim(fixedWindow(QStringLiteral("a"), 100, Steam, 100), &held);
        QVERIFY(held);
        QCOMPARE(held->launchId, 0);
        held.reset();
        m.claim(fixedWindow(QStringLiteral("b"), 100, Steam, 200), &held);
        QVERIFY(!held);
    }

    // A later holder after the first one closed holds the launch again.
    void testNewlyHeldReportedAgainAfterHolderClosed()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, fixedWindow(QStringLiteral("a"), 100, Steam, 100));
        m.release(QStringLiteral("a"), 300);
        std::optional<PendingLaunch> held;
        m.claim(fixedWindow(QStringLiteral("b"), 100, Steam, 500), &held);
        QVERIFY(held);
        QCOMPARE(held->launchId, 0);
    }

    // Two holders open at once: the launch stays held until both closed.
    void testOverlappingHoldersHoldUntilLastCloses()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, fixedWindow(QStringLiteral("a"), 100, Steam, 100));
        claimedId(m, fixedWindow(QStringLiteral("b"), 100, Steam, 200));
        m.release(QStringLiteral("a"), 20000);
        QVERIFY(m.expire(40000).isEmpty());
        QCOMPARE(m.pendingCount(), 1);
        m.release(QStringLiteral("b"), 40000);
        QVERIFY(m.expire(40000 + Matcher::HandoverGapMs).isEmpty());
        QCOMPARE(m.expire(40000 + Matcher::HandoverGapMs + 1).size(), 1);
    }

    void testFixedSizeWindowHoldsRearmedLaunch()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Steam, 0));
        QVERIFY(!m.release(QStringLiteral("a"), 1000));
        QCOMPARE(claimedId(m, fixedWindow(QStringLiteral("loading"), 100, Steam, 1500)), -1);
        // Held beyond the usual HandoverGapMs.
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("main"), 100, Steam, 60000)), 0);
    }

    void testFixedSizeWindowNeverGetsHandover()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Steam, 0));
        QCOMPARE(claimedId(m, fixedWindow(QStringLiteral("fixed"), 100, Steam, 1500)), -1);
        QVERIFY(!m.release(QStringLiteral("a"), 2000));
        QVERIFY(!m.release(QStringLiteral("fixed"), 2100));
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("main"), 100, Steam, 2200)), 0);
    }

    void testFixedSizeWindowAfterRearmedExpiryIsIgnored()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Steam, 0));
        m.release(QStringLiteral("a"), 1000);
        QCOMPARE(claimedId(m, fixedWindow(QStringLiteral("late"), 100, Steam, 5000)), -1);
        QCOMPARE(claimedId(m, windowWithId(QStringLiteral("main"), 100, Steam, 5100)), -1);
    }

    void testRemoveDropsHold()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, fixedWindow(QStringLiteral("a"), 100, Steam, 100));
        QVERIFY(m.remove(0));
        QVERIFY(!m.nextDeadlineMs());
    }

    void testRearmedExpiresSilently()
    {
        Matcher m(Timeout);
        m.addPending(launch(0, 100, Steam, 0));
        claimedId(m, windowWithId(QStringLiteral("a"), 100, Steam, 0));
        m.release(QStringLiteral("a"), 1000);
        QCOMPARE(m.pendingCount(), 0);
        QVERIFY(m.expire(1000000).isEmpty());
    }
};

QTEST_GUILESS_MAIN(MatcherTest)

#include "matchertest.moc"
