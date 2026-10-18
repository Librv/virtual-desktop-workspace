// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include <QList>
#include <Qt>

namespace Vdw::ShortcutKeys
{

/// The modifier keys (Qt::Key_Control, Key_Meta, Key_Shift, Key_Alt) that
/// `chord` (a Qt key|modifier int, as kglobalaccel sends it) needs, in that
/// order. Empty for 0.
QList<Qt::Key> modifierKeys(int chord);

}
