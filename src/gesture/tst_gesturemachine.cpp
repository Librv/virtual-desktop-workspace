// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "gesturemachine.h"

#include <QTest>

using namespace Vdw;
using K = Command::Kind;

namespace
{
const Shortcut kMain = Shortcut::main();
const Shortcut kWsA = Shortcut::workspace(QStringLiteral("a"));
const QString kDefault = QStringLiteral("def");

QString describe(const Commands &cmds)
{
    QStringList parts;
    for (const Command &c : cmds) {
        switch (c.kind) {
        case K::StartTimer:
            parts << QStringLiteral("StartTimer(%1)").arg(c.ms);
            break;
        case K::ShowMenu:
            parts << QStringLiteral("ShowMenu");
            break;
        case K::HideMenu:
            parts << QStringLiteral("HideMenu");
            break;
        case K::Open:
            parts << QStringLiteral("Open(%1,%2)").arg(c.workspaceId, c.force ? QStringLiteral("force") : QStringLiteral("plain"));
            break;
        case K::ShowConfirmOsd:
            parts << QStringLiteral("ShowConfirmOsd(%1)").arg(c.workspaceId);
            break;
        case K::HideConfirmOsd:
            parts << QStringLiteral("HideConfirmOsd");
            break;
        }
    }
    return parts.join(QLatin1Char(' '));
}

/// Token of the (single) StartTimer command in cmds.
int tokenOf(const Commands &cmds)
{
    for (const Command &c : cmds) {
        if (c.kind == K::StartTimer) {
            return c.token;
        }
    }
    return -1;
}
}

#define CMDS(actual, expected) QCOMPARE(describe(actual), QStringLiteral(expected))

class GestureMachineTest : public QObject
{
    Q_OBJECT

private:
    GestureMachine make()
    {
        GestureConfig cfg;
        cfg.defaultWorkspaceId = kDefault;
        return GestureMachine(cfg);
    }

    /// Tap on the main shortcut that finds the desktop busy; returns the confirm timer token.
    void tapBusy(GestureMachine &m, int &token)
    {
        m.press(kMain);
        CMDS(m.release(kMain), "Open(def,plain)");
        const Commands c = m.openResult(kDefault, true);
        CMDS(c, "ShowConfirmOsd(def) StartTimer(2000)");
        token = tokenOf(c);
    }

    /// Menu pick of workspace "x" that finds the desktop busy; returns the confirm timer token.
    void menuPickBusy(GestureMachine &m, int &token)
    {
        const int hold = tokenOf(m.press(kMain));
        CMDS(m.timerFired(hold), "ShowMenu");
        CMDS(m.release(kMain), "");
        CMDS(m.menuResult(QStringLiteral("x")), "HideMenu Open(x,plain)");
        const Commands c = m.openResult(QStringLiteral("x"), true);
        CMDS(c, "ShowConfirmOsd(x) StartTimer(2000)");
        token = tokenOf(c);
    }

private Q_SLOTS:
    void tapOpensDefault()
    {
        GestureMachine m = make();
        CMDS(m.press(kMain), "StartTimer(250)");
        CMDS(m.release(kMain), "Open(def,plain)");
        CMDS(m.openResult(kDefault, false), "");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
    }

    void holdShowsMenu()
    {
        GestureMachine m = make();
        const int t = tokenOf(m.press(kMain));
        CMDS(m.timerFired(t), "ShowMenu");
        QCOMPARE(m.state(), GestureMachine::State::Menu);
    }

    void configIsUsed()
    {
        GestureMachine m = make();
        GestureConfig cfg;
        cfg.defaultWorkspaceId = QStringLiteral("other");
        cfg.holdThresholdMs = 400;
        cfg.confirmTimeoutMs = 3000;
        m.setConfig(cfg);
        CMDS(m.press(kMain), "StartTimer(400)");
        CMDS(m.release(kMain), "Open(other,plain)");
        CMDS(m.openResult(QStringLiteral("other"), true), "ShowConfirmOsd(other) StartTimer(3000)");
    }

    void menuPickOpens()
    {
        GestureMachine m = make();
        const int t = tokenOf(m.press(kMain));
        m.timerFired(t);
        CMDS(m.release(kMain), "");
        CMDS(m.menuResult(QStringLiteral("x")), "HideMenu Open(x,plain)");
        CMDS(m.openResult(QStringLiteral("x"), false), "");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
    }

