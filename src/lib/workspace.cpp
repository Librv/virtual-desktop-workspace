// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "workspace.h"

#include <QJsonArray>

namespace Vdw
{

QJsonObject Slot::toJson() const
{
    return QJsonObject{
        {QStringLiteral("app"), app},
        {QStringLiteral("args"), args},
        {QStringLiteral("geometry"),
         QJsonObject{
             {QStringLiteral("x"), geometry.x()},
             {QStringLiteral("y"), geometry.y()},
             {QStringLiteral("width"), geometry.width()},
             {QStringLiteral("height"), geometry.height()},
         }},
    };
}

QJsonObject Workspace::toJson(bool isDefault) const
{
    QJsonArray slotArray;
    for (const Slot &slot : slots) {
        slotArray.append(slot.toJson());
    }
    return QJsonObject{
        {QStringLiteral("id"), id},
        {QStringLiteral("name"), name},
        {QStringLiteral("icon"), icon},
        {QStringLiteral("isDefault"), isDefault},
        {QStringLiteral("slots"), slotArray},
    };
}

} // namespace Vdw
