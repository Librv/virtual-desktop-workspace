// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "browserprofile.h"

#include <KShell>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>

namespace Vdw
{

namespace
{

constexpr qint64 maxLocalStateBytes = 16 * 1024 * 1024;

void setError(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
}

const QString profileOption = QStringLiteral("--profile-directory");

} // namespace

const Browser *findBrowser(const QString &desktopId)
{
    static const Browser browsers[] = {
        {QStringLiteral("helium"), QStringLiteral("net.imput.helium")},
    };
    for (const Browser &browser : browsers) {
        if (browser.desktopId == desktopId) {
            return &browser;
        }
    }
    return nullptr;
}

QString userDataDir(const Browser &browser)
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1Char('/') + browser.configDirName;
}

QList<BrowserProfile> parseLocalState(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, QStringLiteral("Local State is not valid JSON: %1").arg(parseError.errorString()));
        return {};
    }
    if (!doc.isObject()) {
        setError(error, QStringLiteral("Local State is not a JSON object"));
        return {};
    }
    const QJsonValue profile = doc.object().value(QStringLiteral("profile"));
    if (!profile.isObject()) {
        setError(error, QStringLiteral("Local State has no \"profile\" object"));
        return {};
    }
    const QJsonValue cache = profile.toObject().value(QStringLiteral("info_cache"));
    if (!cache.isObject()) {
        setError(error, QStringLiteral("Local State has no \"profile.info_cache\" object"));
        return {};
    }
    const QJsonObject infoCache = cache.toObject();
    QList<BrowserProfile> result;
    result.reserve(infoCache.size());
    for (auto it = infoCache.begin(); it != infoCache.end(); ++it) {
        BrowserProfile entry;
        entry.dir = it.key();
        const QJsonValue name = it.value().toObject().value(QStringLiteral("name"));
        entry.name = name.isString() ? name.toString() : entry.dir;
        result.append(entry);
    }
    return result;
}

QList<BrowserProfile> readLocalState(const QString &userDataDir, QString *error)
{
    QFile file(userDataDir + QStringLiteral("/Local State"));
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("cannot read %1: %2").arg(file.fileName(), file.errorString()));
        return {};
    }
    // Bounded read, not a size check: the file can grow after open, and
    // special files report size 0.
    const QByteArray json = file.read(maxLocalStateBytes + 1);
    if (json.size() > maxLocalStateBytes) {
        setError(error, QStringLiteral("%1 is too large").arg(file.fileName()));
        return {};
    }
    return parseLocalState(json, error);
}

std::optional<QString> withProfileArgs(const QString &args, const QString &profileDir)
{
    KShell::Errors status = KShell::NoError;
    const QStringList split = KShell::splitArgs(args, KShell::AbortOnMeta | KShell::TildeExpand, &status);
    if (status != KShell::NoError) {
        return std::nullopt;
    }
    QStringList kept;
    for (qsizetype i = 0; i < split.size(); ++i) {
        const QString &arg = split.at(i);
        if (arg == profileOption) {
            // Skip its value, if any. A next option is not a value.
            if (i + 1 < split.size() && !split.at(i + 1).startsWith(QLatin1Char('-'))) {
                ++i;
            }
        } else if (!arg.startsWith(profileOption + QLatin1Char('='))) {
            kept.append(arg);
        }
    }
    if (!profileDir.isEmpty()) {
        kept.append(profileOption + QLatin1Char('=') + profileDir);
    }
    return KShell::joinArgs(kept);
}

ProfileLaunch profileLaunch(const QString &args, const QString &profile, const QList<BrowserProfile> &profiles, const QString &readError)
{
    QString warning;
    QString dir = profile;
    if (!readError.isEmpty()) {
        warning = QStringLiteral("cannot read the browser profiles: ") + readError;
        dir.clear();
    } else if (std::none_of(profiles.cbegin(), profiles.cend(), [&profile](const BrowserProfile &p) {
                   return p.dir == profile;
               })) {
        warning = QStringLiteral("the profile does not exist");
        dir.clear();
    }
    const std::optional<QString> result = withProfileArgs(args, dir);
    if (!result) {
        // vdw-ui reports the parse error for the unchanged args.
        return {args, warning};
    }
    return {*result, warning};
}

ProfileLaunch slotLaunch(const Slot &slot, LocalStateCache &cache)
{
    const Browser *browser = findBrowser(slot.app);
    if (!browser || slot.profile.isEmpty()) {
        return {slot.args, QString()};
    }
    auto it = cache.find(slot.app);
    if (it == cache.end()) {
        QString error;
        const QList<BrowserProfile> profiles = readLocalState(userDataDir(*browser), &error);
        it = cache.insert(slot.app, {profiles, error});
    }
    return profileLaunch(slot.args, slot.profile, it->first, it->second);
}

QList<QList<int>> launchChains(const QList<Slot> &slots)
{
    QHash<const Browser *, int> browserSlots;
    for (const Slot &slot : slots) {
        if (const Browser *browser = findBrowser(slot.app)) {
            ++browserSlots[browser];
        }
    }
    QList<QList<int>> chains;
    QHash<const Browser *, int> chainOf;
    for (int i = 0; i < slots.size(); ++i) {
        const Browser *browser = findBrowser(slots.at(i).app);
        if (!browser || browserSlots.value(browser) < 2) {
            chains.append(QList<int>{i});
            continue;
        }
        const auto it = chainOf.constFind(browser);
        if (it == chainOf.constEnd()) {
            chainOf.insert(browser, chains.size());
            chains.append(QList<int>{i});
        } else {
            chains[*it].append(i);
        }
    }
    return chains;
}

}
