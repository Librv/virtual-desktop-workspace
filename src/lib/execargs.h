#pragma once

#include <QString>
#include <QStringList>

namespace Vdw
{

/// Join `args` for appending to a .desktop `Exec` line: each argument is
/// shell-quoted and `%` is doubled, so `100%` or `%U` reach the app literally
/// instead of being read as field codes.
QString quoteExecArgs(const QStringList &args);

}
