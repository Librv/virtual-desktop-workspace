#pragma once

#include "config.h"
#include "gesturemachine.h"

#include <QHash>
#include <QKeySequence>
#include <QObject>

class QAction;
class KModifierKeyInfo;

namespace Vdw
{

/// The global shortcuts of component `vdw` in kglobalaccel: the main shortcut
/// and one per workspace. Reports presses and releases (from
/// globalShortcutActiveChanged only, S1) and whether every modifier of the
/// main shortcut is held.
class Shortcuts : public QObject
{
    Q_OBJECT

public:
    explicit Shortcuts(QObject *parent = nullptr);

    /// Register the main action and one action per workspace of `config`,
    /// update names, and drop the global shortcut of workspaces that are gone.
    void sync(const Config &config);

    /// The current key sequence of `shortcut` (empty if none is set).
    QKeySequence sequence(const Vdw::Shortcut &shortcut) const;

    bool modifiersHeld() const
    {
        return m_modifiersHeld;
    }

Q_SIGNALS:
    void pressed(const Vdw::Shortcut &shortcut);
    void released(const Vdw::Shortcut &shortcut);
    void modifiersHeldChanged(bool held);

private:
    QAction *registerAction(const QString &name, const QString &text, const QKeySequence &defaultKey);
    void removeStaleActions();
    void renameAction(const QString &name, const QString &text);
    void updateModifiers();

    QAction *m_main = nullptr;
    QHash<QString, QAction *> m_workspaces; ///< by workspace id
    KModifierKeyInfo *m_modifierInfo = nullptr;
    bool m_modifiersHeld = false;
};

}
