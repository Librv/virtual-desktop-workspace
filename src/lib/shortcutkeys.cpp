// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "shortcutkeys.h"

#include <QStringList>

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

QString modifierText(const QKeySequence &sequence)
{
    QStringList names;
    for (const Qt::Key key : modifierKeys(sequence)) {
        switch (key) {
        case Qt::Key_Control:
            names << QStringLiteral("Ctrl");
            break;
        case Qt::Key_Meta:
            names << QStringLiteral("Meta");
            break;
        case Qt::Key_Shift:
            names << QStringLiteral("Shift");
            break;
        default:
            names << QStringLiteral("Alt");
            break;
        }
    }
    return names.join(QLatin1Char('+'));
}

}
