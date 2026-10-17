// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include "gesturemachine.h"

#include <QHash>
#include <QObject>

class RadialMenu;
class Toast;
class QTimer;

namespace Vdw
{

class KWinScripts;
class Manager;
class Shortcuts;

/// Owns the GestureMachine and carries out its commands: timers, opening
/// workspaces through the Manager, the radial menu and the toast.
class GestureController : public QObject
{
    Q_OBJECT

public:
    GestureController(Manager *manager, Shortcuts *shortcuts, KWinScripts *scripts, RadialMenu *menu, Toast *toast, QObject *parent = nullptr);

    /// The three inputs; real shortcuts and the /Debug object use the same ones.
    void press(const Vdw::Shortcut &shortcut);
    void release(const Vdw::Shortcut &shortcut);
    void setModifiersHeld(bool held);

private:
    void execute(const Vdw::Commands &commands);
    void runCommand(const Vdw::Command &command);
    void open(const QString &id, bool force);
    void showMenu();
    void hideMenu();
    void showConfirmToast(const QString &id);
    void showNoDefaultToast();
    void showToast(const QString &icon, const QString &text);
    void hideToast();
    void refreshConfig();

    Vdw::GestureMachine m_machine;
    Manager *m_manager;
    Shortcuts *m_shortcuts;
    KWinScripts *m_scripts;
    RadialMenu *m_menu;
    Toast *m_toast;
    /// Hides the no-default-workspace hint; stopped by every other toast change.
    QTimer *m_toastTimer;
    /// Bumped by every ShowMenu and HideMenu so that a slow cursor query
    /// never shows a menu the user has already released.
    int m_menuGeneration = 0;
    Vdw::Shortcut m_lastPressed;
    /// The shortcut whose press confirms, by workspace id (set when Open(id, false) runs).
    QHash<QString, Vdw::Shortcut> m_confirmShortcuts;
};

/// The /Debug names of a shortcut: "main" or a workspace id.
Vdw::Shortcut shortcutFromName(const QString &name);

}
