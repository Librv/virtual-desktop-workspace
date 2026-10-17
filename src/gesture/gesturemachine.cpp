// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "gesturemachine.h"

namespace Vdw
{

namespace
{
const Shortcut kMain = Shortcut::main();
}

GestureMachine::GestureMachine(const GestureConfig &config)
    : m_config(config)
{
}

void GestureMachine::setConfig(const GestureConfig &config)
{
    m_config = config;
}

void GestureMachine::startTimer(Commands &out, int ms)
{
    ++m_generation;
    out.append(Command::startTimer(m_generation, ms));
}

void GestureMachine::invalidateTimer()
{
    ++m_generation;
}

Commands GestureMachine::beginPress(const Shortcut &shortcut)
{
    Commands out;
    m_pressed = shortcut;
    if (shortcut.isMain) {
        m_state = State::Pressed;
        startTimer(out, m_config.holdThresholdMs);
    } else {
        m_state = State::WorkspacePressed;
        invalidateTimer();
    }
    return out;
}

Commands GestureMachine::beginOpen(const QString &id, const Shortcut &confirmShortcut)
{
    m_state = State::AwaitingOpenResult;
    m_confirmId = id;
    m_confirmShortcut = confirmShortcut;
    return {Command::open(id, false)};
}

Commands GestureMachine::press(const Shortcut &shortcut)
{
    switch (m_state) {
    case State::AwaitingConfirm: {
        if (shortcut == m_confirmShortcut) {
            // The matching release arrives in Idle and is ignored.
            const QString id = m_confirmId;
            m_state = State::Idle;
            invalidateTimer();
            return {Command::hideConfirmOsd(), Command::open(id, true)};
        }
        // Another shortcut: the confirm window is over, start fresh.
        Commands out{Command::hideConfirmOsd()};
        out += beginPress(shortcut);
        return out;
    }
    case State::Menu: {
        // A press without a release: restart, closing the menu.
        Commands out{Command::hideMenu()};
        out += beginPress(shortcut);
        return out;
    }
    case State::Idle:
    case State::Pressed:
    case State::WorkspacePressed:
    case State::AwaitingMenuResult:
    case State::AwaitingOpenResult:
        return beginPress(shortcut);
    }
    return {};
}

Commands GestureMachine::release(const Shortcut &shortcut)
{
    if (shortcut != m_pressed) {
        return {};
    }
    switch (m_state) {
    case State::Pressed:
        invalidateTimer();
        return beginOpen(m_config.defaultWorkspaceId, kMain);
    case State::WorkspacePressed:
        return beginOpen(shortcut.workspaceId, shortcut);
    case State::Menu:
        m_state = State::AwaitingMenuResult;
        return {};
    case State::Idle:
    case State::AwaitingMenuResult:
    case State::AwaitingOpenResult:
    case State::AwaitingConfirm:
        break;
    }
    return {};
}

Commands GestureMachine::modifiersHeld(bool held)
{
    m_modifiersHeld = held;
    if (!held && m_state == State::AwaitingConfirm && m_confirmShortcut.isMain) {
        m_state = State::Idle;
        invalidateTimer();
        return {Command::hideConfirmOsd()};
    }
    return {};
}

Commands GestureMachine::timerFired(int token)
{
    if (token != m_generation) {
        return {};
    }
    if (m_state == State::Pressed) {
        m_state = State::Menu;
        return {Command::showMenu()};
    }
    if (m_state == State::AwaitingConfirm) {
        m_state = State::Idle;
        return {Command::hideConfirmOsd()};
    }
    return {};
}

Commands GestureMachine::menuResult(const std::optional<QString> &workspaceId)
{
    if (m_state != State::AwaitingMenuResult) {
        return {};
    }
    Commands out{Command::hideMenu()};
    if (workspaceId) {
        out += beginOpen(*workspaceId, kMain);
    } else {
        m_state = State::Idle;
    }
    return out;
}

Commands GestureMachine::openResult(const QString &workspaceId, bool busy)
{
    if (m_state != State::AwaitingOpenResult || workspaceId != m_confirmId) {
        return {};
    }
    if (!busy) {
        m_state = State::Idle;
        return {};
    }
    if (m_confirmShortcut.isMain && !m_modifiersHeld) {
        m_state = State::Idle;
        return {};
    }
    Commands out;
    m_state = State::AwaitingConfirm;
    out.append(Command::showConfirmOsd(workspaceId));
    startTimer(out, m_config.confirmTimeoutMs);
    return out;
}

} // namespace Vdw
