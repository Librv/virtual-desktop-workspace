// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "workspacemodel.h"

#include "configwriter.h"

#include <QSet>

#include <algorithm>

namespace Vdw
{

WorkspaceModel::WorkspaceModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int WorkspaceModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_workspaces.size();
}

QVariant WorkspaceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || !validRow(index.row())) {
        return {};
    }
    const Workspace &ws = m_workspaces.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return ws.name;
    case IdRole:
        return ws.id;
    case IconRole:
        return ws.icon;
    case IsDefaultRole:
        return ws.id == m_default;
    case SlotCountRole:
        return ws.slots.size();
    }
    return {};
}

QHash<int, QByteArray> WorkspaceModel::roleNames() const
{
    return {
        {IdRole, "wsId"},
        {NameRole, "wsName"},
        {IconRole, "wsIcon"},
        {IsDefaultRole, "isDefault"},
        {SlotCountRole, "slotCount"},
    };
}

void WorkspaceModel::setWorkspaces(const QList<Workspace> &workspaces, const QString &defaultWorkspace)
{
    beginResetModel();
    m_workspaces = workspaces;
    m_default = defaultWorkspace;
    endResetModel();
    Q_EMIT defaultWorkspaceChanged();
}

int WorkspaceModel::mergeNew(const QList<Workspace> &workspaces)
{
    QList<Workspace> fresh;
    for (const Workspace &ws : workspaces) {
        const bool known = std::any_of(m_workspaces.cbegin(), m_workspaces.cend(), [&](const Workspace &w) {
            return w.id == ws.id;
        });
        if (!known) {
            fresh.append(ws);
        }
    }
    if (fresh.isEmpty()) {
        return 0;
    }
    beginInsertRows({}, m_workspaces.size(), m_workspaces.size() + fresh.size() - 1);
    m_workspaces.append(fresh);
    endInsertRows();
    Q_EMIT changed();
    return fresh.size();
}

int WorkspaceModel::addWorkspace(const QString &name)
{
    QStringList ids;
    for (const Workspace &ws : std::as_const(m_workspaces)) {
        ids << ws.id;
    }
    Workspace ws;
    ws.name = name.trimmed();
    ws.id = ConfigWriter::slugFromName(ws.name, ids);
    ws.icon = QStringLiteral("window-duplicate");
    ws.order = m_workspaces.size();

    const int row = m_workspaces.size();
    beginInsertRows({}, row, row);
    m_workspaces.append(ws);
    endInsertRows();
    Q_EMIT changed();
    return row;
}

void WorkspaceModel::removeWorkspace(int row)
{
    if (!validRow(row)) {
        return;
    }
    const bool wasDefault = m_workspaces.at(row).id == m_default;
    beginRemoveRows({}, row, row);
    m_workspaces.removeAt(row);
    endRemoveRows();
    if (wasDefault) {
        m_default.clear();
        Q_EMIT defaultWorkspaceChanged();
    }
    Q_EMIT changed();
}

int WorkspaceModel::moveWorkspace(int row, int delta)
{
    if (!validRow(row)) {
        return row;
    }
    const int target = qBound(0, row + delta, int(m_workspaces.size()) - 1);
    if (target == row) {
        return row;
    }
    // beginMoveRows wants the destination as "insert before", hence +1 going down.
    beginMoveRows({}, row, row, {}, target > row ? target + 1 : target);
    m_workspaces.move(row, target);
    endMoveRows();
    Q_EMIT changed();
    return target;
}

void WorkspaceModel::setName(int row, const QString &name)
{
    if (!validRow(row) || m_workspaces.at(row).name == name) {
        return;
    }
    m_workspaces[row].name = name;
    Q_EMIT dataChanged(index(row), index(row), {NameRole});
    Q_EMIT changed();
}

void WorkspaceModel::setIcon(int row, const QString &icon)
{
    if (!validRow(row) || m_workspaces.at(row).icon == icon) {
        return;
    }
    m_workspaces[row].icon = icon;
    Q_EMIT dataChanged(index(row), index(row), {IconRole});
    Q_EMIT changed();
}

void WorkspaceModel::setDefaultRow(int row)
{
    const QString id = validRow(row) ? m_workspaces.at(row).id : QString();
    if (id == m_default) {
        return;
    }
    m_default = id;
    if (!m_workspaces.isEmpty()) {
        Q_EMIT dataChanged(index(0), index(m_workspaces.size() - 1), {IsDefaultRole});
    }
    Q_EMIT defaultWorkspaceChanged();
    Q_EMIT changed();
}

int WorkspaceModel::slotCount(int row) const
{
    return validRow(row) ? m_workspaces.at(row).slots.size() : 0;
}

