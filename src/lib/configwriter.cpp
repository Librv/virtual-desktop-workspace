// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "configwriter.h"

#include "config.h"

#include <KConfig>
#include <KConfigGroup>

#include <QRegularExpression>
#include <QSet>

namespace Vdw::ConfigWriter
{

QString formatGeometry(const QRectF &g)
{
    auto num = [](double v) {
        return QString::number(v, 'g', 6);
    };
    return QStringLiteral("%1,%2,%3,%4").arg(num(g.x()), num(g.y()), num(g.width()), num(g.height()));
}

QString slugFromName(const QString &name, const QStringList &existingIds)
{
    // Decompose so that "e with acute" becomes "e" plus a mark that is dropped.
    QString slug = name.normalized(QString::NormalizationForm_KD).toLower();
    static const QRegularExpression unusable(QStringLiteral("[^a-z0-9]+"));
    slug.replace(unusable, QStringLiteral("-"));
    while (slug.startsWith(QLatin1Char('-'))) {
        slug.remove(0, 1);
    }
    while (slug.endsWith(QLatin1Char('-'))) {
        slug.chop(1);
    }
    if (slug.isEmpty()) {
        slug = QStringLiteral("workspace");
    }
    QString candidate = slug;
    for (int n = 2; existingIds.contains(candidate); ++n) {
        candidate = QStringLiteral("%1-%2").arg(slug).arg(n);
    }
    return candidate;
}

QStringList validate(const QList<Workspace> &workspaces, const QString &defaultWorkspace)
{
    QStringList errors;
    QSet<QString> seen;
    for (const Workspace &ws : workspaces) {
        const QString label = ws.name.isEmpty() ? ws.id : ws.name;
        if (!Config::isValidId(ws.id)) {
            errors << QStringLiteral("Workspace \"%1\": invalid id \"%2\"").arg(label, ws.id);
        } else if (seen.contains(ws.id)) {
            errors << QStringLiteral("Workspace \"%1\": duplicate id \"%2\"").arg(label, ws.id);
        }
        seen.insert(ws.id);
        if (ws.name.trimmed().isEmpty()) {
            errors << QStringLiteral("Workspace \"%1\": the name is empty").arg(ws.id);
        }
        for (int i = 0; i < ws.slots.size(); ++i) {
            const Slot &slot = ws.slots.at(i);
            if (slot.app.trimmed().isEmpty()) {
                errors << QStringLiteral("Workspace \"%1\", app %2: no app chosen").arg(label).arg(i + 1);
            }
            QRectF parsed;
            if (!Config::parseGeometry(formatGeometry(slot.geometry), parsed)) {
                errors
                    << QStringLiteral("Workspace \"%1\", app %2: geometry must be fractions from 0 to 1 with width and height above 0").arg(label).arg(i + 1);
            }
        }
    }
    if (!defaultWorkspace.isEmpty() && !seen.contains(defaultWorkspace)) {
        errors << QStringLiteral("The default workspace \"%1\" does not exist").arg(defaultWorkspace);
    }
    return errors;
}

void write(KConfig &config, const QList<Workspace> &workspaces, const QString &defaultWorkspace)
{
    config.deleteGroup(QStringLiteral("Workspace"));

    KConfigGroup general = config.group(QStringLiteral("General"));
    if (defaultWorkspace.isEmpty()) {
        general.deleteEntry("DefaultWorkspace");
    } else {
        general.writeEntry("DefaultWorkspace", defaultWorkspace);
    }

    KConfigGroup root = config.group(QStringLiteral("Workspace"));
    for (int i = 0; i < workspaces.size(); ++i) {
        const Workspace &ws = workspaces.at(i);
        KConfigGroup g = root.group(ws.id);
        g.writeEntry("Name", ws.name);
        g.writeEntry("Icon", ws.icon);
        g.writeEntry("Order", i);
        KConfigGroup slots = g.group(QStringLiteral("Slot"));
        for (int s = 0; s < ws.slots.size(); ++s) {
            const Slot &slot = ws.slots.at(s);
            KConfigGroup sg = slots.group(QString::number(s));
            sg.writeEntry("App", slot.app);
            sg.writeEntry("Args", slot.args);
            sg.writeEntry("Geometry", formatGeometry(slot.geometry));
        }
    }
}

} // namespace Vdw::ConfigWriter
