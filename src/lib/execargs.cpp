// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "execargs.h"

#include <KShell>

namespace Vdw
{

QString quoteExecArgs(const QStringList &args)
{
    QStringList escaped;
    escaped.reserve(args.size());
    for (const QString &arg : args) {
        QString copy = arg;
        escaped.append(copy.replace(QLatin1Char('%'), QStringLiteral("%%")));
    }
    return KShell::joinArgs(escaped);
}

}
