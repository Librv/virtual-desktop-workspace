// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "appmodel.h"

#include <KApplicationTrader>
#include <KService>

#include <QVariantMap>

#include <algorithm>

AppModel::AppModel(QObject *parent)
    : QAbstractListModel(parent)
{
    const KService::List services = KApplicationTrader::query([](const KService::Ptr &service) {
        return !service->noDisplay() && !service->desktopEntryName().isEmpty();
    });
    for (const KService::Ptr &service : services) {
        m_all.append({service->desktopEntryName(), service->name(), service->icon(), service->comment()});
    }
    std::sort(m_all.begin(), m_all.end(), [](const Entry &a, const Entry &b) {
        return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
    });
    m_shown = m_all;
}

int AppModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_shown.size();
}

QVariant AppModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_shown.size()) {
        return {};
    }
    const Entry &e = m_shown.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return e.name;
    case DesktopIdRole:
        return e.desktopId;
    case IconRole:
        return e.icon;
    case CommentRole:
        return e.comment;
    }
    return {};
}

QHash<int, QByteArray> AppModel::roleNames() const
{
    return {
        {DesktopIdRole, "appId"},
        {NameRole, "appName"},
        {IconRole, "appIcon"},
        {CommentRole, "appComment"},
    };
}

void AppModel::setFilter(const QString &text)
{
    beginResetModel();
    const QString needle = text.trimmed();
    m_shown.clear();
    for (const Entry &e : std::as_const(m_all)) {
        if (needle.isEmpty() || e.name.contains(needle, Qt::CaseInsensitive) || e.comment.contains(needle, Qt::CaseInsensitive)
            || e.desktopId.contains(needle, Qt::CaseInsensitive)) {
            m_shown.append(e);
        }
    }
    endResetModel();
}

QVariantMap AppModel::lookup(const QString &desktopId)
{
    QVariantMap result{{QStringLiteral("found"), false},
                       {QStringLiteral("name"), desktopId},
                       {QStringLiteral("icon"), QStringLiteral("application-x-executable")}};
    if (desktopId.isEmpty()) {
        return result;
    }
    const KService::Ptr service = KService::serviceByDesktopName(desktopId);
    if (service) {
        result[QStringLiteral("found")] = true;
        result[QStringLiteral("name")] = service->name();
        result[QStringLiteral("icon")] = service->icon();
    }
    return result;
}
