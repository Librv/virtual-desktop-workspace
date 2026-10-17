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
    void press(const vdw::Shortcut &shortcut);
    void release(const vdw::Shortcut &shortcut);
    void setModifiersHeld(bool held);

private:
    void execute(const vdw::Commands &commands);
    void runCommand(const vdw::Command &command);
    void open(const QString &id, bool force);
    void showMenu();
    void hideMenu();
    void showConfirmOsd(const QString &id);
    void showNoDefaultOsd();
    void showOsd(const QString &icon, const QString &text);
    void refreshConfig();

    vdw::GestureMachine m_machine;
    Manager *m_manager;
    Shortcuts *m_shortcuts;
    KWinScripts *m_scripts;
    RadialMenu *m_menu;
    /// Bumped by every ShowMenu and HideMenu so that a slow cursor query
    /// never shows a menu the user has already released.
    int m_menuGeneration = 0;
    vdw::Shortcut m_lastPressed;
    /// The shortcut whose press confirms, by workspace id (set when Open(id, false) runs).
    QHash<QString, vdw::Shortcut> m_confirmShortcuts;
};

/// The /Debug names of a shortcut: "main" or a workspace id.
vdw::Shortcut shortcutFromName(const QString &name);

}
