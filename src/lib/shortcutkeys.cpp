// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "shortcutkeys.h"

namespace Vdw::ShortcutKeys
{

QList<Qt::Key> modifierKeys(int chord)
{
    QList<Qt::Key> keys;
    const Qt::KeyboardModifiers modifiers = Qt::KeyboardModifiers::fromInt(chord & Qt::KeyboardModifierMask);
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

}
