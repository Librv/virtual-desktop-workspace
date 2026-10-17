// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include "gesturemachine.h"

#include <QObject>

class QTimer;

namespace Vdw
{

class KWinScripts;
class Manager;
class Shortcuts;
class UiClient;

/// Owns the GestureMachine and carries out its commands: timers, opening
/// workspaces through the Manager, the radial menu and the toast.
class GestureController : public QObject
{
    Q_OBJECT

public:
    GestureController(Manager *manager, Shortcuts *shortcuts, KWinScripts *scripts, UiClient *ui, QObject *parent = nullptr);

    /// The three inputs; real shortcuts and the /Debug object use the same ones.
    void press(const Vdw::Shortcut &shortcut);
    void release(const Vdw::Shortcut &shortcut);
    void setModifiersHeld(bool held);
    /// The /Debug override: the helper's modifier answers are ignored until the next real shortcut press.
    void debugSetModifiersHeld(bool held);

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
    UiClient *m_ui;
    /// Hides the no-default-workspace hint; stopped by every other toast change.
    QTimer *m_toastTimer;
    /// Bumped by every ShowMenu and HideMenu so that a slow cursor query
    /// never shows a menu the user has already released.
    int m_menuGeneration = 0;
    /// Bumped by every release; a `selected` answer for an older one is dropped.
    int m_releaseSequence = 0;
    bool m_debugModifiers = false;
};

/// The /Debug names of a shortcut: "main" or a workspace id.
Vdw::Shortcut shortcutFromName(const QString &name);

}
