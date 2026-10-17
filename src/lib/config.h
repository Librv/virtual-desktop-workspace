#pragma once

#include "workspace.h"

#include <QList>
#include <QString>
#include <QStringList>

class KConfig;

namespace Vdw
{

/// The parsed `vdwrc`. Pure data plus a loader; no GUI, no D-Bus.
class Config
{
public:
    /// Parse `config`. Invalid entries are skipped with a warning (also kept in
    /// warnings()); the rest still loads.
    static Config load(const KConfig &config);

    /// True if `id` is a valid workspace id slug ([a-z0-9-]+).
    static bool isValidId(const QString &id);

    /// Parse "x,y,width,height" (fractions 0..1, width and height > 0).
    /// Returns false and leaves `out` untouched if invalid.
    static bool parseGeometry(const QString &text, QRectF &out);

    const QList<Workspace> &workspaces() const
    {
        return m_workspaces;
    }
    const Workspace *find(const QString &id) const;

    QString defaultWorkspace() const
    {
        return m_defaultWorkspace;
    }
    int holdThresholdMs() const
    {
        return m_holdThresholdMs;
    }
    int confirmTimeoutMs() const
    {
        return m_confirmTimeoutMs;
    }
    int placementTimeoutMs() const
    {
        return m_placementTimeoutMs;
    }
    QStringList warnings() const
    {
        return m_warnings;
    }

    /// JSON array of {id, name, icon, isDefault, slots}, in sorted order.
    QByteArray toJson() const;

private:
    QList<Workspace> m_workspaces; ///< sorted by Order, then Name
    QString m_defaultWorkspace;
    int m_holdThresholdMs = 250;
    int m_confirmTimeoutMs = 2000;
    int m_placementTimeoutMs = 10000;
    QStringList m_warnings;
};

} // namespace Vdw
