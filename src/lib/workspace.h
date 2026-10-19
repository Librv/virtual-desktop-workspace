// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include <QJsonObject>
#include <QList>
#include <QRectF>
#include <QString>

namespace Vdw
{

/// One app of a workspace: which app to launch, with which extra arguments,
/// and where to put its window (fractions 0..1 of the usable area).
struct Slot {
    QString app; ///< .desktop id, e.g. "org.kde.konsole"
    QString args; ///< one string, split with KShell::splitArgs at launch time
    QRectF geometry; ///< x, y, width, height as fractions
    QString profile; ///< Chromium profile directory, e.g. "Profile 1"; empty = last used

    bool operator==(const Slot &other) const = default;
    QJsonObject toJson() const;
};

struct Workspace {
    QString id; ///< stable slug, [a-z0-9-]+
    QString name;
    QString icon;
    int order = 0;
    QList<Slot> slots;

    bool operator==(const Workspace &other) const = default;
    QJsonObject toJson(bool isDefault) const;
};

} // namespace Vdw
