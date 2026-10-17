// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "gesturecontroller.h"

#include "kwinscripts.h"
#include "manager.h"
#include "shortcutkeys.h"
#include "shortcuts.h"
#include "uiclient.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QTimer>

Q_DECLARE_LOGGING_CATEGORY(VDW_DAEMON)

namespace Vdw
{

namespace
{
int modifierMask(const QKeySequence &sequence)
{
    int mask = 0;
    for (const Qt::Key key : ShortcutKeys::modifierKeys(sequence)) {
        switch (key) {
        case Qt::Key_Control:
            mask |= Qt::ControlModifier;
            break;
        case Qt::Key_Meta:
            mask |= Qt::MetaModifier;
            break;
        case Qt::Key_Shift:
            mask |= Qt::ShiftModifier;
            break;
        default:
            mask |= Qt::AltModifier;
            break;
        }
    }
    return mask;
}
}

Vdw::Shortcut shortcutFromName(const QString &name)
{
    return name == QLatin1String("main") ? Vdw::Shortcut::main() : Vdw::Shortcut::workspace(name);
}

GestureController::GestureController(Manager *manager, Shortcuts *shortcuts, KWinScripts *scripts, UiClient *ui, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
    , m_shortcuts(shortcuts)
    , m_scripts(scripts)
    , m_ui(ui)
    , m_toastTimer(new QTimer(this))
{
    m_toastTimer->setSingleShot(true);
    m_toastTimer->setInterval(3000);
    connect(m_toastTimer, &QTimer::timeout, this, &GestureController::hideToast);
    refreshConfig();
    connect(m_manager, &Manager::configReloaded, this, &GestureController::refreshConfig);
    connect(m_shortcuts, &Shortcuts::pressed, this, [this](const Vdw::Shortcut &shortcut) {
        m_debugModifiers = false; // a real key press: the helper is the truth again
        press(shortcut);
    });
    connect(m_shortcuts, &Shortcuts::released, this, &GestureController::release);
    connect(m_ui, &UiClient::modifiersHeldChanged, this, [this](bool held) {
        if (!m_debugModifiers) {
            setModifiersHeld(held);
        }
    });
}

void GestureController::refreshConfig()
{
    const Config &config = m_manager->config();
    Vdw::GestureConfig gesture;
    gesture.defaultWorkspaceId = config.defaultWorkspace();
    gesture.holdThresholdMs = config.holdThresholdMs();
    gesture.confirmTimeoutMs = config.confirmTimeoutMs();
    m_machine.setConfig(gesture);
}

void GestureController::press(const Vdw::Shortcut &shortcut)
{
    if (shortcut.isMain) {
        // Up before the hold threshold, and tracking modifiers for the whole gesture.
        m_ui->trackModifiers(modifierMask(m_shortcuts->sequence(shortcut)));
    }
    execute(m_machine.press(shortcut));
}

void GestureController::release(const Vdw::Shortcut &shortcut)
{
    // Asked before the commands run: they may hide the menu, and the helper
    // handles lines in order.
    const int sequence = ++m_releaseSequence;
    m_ui->select([this, sequence](const QString &selected) {
        // Another release or a state change meanwhile: this answer is stale.
        if (sequence == m_releaseSequence && m_machine.state() == Vdw::GestureMachine::State::AwaitingMenuResult) {
            // Empty (dead zone, or the menu is not up yet) cancels.
            execute(m_machine.menuResult(selected.isEmpty() ? std::nullopt : std::optional<QString>(selected)));
        }
    });
    execute(m_machine.release(shortcut));
}

void GestureController::debugSetModifiersHeld(bool held)
{
    m_debugModifiers = true;
    setModifiersHeld(held);
}

void GestureController::setModifiersHeld(bool held)
{
    execute(m_machine.modifiersHeld(held));
}

void GestureController::execute(const Vdw::Commands &commands)
{
    for (const Vdw::Command &command : commands) {
        runCommand(command);
    }
    m_ui->setGestureActive(m_machine.state() != Vdw::GestureMachine::State::Idle);
}

void GestureController::runCommand(const Vdw::Command &command)
{
    using Kind = Vdw::Command::Kind;
    switch (command.kind) {
    case Kind::StartTimer:
        QTimer::singleShot(command.ms, this, [this, token = command.token] {
            execute(m_machine.timerFired(token));
        });
        break;
    case Kind::Open:
        open(command.workspaceId, command.force);
        break;
    case Kind::ShowMenu:
        showMenu();
        break;
    case Kind::HideMenu:
        hideMenu();
        break;
    case Kind::ShowConfirmOsd:
        showConfirmToast(command.workspaceId);
        break;
    case Kind::HideConfirmOsd:
        hideToast();
        break;
    }
}

void GestureController::open(const QString &id, bool force)
{
    qCInfo(VDW_DAEMON) << "gesture: open" << id << "force" << force;
    // A tap with no default (the loader clears an unknown DefaultWorkspace).
    if (!force && id.isEmpty()) {
        showNoDefaultToast();
    }
    m_manager->open(id, force, [this, id](const QString &result) {
        if (result.startsWith(QLatin1String("error"))) {
            qCWarning(VDW_DAEMON) << "gesture: open" << id << "failed:" << result;
        }
        execute(m_machine.openResult(id, result == QLatin1String("busy")));
    });
}

void GestureController::showMenu()
{
    const int generation = ++m_menuGeneration;
    m_scripts->queryCursor([this, generation](const QJsonObject &cursor, const QString &error) {
        if (generation != m_menuGeneration) {
            return; // released (or hidden) while KWin was answering
        }
        QPoint center(-1, -1); // the helper centers it on the screen
        if (error.isEmpty()) {
            center = QPoint(cursor.value(QLatin1String("x")).toInt(), cursor.value(QLatin1String("y")).toInt());
        } else {
            qCWarning(VDW_DAEMON) << "gesture: no cursor position:" << error;
        }
        QJsonArray items;
        for (const Workspace &workspace : m_manager->config().workspaces()) {
            items.append(QJsonObject{{QStringLiteral("id"), workspace.id},
                                     {QStringLiteral("name"), workspace.name},
                                     {QStringLiteral("icon"), workspace.icon.isEmpty() ? QStringLiteral("virtual-desktops") : workspace.icon}});
        }
        qCInfo(VDW_DAEMON) << "gesture: show menu at" << center << "with" << items.size() << "workspaces";
        m_ui->showMenu(center, items);
    });
}

void GestureController::hideMenu()
{
    ++m_menuGeneration;
    m_ui->hideMenu();
}

void GestureController::showConfirmToast(const QString &id)
{
    const Workspace *workspace = m_manager->config().find(id);
    const QString name = workspace ? workspace->name : id;
    const QString icon = workspace && !workspace->icon.isEmpty() ? workspace->icon : QStringLiteral("virtual-desktops");

    const Vdw::Shortcut confirmShortcut = m_machine.confirmShortcut();
    QKeySequence sequence = m_shortcuts->sequence(confirmShortcut);
    if (sequence.isEmpty()) {
        sequence = m_shortcuts->sequence(Vdw::Shortcut::main());
    }
    // Only main-shortcut confirms require the modifiers to stay held.
    const QString mods = confirmShortcut.isMain ? ShortcutKeys::modifierText(sequence) : QString();
    QString key = ShortcutKeys::keyText(sequence);
    if (key.isEmpty()) {
        key = QStringLiteral("the shortcut");
    }
    const QString text = mods.isEmpty() ? QStringLiteral("Desktop not empty. Press %1 again to set up %2 here.").arg(key, name)
                                        : QStringLiteral("Desktop not empty. Keep holding %1 and press %2 to set up %3 here.").arg(mods, key, name);
    showToast(icon, text);
}

void GestureController::showNoDefaultToast()
{
    showToast(QStringLiteral("preferences-desktop-virtual"),
              QStringLiteral("No default workspace. Hold the shortcut for the menu, or pick one in System Settings."));
    m_toastTimer->start();
}

void GestureController::showToast(const QString &icon, const QString &text)
{
    qCInfo(VDW_DAEMON) << "gesture: toast" << text;
    m_toastTimer->stop();
    m_ui->showToast(icon, text);
}

void GestureController::hideToast()
{
    m_toastTimer->stop();
    m_ui->hideToast();
}

}