    void menuDeadZoneCancels()
    {
        GestureMachine m = make();
        const int t = tokenOf(m.press(kMain));
        m.timerFired(t);
        m.release(kMain);
        CMDS(m.menuResult(std::nullopt), "HideMenu");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
    }

    void menuResultOutsideMenuIgnored()
    {
        GestureMachine m = make();
        CMDS(m.menuResult(QStringLiteral("x")), "");
        const int t = tokenOf(m.press(kMain));
        m.timerFired(t);
        // Still pressed, not released yet.
        CMDS(m.menuResult(QStringLiteral("x")), "");
        QCOMPARE(m.state(), GestureMachine::State::Menu);
    }

    void workspaceShortcutOpensOnRelease()
    {
        GestureMachine m = make();
        CMDS(m.press(kWsA), "");
        CMDS(m.release(kWsA), "Open(a,plain)");
    }

    void workspaceShortcutNeverShowsMenu()
    {
        GestureMachine m = make();
        CMDS(m.press(kWsA), "");
        QCOMPARE(m.state(), GestureMachine::State::WorkspacePressed);
        CMDS(m.release(kWsA), "Open(a,plain)");
    }

    void workspaceShortcutBusyConfirmsWithSameShortcut()
    {
        GestureMachine m = make();
        m.press(kWsA);
        m.release(kWsA);
        CMDS(m.openResult(QStringLiteral("a"), true), "ShowConfirmOsd(a) StartTimer(2000)");
        // The main shortcut is not the confirming shortcut here: it starts a new gesture.
        QCOMPARE(m.state(), GestureMachine::State::AwaitingConfirm);
        CMDS(m.press(kWsA), "HideConfirmOsd Open(a,force)");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
        CMDS(m.release(kWsA), "");
    }

    void otherWorkspaceShortcutDoesNotConfirm()
    {
        GestureMachine m = make();
        m.press(kWsA);
        m.release(kWsA);
        m.openResult(QStringLiteral("a"), true);
        const Shortcut b = Shortcut::workspace(QStringLiteral("b"));
        CMDS(m.press(b), "HideConfirmOsd");
        CMDS(m.release(b), "Open(b,plain)");
    }

    void mainPressDoesNotConfirmWorkspaceShortcut()
    {
        GestureMachine m = make();
        m.press(kWsA);
        m.release(kWsA);
        m.openResult(QStringLiteral("a"), true);
        CMDS(m.press(kMain), "HideConfirmOsd StartTimer(250)");
        CMDS(m.release(kMain), "Open(def,plain)");
    }

    void tapConfirmPath()
    {
        GestureMachine m = make();
        int tok = 0;
        tapBusy(m, tok);
        CMDS(m.press(kMain), "HideConfirmOsd Open(def,force)");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
        // The matching release is swallowed.
        CMDS(m.release(kMain), "");
        CMDS(m.timerFired(1), "");
    }

    void menuConfirmPath()
    {
        GestureMachine m = make();
        int tok = 0;
        menuPickBusy(m, tok);
        CMDS(m.modifiersHeld(true), "");
        CMDS(m.press(kMain), "HideConfirmOsd Open(x,force)");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
        CMDS(m.release(kMain), "");
    }

    void tapConfirmCancelledByModifierRelease()
    {
        GestureMachine m = make();
        int t = 0;
        tapBusy(m, t);
        CMDS(m.modifiersHeld(false), "HideConfirmOsd");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
        // The cancelled confirm timer is stale and a new press is an ordinary tap.
        CMDS(m.timerFired(t), "");
        CMDS(m.press(kMain), "StartTimer(250)");
        CMDS(m.release(kMain), "Open(def,plain)");
    }

    void menuConfirmCancelledByModifierRelease()
    {
        GestureMachine m = make();
        int t = 0;
        menuPickBusy(m, t);
        CMDS(m.modifiersHeld(false), "HideConfirmOsd");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
        CMDS(m.timerFired(t), "");
        CMDS(m.press(kMain), "StartTimer(250)");
    }

    void busyWithModifiersUpCancelsWithoutToast()
    {
        GestureMachine m = make();
        m.press(kMain);
        m.release(kMain);
        CMDS(m.modifiersHeld(false), "");
        CMDS(m.openResult(kDefault, true), "");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
    }

