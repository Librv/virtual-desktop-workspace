// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "keytext.h"

#include "shortcutkeys.h"

#include <QKeySequence>
#include <QStringList>

namespace Vdw::ShortcutKeys
{

QString keyText(int chord)
{
    if (chord == 0) {
        return {};
    }
    return QKeySequence(chord & ~Qt::KeyboardModifierMask).toString(QKeySequence::PortableText);
}

QString modifierText(int chord)
{
    QStringList names;
    for (const Qt::Key key : modifierKeys(chord)) {
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
