// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include "workspace.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>

#include <optional>
#include <utility>

namespace Vdw
{

/// A Chromium-based browser that has profiles vdw can pick.
struct Browser {
    QString desktopId; ///< .desktop id without the suffix, e.g. "helium"
    QString configDirName; ///< directory in the XDG config dir that holds "Local State"
};

/// The known browser for a .desktop id, or nullptr.
const Browser *findBrowser(const QString &desktopId);

/// The browser's user data directory (the one with "Local State").
QString userDataDir(const Browser &browser);

struct BrowserProfile {
    QString dir; ///< profile directory name, e.g. "Profile 1"
    QString name; ///< display name; the directory name when the browser has none

    bool operator==(const BrowserProfile &other) const = default;
};

/// Profiles listed in the JSON of a "Local State" file, sorted by dir name. On
/// failure returns an empty list and sets `*error` (which may be nullptr).
QList<BrowserProfile> parseLocalState(const QByteArray &json, QString *error);

/// Reads `<userDataDir>/Local State`. Refuses files over 16 MiB.
QList<BrowserProfile> readLocalState(const QString &userDataDir, QString *error);

/// `args` with every `--profile-directory` option removed and, when
/// `profileDir` is not empty, one `--profile-directory=<profileDir>` appended.
/// Returns nullopt when `args` cannot be split.
std::optional<QString> withProfileArgs(const QString &args, const QString &profileDir);

/// True when `args` has a `--profile-directory` option (with `=` or as a
/// separate value). False when `args` cannot be split.
bool hasProfileOption(const QString &args);

/// Why a saved profile cannot be used.
enum class ProfileIssue {
    None,
    Missing, ///< the profile is not in "Local State"
    Unreadable, ///< "Local State" cannot be read
};

/// The args to launch a browser slot with. `warning` is not empty when the
/// profile could not be used and the browser will open its last-used profile.
struct ProfileLaunch {
    QString args;
    QString warning;
    ProfileIssue issue = ProfileIssue::None; ///< why `warning` is set
};

/// The text of the notification vdwd sends for a profile that was not used.
struct ProfileNotice {
    QString title;
    QString text;
};

/// The notification for `issue` of a browser slot. `appName` is the display
/// name of the app. Both texts are empty for `ProfileIssue::None`.
ProfileNotice profileNotice(const QString &appName, const QString &profile, ProfileIssue issue);

/// Decides the launch args for a slot of a known browser with `profile` set.
/// `profiles` and `readError` are the result of `readLocalState` (`readError`
/// not empty means Local State could not be read). A profile that is not listed
/// is never passed on: Chromium would create an empty one.
ProfileLaunch profileLaunch(const QString &args, const QString &profile, const QList<BrowserProfile> &profiles, const QString &readError);

/// The result of `readLocalState` for each browser (profiles, read error),
/// so one open of a workspace reads each "Local State" once.
using LocalStateCache = QHash<QString, std::pair<QList<BrowserProfile>, QString>>;

/// The launch args for `slot`. Only a known browser with a profile set is
/// changed; for other slots `args` is `slot.args` and there is no warning.
ProfileLaunch slotLaunch(const Slot &slot, LocalStateCache &cache);

/// Slot indices grouped into launch chains. Slots in one chain launch one at a
/// time, in slot order, because a browser hands a new window to its running
/// process: the windows then share pid and app id and cannot be told apart.
/// All slots of one known browser form one chain when the workspace has 2 or
/// more of them, with or without a profile. Every other slot is a chain of
/// one. Chains are ordered by the index of their first slot.
QList<QList<int>> launchChains(const QList<Slot> &slots);

}
