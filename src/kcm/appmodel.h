#pragma once

#include <QAbstractListModel>
#include <QList>

/// The installed apps the user can pick for a slot: `.desktop` ids with name
/// and icon, filterable by a search text.
class AppModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        DesktopIdRole = Qt::UserRole + 1,
        NameRole,
        IconRole,
        CommentRole,
    };

    explicit AppModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Show only apps whose name, comment or id contain `text` (case insensitive).
    Q_INVOKABLE void setFilter(const QString &text);

    /// {found, name, icon} for a `.desktop` id (without the ".desktop" suffix).
    static QVariantMap lookup(const QString &desktopId);

private:
    struct Entry {
        QString desktopId;
        QString name;
        QString icon;
        QString comment;
    };
    QList<Entry> m_all;
    QList<Entry> m_shown;
};
