// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "config.h"

#include <KConfig>
#include <KConfigGroup>

#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QRegularExpression>

#include <algorithm>

Q_LOGGING_CATEGORY(VDW_CONFIG, "vdw.config")

namespace Vdw
{

namespace
{

const QStringList workspaceKeys = {QStringLiteral("Name"), QStringLiteral("Icon"), QStringLiteral("Order")};
const QStringList slotKeys = {QStringLiteral("App"), QStringLiteral("Args"), QStringLiteral("Profile"), QStringLiteral("Geometry")};
const QStringList generalKeys = {QStringLiteral("DefaultWorkspace"),
                                 QStringLiteral("HoldThresholdMs"),
                                 QStringLiteral("ConfirmTimeoutMs"),
                                 QStringLiteral("PlacementTimeoutMs")};

void warn(QStringList &warnings, const QString &message)
{
    warnings.append(message);
    qCWarning(VDW_CONFIG).noquote() << message;
}

void warnUnknownKeys(QStringList &warnings, const KConfigGroup &group, const QStringList &known, const QString &where)
{
    const QStringList keys = group.keyList();
    for (const QString &key : keys) {
        if (!known.contains(key)) {
            warn(warnings, QStringLiteral("%1: unknown key '%2' ignored").arg(where, key));
        }
    }
}

int readPositiveInt(QStringList &warnings, const KConfigGroup &group, const QString &key, int fallback)
{
    if (!group.hasKey(key)) {
        return fallback;
    }
    bool ok = false;
    const int value = group.readEntry(key, QString()).toInt(&ok);
    if (!ok || value <= 0) {
        warn(warnings, QStringLiteral("[General] %1: invalid value, using %2").arg(key).arg(fallback));
        return fallback;
    }
    return value;
}

} // namespace

bool Config::isValidId(const QString &id)
{
    // anchoredPattern: a plain "$" would also accept a trailing newline
    static const QRegularExpression re(QRegularExpression::anchoredPattern(QStringLiteral("[a-z0-9-]+")));
    return re.match(id).hasMatch();
}

bool Config::parseGeometry(const QString &text, QRectF &out)
{
    const QStringList parts = text.split(QLatin1Char(','));
    if (parts.size() != 4) {
        return false;
    }
    double v[4];
    for (int i = 0; i < 4; ++i) {
        bool ok = false;
        v[i] = parts[i].trimmed().toDouble(&ok);
        if (!ok || !(v[i] >= 0.0 && v[i] <= 1.0)) { // also rejects NaN
            return false;
        }
    }
    if (v[2] <= 0.0 || v[3] <= 0.0) {
        return false;
    }
    out = QRectF(v[0], v[1], v[2], v[3]);
    return true;
}

Config Config::load(const KConfig &config)
{
    Config result;

    const KConfigGroup general = config.group(QStringLiteral("General"));
    warnUnknownKeys(result.m_warnings, general, generalKeys, QStringLiteral("[General]"));
    result.m_holdThresholdMs = readPositiveInt(result.m_warnings, general, QStringLiteral("HoldThresholdMs"), result.m_holdThresholdMs);
    result.m_confirmTimeoutMs = readPositiveInt(result.m_warnings, general, QStringLiteral("ConfirmTimeoutMs"), result.m_confirmTimeoutMs);
    result.m_placementTimeoutMs = readPositiveInt(result.m_warnings, general, QStringLiteral("PlacementTimeoutMs"), result.m_placementTimeoutMs);

    const KConfigGroup workspaces = config.group(QStringLiteral("Workspace"));
    const QStringList ids = workspaces.groupList();
    for (const QString &id : ids) {
        const QString where = QStringLiteral("[Workspace][%1]").arg(id);
        if (!isValidId(id)) {
            warn(result.m_warnings, QStringLiteral("%1: invalid workspace id (allowed: a-z, 0-9, -), skipped").arg(where));
            continue;
        }
        const KConfigGroup wsGroup = workspaces.group(id);
        warnUnknownKeys(result.m_warnings, wsGroup, workspaceKeys, where);

        Workspace ws;
        ws.id = id;
        ws.name = wsGroup.readEntry("Name", QString());
        if (ws.name.isEmpty()) {
            ws.name = id;
        }
        ws.icon = wsGroup.readEntry("Icon", QString());
        if (wsGroup.hasKey("Order")) {
            bool ok = false;
            ws.order = wsGroup.readEntry("Order", QString()).toInt(&ok);
            if (!ok) {
                warn(result.m_warnings, QStringLiteral("%1 Order: not an integer, using 0").arg(where));
                ws.order = 0;
            }
        }

        const KConfigGroup slotsGroup = wsGroup.group(QStringLiteral("Slot"));
        QList<std::pair<int, Slot>> slots;
        const QStringList slotNames = slotsGroup.groupList();
        for (const QString &slotName : slotNames) {
            const QString slotWhere = QStringLiteral("%1[Slot][%2]").arg(where, slotName);
            bool ok = false;
            const int index = slotName.toInt(&ok);
            if (!ok || index < 0) {
                warn(result.m_warnings, QStringLiteral("%1: slot name is not a non-negative integer, skipped").arg(slotWhere));
                continue;
            }
            const KConfigGroup g = slotsGroup.group(slotName);
            warnUnknownKeys(result.m_warnings, g, slotKeys, slotWhere);

            Slot slot;
            slot.app = g.readEntry("App", QString()).trimmed();
            if (slot.app.isEmpty()) {
                warn(result.m_warnings, QStringLiteral("%1: missing App, slot skipped").arg(slotWhere));
                continue;
            }
            slot.args = g.readEntry("Args", QString());
            slot.profile = g.readEntry("Profile", QString()).trimmed();
            if (!parseGeometry(g.readEntry("Geometry", QString()), slot.geometry)) {
                warn(result.m_warnings, QStringLiteral("%1: bad or missing Geometry (want x,y,width,height as fractions 0..1), slot skipped").arg(slotWhere));
                continue;
            }
            slots.append({index, slot});
        }
        std::stable_sort(slots.begin(), slots.end(), [](const auto &a, const auto &b) {
            return a.first < b.first;
        });
        for (const auto &entry : std::as_const(slots)) {
            ws.slots.append(entry.second);
        }
        result.m_workspaces.append(ws);
    }

    std::stable_sort(result.m_workspaces.begin(), result.m_workspaces.end(), [](const Workspace &a, const Workspace &b) {
        if (a.order != b.order) {
            return a.order < b.order;
        }
        const int byName = QString::compare(a.name, b.name, Qt::CaseInsensitive);
        return byName != 0 ? byName < 0 : a.id < b.id;
    });

    const QString defaultId = general.readEntry("DefaultWorkspace", QString());
    if (!defaultId.isEmpty()) {
        if (result.find(defaultId)) {
            result.m_defaultWorkspace = defaultId;
        } else {
            warn(result.m_warnings, QStringLiteral("[General] DefaultWorkspace: unknown workspace '%1', ignored").arg(defaultId));
        }
    }
    return result;
}

const Workspace *Config::find(const QString &id) const
{
    for (const Workspace &ws : m_workspaces) {
        if (ws.id == id) {
            return &ws;
        }
    }
    return nullptr;
}

QByteArray Config::toJson() const
{
    QJsonArray array;
    for (const Workspace &ws : m_workspaces) {
        array.append(ws.toJson(ws.id == m_defaultWorkspace));
    }
    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

} // namespace Vdw
