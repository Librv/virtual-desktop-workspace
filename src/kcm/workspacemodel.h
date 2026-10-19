// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include "browserprofile.h"
#include "workspace.h"

#include <QAbstractListModel>
#include <QList>
#include <QSet>
#include <QString>

namespace Vdw
{

/// One entry of the profile picker of a browser slot.
struct ProfileChoice {
    QString dir; ///< profile directory name, what vdwrc stores
    QString name; ///< display name
    bool missing = false; ///< `dir` is saved in the slot but the browser has no such profile
};

/// The picker entries for `profiles`, sorted by display name (numbers in names
/// compare as numbers, ties by dir). When `saved` is not empty and not in
/// `profiles`, one entry for it is added at the end with `missing` set.
QList<ProfileChoice> profileChoices(const QList<BrowserProfile> &profiles, const QString &saved);

/// The editable list of workspaces behind the KCM. Pure data editing: no
/// file access, no D-Bus. Slot editing goes through SlotModel.
class WorkspaceModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString defaultWorkspace READ defaultWorkspace NOTIFY defaultWorkspaceChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        IconRole,
        IsDefaultRole,
        SlotCountRole,
    };

    explicit WorkspaceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Replace everything (e.g. after loading the config). Does not emit changed().
    void setWorkspaces(const QList<Workspace> &workspaces, const QString &defaultWorkspace);
    const QList<Workspace> &workspaces() const
    {
        return m_workspaces;
    }
    QString defaultWorkspace() const
    {
        return m_default;
    }

    /// Append every workspace of `workspaces` (read from disk) that appeared
    /// there since the last load or save: existing ones (and their unsaved
    /// edits) stay untouched, deleted ones stay deleted. Returns how many.
    int mergeNew(const QList<Workspace> &workspaces);
    /// The model was just written to disk: its ids are what disk holds now.
    void markSaved();
    /// Add a workspace named `name` with a fresh unique slug id; returns its row.
    Q_INVOKABLE int addWorkspace(const QString &name);
    Q_INVOKABLE void removeWorkspace(int row);
    /// Move `row` by `delta` positions (clamped); returns the new row.
    Q_INVOKABLE int moveWorkspace(int row, int delta);
    Q_INVOKABLE void setName(int row, const QString &name);
    Q_INVOKABLE void setIcon(int row, const QString &icon);
    /// Make `row` the default workspace; -1 clears the default.
    Q_INVOKABLE void setDefaultRow(int row);

    int slotCount(int row) const;
    const Slot *slotAt(int row, int index) const;
    Q_INVOKABLE void addSlot(int row);
    Q_INVOKABLE void removeSlot(int row, int index);
    /// Move the slot at `from` to position `to` (the last slot is on top when windows overlap).
    /// Invalid indices and from == to do nothing.
    Q_INVOKABLE void moveSlot(int row, int from, int to);
    /// Edit one slot in place. Emits changed() but no slotsReset().
    bool setSlotAt(int row, int index, const Slot &slot);

Q_SIGNALS:
    /// Any edit; the KCM marks itself as needing a save.
    void changed();
    /// Slots were added, removed or reordered in `row` (not for edits in place).
    void slotsReset(int row);
    void defaultWorkspaceChanged();

private:
    bool validRow(int row) const
    {
        return row >= 0 && row < m_workspaces.size();
    }

    QList<Workspace> m_workspaces;
    QString m_default;
    QSet<QString> m_onDisk; ///< ids on disk at the last load, save or merge
};

/// The slots of one workspace of a WorkspaceModel, edited in place so that a
/// ListView keeps its delegates (and focus) while the user types.
class SlotModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int workspaceRow READ workspaceRow WRITE setWorkspaceRow NOTIFY workspaceRowChanged)

public:
    enum Role {
        AppRole = Qt::UserRole + 1,
        ArgsRole,
        ProfileRole,
        XRole,
        YRole,
        WidthRole,
        HeightRole,
    };

    explicit SlotModel(WorkspaceModel *source);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role) override;
    QHash<int, QByteArray> roleNames() const override;

    int workspaceRow() const
    {
        return m_row;
    }
    void setWorkspaceRow(int row);

    /// Set one field by role name from QML ("app", "args", "x", ...).
    Q_INVOKABLE bool setField(int index, const QString &field, const QVariant &value);

Q_SIGNALS:
    void workspaceRowChanged();

private:
    WorkspaceModel *m_source;
    int m_row = -1;
};

} // namespace Vdw
