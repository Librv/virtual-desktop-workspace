// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include "config.h"
#include "gesturemachine.h"

#include <QHash>
#include <QKeySequence>
#include <QObject>

class QAction;

namespace Vdw
{

/// The global shortcuts of component `vdw` in kglobalaccel: the main shortcut
/// and one per workspace. Reports presses and releases (from
/// globalShortcutActiveChanged only, S1).
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

Q_SIGNALS:
    void pressed(const Vdw::Shortcut &shortcut);
    void released(const Vdw::Shortcut &shortcut);

private:
    QAction *registerAction(const QString &name, const QString &text, const QKeySequence &defaultKey);
    void removeStaleActions();
    void renameAction(const QString &name, const QString &text);

    QAction *m_main = nullptr;
    QHash<QString, QAction *> m_workspaces; ///< by workspace id
};

}