const Slot *WorkspaceModel::slotAt(int row, int index) const
{
    if (!validRow(row) || index < 0 || index >= m_workspaces.at(row).slots.size()) {
        return nullptr;
    }
    return &m_workspaces.at(row).slots.at(index);
}

void WorkspaceModel::addSlot(int row)
{
    if (!validRow(row)) {
        return;
    }
    Slot slot;
    // A visible default: the right half of the screen.
    slot.geometry = QRectF(0.5, 0.0, 0.5, 1.0);
    m_workspaces[row].slots.append(slot);
    Q_EMIT dataChanged(index(row), index(row), {SlotCountRole});
    Q_EMIT slotsReset(row);
    Q_EMIT changed();
}

void WorkspaceModel::removeSlot(int row, int index)
{
    if (!slotAt(row, index)) {
        return;
    }
    m_workspaces[row].slots.removeAt(index);
    Q_EMIT dataChanged(this->index(row), this->index(row), {SlotCountRole});
    Q_EMIT slotsReset(row);
    Q_EMIT changed();
}

bool WorkspaceModel::setSlotAt(int row, int index, const Slot &slot)
{
    if (!slotAt(row, index)) {
        return false;
    }
    if (m_workspaces.at(row).slots.at(index) == slot) {
        return true;
    }
    m_workspaces[row].slots[index] = slot;
    Q_EMIT changed();
    return true;
}

SlotModel::SlotModel(WorkspaceModel *source)
    : QAbstractListModel(source)
    , m_source(source)
{
    connect(source, &WorkspaceModel::slotsReset, this, [this](int row) {
        if (row == m_row) {
            beginResetModel();
            endResetModel();
        }
    });
    // The workspace at m_row may now be another one; re-read it. The QML side
    // keeps workspaceRow bound to its current index.
    auto refresh = [this]() {
        beginResetModel();
        endResetModel();
    };
    connect(source, &QAbstractItemModel::modelReset, this, refresh);
    connect(source, &QAbstractItemModel::rowsRemoved, this, refresh);
    connect(source, &QAbstractItemModel::rowsMoved, this, refresh);
}

int SlotModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_source->slotCount(m_row);
}

QVariant SlotModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }
    const Slot *slot = m_source->slotAt(m_row, index.row());
    if (!slot) {
        return {};
    }
    switch (role) {
    case AppRole:
        return slot->app;
    case ArgsRole:
        return slot->args;
    case XRole:
        return slot->geometry.x();
    case YRole:
        return slot->geometry.y();
    case WidthRole:
        return slot->geometry.width();
    case HeightRole:
        return slot->geometry.height();
    }
    return {};
}

bool SlotModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid()) {
        return false;
    }
    const Slot *current = m_source->slotAt(m_row, index.row());
    if (!current) {
        return false;
    }
    Slot slot = *current;
    QRectF g = slot.geometry;
    bool ok = true;
    const double number = value.toDouble(&ok);
    switch (role) {
    case AppRole:
        slot.app = value.toString().trimmed();
        break;
    case ArgsRole:
        slot.args = value.toString();
        break;
    case XRole:
        g.moveLeft(number);
        break;
    case YRole:
        g.moveTop(number);
        break;
    case WidthRole:
        g.setWidth(number);
        break;
    case HeightRole:
        g.setHeight(number);
        break;
    default:
        return false;
    }
    const bool geometryRole = role >= XRole && role <= HeightRole;
    if (geometryRole) {
        if (!ok || number < 0.0 || number > 1.0 || ((role == WidthRole || role == HeightRole) && number <= 0.0)) {
            return false;
        }
        slot.geometry = g;
    }
    if (!m_source->setSlotAt(m_row, index.row(), slot)) {
        return false;
    }
    Q_EMIT dataChanged(index, index, {role});
    return true;
}

QHash<int, QByteArray> SlotModel::roleNames() const
{
    return {
        {AppRole, "app"},
        {ArgsRole, "args"},
        {XRole, "geoX"},
        {YRole, "geoY"},
        {WidthRole, "geoWidth"},
        {HeightRole, "geoHeight"},
    };
}

void SlotModel::setWorkspaceRow(int row)
{
    if (row == m_row) {
        return;
    }
    beginResetModel();
    m_row = row;
    endResetModel();
    Q_EMIT workspaceRowChanged();
}

bool SlotModel::setField(int index, const QString &field, const QVariant &value)
{
    const QHash<int, QByteArray> names = roleNames();
    for (auto it = names.cbegin(); it != names.cend(); ++it) {
        if (it.value() == field.toUtf8()) {
            return setData(this->index(index), value, it.key());
        }
    }
    return false;
}

} // namespace Vdw
