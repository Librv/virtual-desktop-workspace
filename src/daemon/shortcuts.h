// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include "config.h"
#include "gesturemachine.h"

#include <QDBusMessage>
#include <QHash>
#include <QObject>

class QDBusServiceWatcher;

namespace Vdw
{

/// The global shortcuts of component `vdw` in kglobalaccel: the main shortcut
/// and one per workspace, talked to over raw D-Bus (the KGlobalAccel client
/// library links QtGui, S5). Reports presses and releases.
class Shortcuts : public QObject
{
    Q_OBJECT

public:
    explicit Shortcuts(QObject *parent = nullptr);

    /// Register the main action and one action per workspace of `config`,
    /// update names, and drop the global shortcut of workspaces that are gone.
    void sync(const Config &config);

    /// The first chord (Qt key|modifier int) of the current key sequence of
    /// `shortcut`, 0 if none is set. Read from a cache that follows kglobalaccel.
    int chord(const Vdw::Shortcut &shortcut) const;

Q_SIGNALS:
    void pressed(const Vdw::Shortcut &shortcut);
    void released(const Vdw::Shortcut &shortcut);

private Q_SLOTS:
    void onShortcutPressed(const QString &component, const QString &action, qlonglong timestamp);
    void onShortcutReleased(const QString &component, const QString &action, qlonglong timestamp);
    void onKeysChanged(const QDBusMessage &message);

private:
    struct Action {
        QString text;
        int defaultKey = 0;
    };

    void registerAction(const QString &name, const Action &action);
    void renameAction(const QString &name, const QString &text);
    void removeStaleActions();
    void emitFor(const QString &component, const QString &name, bool pressed);

    QHash<QString, Action> m_actions; ///< by action name ("main", "open-<id>")
    QHash<QString, int> m_chords; ///< first chord of each action's keys, as kglobalaccel last said
    QDBusServiceWatcher *m_watcher = nullptr;
};

}