    void busyMenuPickWithModifiersUpCancelsWithoutToast()
    {
        GestureMachine m = make();
        m.timerFired(tokenOf(m.press(kMain)));
        m.release(kMain);
        m.menuResult(QStringLiteral("x"));
        m.modifiersHeld(false);
        CMDS(m.openResult(QStringLiteral("x"), true), "");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
    }

    void workspaceBusyWithModifiersUpStillConfirms()
    {
        GestureMachine m = make();
        m.modifiersHeld(false);
        m.press(kWsA);
        m.release(kWsA);
        CMDS(m.openResult(QStringLiteral("a"), true), "ShowConfirmOsd(a) StartTimer(2000)");
    }

    void modifiersBackUpAllowsConfirmAgain()
    {
        GestureMachine m = make();
        m.modifiersHeld(false);
        m.modifiersHeld(true);
        int t = 0;
        tapBusy(m, t);
    }

    void hideConfirmOsdOnPressOfAnotherShortcut()
    {
        GestureMachine m = make();
        int t = 0;
        tapBusy(m, t);
        CMDS(m.press(kWsA), "HideConfirmOsd");
        QCOMPARE(m.state(), GestureMachine::State::WorkspacePressed);
        // Main press during a workspace confirm: hide, then a fresh hold timer.
        m.release(kWsA);
        m.openResult(QStringLiteral("a"), true);
        CMDS(m.press(kMain), "HideConfirmOsd StartTimer(250)");
    }

    void tapConfirmCancelledByTimeout()
    {
        GestureMachine m = make();
        int t = 0;
        tapBusy(m, t);
        CMDS(m.timerFired(t), "HideConfirmOsd");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
        CMDS(m.press(kMain), "StartTimer(250)");
    }

    void menuConfirmCancelledByTimeout()
    {
        GestureMachine m = make();
        int t = 0;
        menuPickBusy(m, t);
        CMDS(m.timerFired(t), "HideConfirmOsd");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
        // A press after the timeout is a fresh gesture, not a confirm.
        CMDS(m.press(kMain), "StartTimer(250)");
    }

    void modifiersHeldTrueIsHarmless()
    {
        GestureMachine m = make();
        CMDS(m.modifiersHeld(true), "");
        CMDS(m.modifiersHeld(false), "");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
    }

    void releaseWithoutPressIgnored()
    {
        GestureMachine m = make();
        CMDS(m.release(kMain), "");
        CMDS(m.release(kWsA), "");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
    }

    void releaseOfOtherShortcutIgnored()
    {
        GestureMachine m = make();
        m.press(kMain);
        CMDS(m.release(kWsA), "");
        QCOMPARE(m.state(), GestureMachine::State::Pressed);
        CMDS(m.release(kMain), "Open(def,plain)");
    }

    void doublePressRestartsFromPressed()
    {
        GestureMachine m = make();
        const int t1 = tokenOf(m.press(kMain));
        const Commands c = m.press(kMain);
        CMDS(c, "StartTimer(250)");
        const int t2 = tokenOf(c);
        QVERIFY(t1 != t2);
        // The first hold timer is stale now.
        CMDS(m.timerFired(t1), "");
        CMDS(m.timerFired(t2), "ShowMenu");
    }

    void doublePressInMenuHidesMenuAndRestarts()
    {
        GestureMachine m = make();
        m.timerFired(tokenOf(m.press(kMain)));
        CMDS(m.press(kMain), "HideMenu StartTimer(250)");
        QCOMPARE(m.state(), GestureMachine::State::Pressed);
        CMDS(m.release(kMain), "Open(def,plain)");
    }

    void pressWhileAwaitingMenuResultRestarts()
    {
        GestureMachine m = make();
        m.timerFired(tokenOf(m.press(kMain)));
        m.release(kMain);
        CMDS(m.press(kMain), "StartTimer(250)");
        // The late menu result is ignored.
        CMDS(m.menuResult(QStringLiteral("x")), "");
    }

    void staleHoldTimerAfterTapIgnored()
    {
        GestureMachine m = make();
        const int t = tokenOf(m.press(kMain));
        m.release(kMain);
        CMDS(m.timerFired(t), "");
        QCOMPARE(m.state(), GestureMachine::State::AwaitingOpenResult);
    }

