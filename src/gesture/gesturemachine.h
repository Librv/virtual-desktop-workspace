// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include <QList>
#include <QString>

#include <optional>

namespace Vdw
{

/// What a global shortcut is: the main shortcut, or one workspace's own shortcut.
struct Shortcut {
    static Shortcut main()
    {
        return {true, {}};
    }
    static Shortcut workspace(const QString &id)
    {
        return {false, id};
    }
    bool operator==(const Shortcut &) const = default;

    bool isMain = true;
    QString workspaceId;
};

/// One thing the daemon has to do in response to an input.
struct Command {
    enum class Kind {
        StartTimer, ///< Call timerFired(token) after ms milliseconds.
        ShowMenu,
        HideMenu,
        Open, ///< Open workspaceId; force skips the busy check.
        ShowConfirmOsd, ///< Tell the user to press the shortcut again to confirm workspaceId.
        HideConfirmOsd, ///< An AwaitingConfirm state ended (confirm, cancel, timeout or a new gesture).
    };

    static Command startTimer(int token, int ms)
    {
        return {Kind::StartTimer, token, ms, {}, false};
    }
    static Command showMenu()
    {
        return {Kind::ShowMenu, 0, 0, {}, false};
    }
    static Command hideMenu()
    {
        return {Kind::HideMenu, 0, 0, {}, false};
    }
    static Command open(const QString &id, bool force)
    {
        return {Kind::Open, 0, 0, id, force};
    }
    static Command showConfirmOsd(const QString &id)
    {
        return {Kind::ShowConfirmOsd, 0, 0, id, false};
    }
    static Command hideConfirmOsd()
    {
        return {Kind::HideConfirmOsd, 0, 0, {}, false};
    }
    bool operator==(const Command &) const = default;

    Kind kind = Kind::HideMenu;
    int token = 0;
    int ms = 0;
    QString workspaceId;
    bool force = false;
};

using Commands = QList<Command>;

struct GestureConfig {
    QString defaultWorkspaceId;
    int holdThresholdMs = 250;
    int confirmTimeoutMs = 2000;
};

/// Pure gesture state machine for the main shortcut and the per-workspace
/// shortcuts. It owns no timers: the caller runs the StartTimer commands and
/// feeds the token back through timerFired().
class GestureMachine
{
public:
    GestureMachine() = default;
    explicit GestureMachine(const GestureConfig &config);

    void setConfig(const GestureConfig &config);

    Commands press(const Shortcut &shortcut);
    Commands release(const Shortcut &shortcut);
    /// True while all modifiers of the main shortcut are held (assumed until
    /// told otherwise). Releasing them cancels a pending confirm that came from
    /// the main shortcut (a tap or a menu pick); confirms from a workspace
    /// shortcut end by timeout (or by a press of another shortcut). A busy
    /// openResult() for the main shortcut while they are up cancels without a
    /// confirm. No other effect.
    Commands modifiersHeld(bool held);
    Commands timerFired(int token);
    /// Selection at release time; empty for the dead zone.
    Commands menuResult(const std::optional<QString> &workspaceId);
    /// Answer to an Open(id, false) command.
    /// A busy answer starts the confirm (ShowConfirmOsd) unless the open came
    /// from the main shortcut and its modifiers are already up: then it just ends.
    Commands openResult(const QString &workspaceId, bool busy);

    enum class State {
        Idle,
        Pressed, ///< Main shortcut down, hold timer running.
        WorkspacePressed, ///< Workspace shortcut down.
        Menu, ///< Menu shown, shortcut still down.
        AwaitingMenuResult, ///< Released over the menu, waiting for menuResult().
        AwaitingOpenResult, ///< Open(id, false) emitted, waiting for openResult().
        AwaitingConfirm,
    };
    State state() const
    {
        return m_state;
    }
    /// The shortcut of the current gesture (valid unless Idle).
    Shortcut pressedShortcut() const
    {
        return m_pressed;
    }
    /// The shortcut whose press confirms the last Open(id, false).
    Shortcut confirmShortcut() const
    {
        return m_confirmShortcut;
    }

private:
    void startTimer(Commands &out, int ms);
    void invalidateTimer();
    Commands beginOpen(const QString &id, const Shortcut &confirmShortcut);
    Commands beginPress(const Shortcut &shortcut);

    GestureConfig m_config;
    State m_state = State::Idle;
    Shortcut m_pressed;
    int m_generation = 0;

    /// Set when Open(id, false) is emitted, used by openResult().
    QString m_confirmId;
    Shortcut m_confirmShortcut; ///< The shortcut whose press confirms.
    bool m_modifiersHeld = true;
};

} // namespace Vdw
