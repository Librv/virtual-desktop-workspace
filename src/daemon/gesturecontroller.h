// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include "gesturemachine.h"

#include <QHash>
#include <QObject>

class RadialMenu;

namespace Vdw
{

class KWinScripts;
class Manager;
class Shortcuts;

/// Owns the GestureMachine and carries out its commands: timers, opening
/// workspaces through the Manager, the radial menu and the confirm OSD.
class GestureController : public QObject
{
    Q_OBJECT

public:
    GestureController(Manager *manager, Shortcuts *shortcuts, KWinScripts *scripts, RadialMenu *menu, QObject *parent = nullptr);

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
    void showConfirmOsd(const QString &id);
    void showNoDefaultOsd();
    void showOsd(const QString &icon, const QString &text);
    void refreshConfig();

    Vdw::GestureMachine m_machine;
    Manager *m_manager;
    Shortcuts *m_shortcuts;
    KWinScripts *m_scripts;
    RadialMenu *m_menu;
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
