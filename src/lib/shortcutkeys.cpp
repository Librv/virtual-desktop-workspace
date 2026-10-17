// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "shortcutkeys.h"

namespace Vdw::ShortcutKeys
{

QList<Qt::Key> modifierKeys(const QKeySequence &sequence)
{
    QList<Qt::Key> keys;
    if (sequence.isEmpty()) {
        return keys;
    }
    const Qt::KeyboardModifiers modifiers = sequence[0].keyboardModifiers();
    if (modifiers.testFlag(Qt::ControlModifier)) {
        keys.append(Qt::Key_Control);
    }
    if (modifiers.testFlag(Qt::MetaModifier)) {
        keys.append(Qt::Key_Meta);
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        keys.append(Qt::Key_Shift);
    }
    if (modifiers.testFlag(Qt::AltModifier)) {
        keys.append(Qt::Key_Alt);
    }
    return keys;
}

QString keyText(const QKeySequence &sequence)
{
    if (sequence.isEmpty()) {
        return {};
    }
    return QKeySequence(sequence[0].key()).toString(QKeySequence::PortableText);
}

}
