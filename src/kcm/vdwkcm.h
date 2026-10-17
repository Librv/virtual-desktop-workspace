// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include "appmodel.h"
#include "workspacemodel.h"

#include <KQuickConfigModule>

/// System Settings page for the saved workspaces. Edits a copy in the
/// models; save() writes vdwrc and tells the daemon to reload.
class VdwKcm : public KQuickConfigModule
{
    Q_OBJECT
    Q_PROPERTY(Vdw::WorkspaceModel *workspaces READ workspaces CONSTANT)
    Q_PROPERTY(Vdw::SlotModel *slots READ slots CONSTANT)
    Q_PROPERTY(AppModel *apps READ apps CONSTANT)
    Q_PROPERTY(QStringList problems READ problems NOTIFY problemsChanged)
    Q_PROPERTY(QStringList notes READ notes NOTIFY notesChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)

public:
    VdwKcm(QObject *parent, const KPluginMetaData &data);

    Vdw::WorkspaceModel *workspaces() const
    {
        return m_workspaces;
    }
    Vdw::SlotModel *slots() const
    {
        return m_slots;
    }
    AppModel *apps() const
    {
        return m_apps;
    }
    QStringList problems() const
    {
        return m_problems;
    }
    /// Warnings of the last load (skipped entries), shown as information.
    QStringList notes() const
    {
        return m_notes;
    }
    QString statusText() const
    {
        return m_statusText;
    }

    /// {found, name, icon} of an app id, for the slot rows.
    Q_INVOKABLE QVariantMap appInfo(const QString &desktopId) const;
    /// Ask the daemon to save the windows of the current desktop as a new workspace.
    Q_INVOKABLE void captureCurrentDesktop(const QString &name);
    /// Open the Shortcuts page of System Settings.
    Q_INVOKABLE void openShortcuts();

    void load() override;
    void save() override;

Q_SIGNALS:
    void problemsChanged();
    void notesChanged();
    void statusTextChanged();

private:
    void edited();
    void updateProblems();
    void setStatusText(const QString &text);
    void reloadDaemon();

    Vdw::WorkspaceModel *m_workspaces;
    Vdw::SlotModel *m_slots;
    AppModel *m_apps;
    QStringList m_problems;
    QStringList m_notes;
    QString m_statusText;
};
