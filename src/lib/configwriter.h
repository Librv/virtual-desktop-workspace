#pragma once

#include "workspace.h"

#include <QList>
#include <QString>
#include <QStringList>

class KConfig;

namespace Vdw::ConfigWriter
{

/// Slug for a workspace name: lower case ASCII letters and digits, runs of
/// anything else become one "-". Unique among `existingIds` (suffix "-2", ...).
/// Falls back to "workspace" when the name has no usable characters.
QString slugFromName(const QString &name, const QStringList &existingIds);

/// Why `workspaces` cannot be saved (empty list: fine). Checks ids, unique
/// ids, names, slot apps and slot geometry against the vdwrc rules.
QStringList validate(const QList<Workspace> &workspaces, const QString &defaultWorkspace);

/// Replace all [Workspace] groups and `DefaultWorkspace` in `config` with the
/// given data, in the format of docs/ARCHITECTURE.md. `Order` is the list
/// index. Other [General] keys are left alone. The caller syncs.
void write(KConfig &config, const QList<Workspace> &workspaces, const QString &defaultWorkspace);

/// Geometry as "x,y,width,height" (the vdwrc format).
QString formatGeometry(const QRectF &geometry);

} // namespace Vdw::ConfigWriter
