// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include <QKeySequence>
#include <QList>
#include <QString>

namespace Vdw::ShortcutKeys
{

/// The modifier keys (Qt::Key_Control, Key_Meta, Key_Shift, Key_Alt) that the
/// first chord of `sequence` needs, in that order. Empty for an empty sequence.
QList<Qt::Key> modifierKeys(const QKeySequence &sequence);

/// The non-modifier key of the first chord as display text ("D" for
/// Ctrl+Meta+D). Empty for an empty sequence.
QString keyText(const QKeySequence &sequence);

}
