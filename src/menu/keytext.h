// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include <QString>

namespace Vdw::ShortcutKeys
{

/// The non-modifier key of `chord` (a Qt key|modifier int) as display text
/// ("D" for Ctrl+Meta+D). Empty for 0.
QString keyText(int chord);

/// The modifiers of `chord` as display text in modifierKeys() order
/// ("Ctrl+Meta" for Ctrl+Meta+D). Empty when there are none.
QString modifierText(int chord);

}