    void staleHoldTimerFromEarlierGestureIgnored()
    {
        GestureMachine m = make();
        const int t1 = tokenOf(m.press(kMain));
        m.release(kMain);
        const int t2 = tokenOf(m.press(kMain));
        CMDS(m.timerFired(t1), "");
        QCOMPARE(m.state(), GestureMachine::State::Pressed);
        CMDS(m.timerFired(t2), "ShowMenu");
    }

    void staleConfirmTimerIgnoredAfterConfirm()
    {
        GestureMachine m = make();
        int t = 0;
        tapBusy(m, t);
        m.press(kMain);
        m.release(kMain);
        const int t2 = tokenOf(m.press(kMain));
        CMDS(m.timerFired(t), "");
        QCOMPARE(m.state(), GestureMachine::State::Pressed);
        CMDS(m.timerFired(t2), "ShowMenu");
    }

    void staleConfirmTimerDoesNotCancelNewConfirm()
    {
        GestureMachine m = make();
        int t1 = 0;
        tapBusy(m, t1);
        m.modifiersHeld(false);
        m.modifiersHeld(true);
        int t2 = 0;
        tapBusy(m, t2);
        CMDS(m.timerFired(t1), "");
        QCOMPARE(m.state(), GestureMachine::State::AwaitingConfirm);
        CMDS(m.timerFired(t2), "HideConfirmOsd");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
    }

    void openResultStaleAfterNewPressIgnored()
    {
        GestureMachine m = make();
        m.press(kMain);
        CMDS(m.release(kMain), "Open(def,plain)");
        const Commands c = m.press(kMain);
        CMDS(c, "StartTimer(250)");
        CMDS(m.openResult(kDefault, true), "");
        QCOMPARE(m.state(), GestureMachine::State::Pressed);
        CMDS(m.timerFired(tokenOf(c)), "ShowMenu");
    }

    void openResultStaleAfterNewPressMenuPick()
    {
        GestureMachine m = make();
        m.timerFired(tokenOf(m.press(kMain)));
        m.release(kMain);
        CMDS(m.menuResult(QStringLiteral("x")), "HideMenu Open(x,plain)");
        const Commands c = m.press(kMain);
        CMDS(m.openResult(QStringLiteral("x"), true), "");
        CMDS(m.release(kMain), "Open(def,plain)");
        Q_UNUSED(c);
    }

    void openResultForOtherIdIgnored()
    {
        GestureMachine m = make();
        m.press(kMain);
        m.release(kMain);
        CMDS(m.openResult(QStringLiteral("zzz"), true), "");
        QCOMPARE(m.state(), GestureMachine::State::AwaitingOpenResult);
        CMDS(m.openResult(kDefault, false), "");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
        CMDS(m.openResult(kDefault, true), "");
    }

    void workspaceConfirmNotCancelledByModifiers()
    {
        GestureMachine m = make();
        m.press(kWsA);
        m.release(kWsA);
        m.openResult(QStringLiteral("a"), true);
        CMDS(m.modifiersHeld(false), "");
        QCOMPARE(m.state(), GestureMachine::State::AwaitingConfirm);
        CMDS(m.press(kWsA), "HideConfirmOsd Open(a,force)");
    }

    void modifiersIgnoredInPressedAndMenu()
    {
        GestureMachine m = make();
        const int t = tokenOf(m.press(kMain));
        CMDS(m.modifiersHeld(false), "");
        QCOMPARE(m.state(), GestureMachine::State::Pressed);
        CMDS(m.timerFired(t), "ShowMenu");
        CMDS(m.modifiersHeld(false), "");
        QCOMPARE(m.state(), GestureMachine::State::Menu);
        CMDS(m.release(kMain), "");
        CMDS(m.menuResult(QStringLiteral("x")), "HideMenu Open(x,plain)");
    }

    void openResultNotBusyIgnoredWhenIdle()
    {
        GestureMachine m = make();
        CMDS(m.openResult(kDefault, false), "");
        QCOMPARE(m.state(), GestureMachine::State::Idle);
    }
};

QTEST_APPLESS_MAIN(GestureMachineTest)

#include "tst_gesturemachine.moc"
