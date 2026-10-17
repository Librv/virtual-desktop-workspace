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

int claimedId(Matcher &m, const ObservedWindow &w)
{
    const auto c = m.claim(w);
    return c ? c->launchId : -1;
}

const QString Konsole = QStringLiteral("org.kde.konsole");
const QString Dolphin = QStringLiteral("org.kde.dolphin");
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
};

QTEST_GUILESS_MAIN(MatcherTest)

#include "matchertest.moc"
