// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "vdwkcm.h"

#include "browserprofile.h"
#include "config.h"
#include "configwriter.h"
#include "dbusnames.h"

#include <KConfig>
#include <KLocalizedString>
#include <KPluginFactory>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QProcess>

using namespace Vdw;

K_PLUGIN_CLASS_WITH_JSON(VdwKcm, "kcm_vdw.json")

VdwKcm::VdwKcm(QObject *parent, const KPluginMetaData &data)
    : KQuickConfigModule(parent, data)
    , m_workspaces(new WorkspaceModel(this))
    , m_slots(new SlotModel(m_workspaces))
    , m_apps(new AppModel(this))
{
    setButtons(Apply);
    connect(m_workspaces, &WorkspaceModel::changed, this, &VdwKcm::edited);
}

QVariantMap VdwKcm::appInfo(const QString &desktopId) const
{
    return AppModel::lookup(desktopId);
}

QVariantMap VdwKcm::browserProfiles(const QString &app, const QString &profile) const
{
    const Browser *browser = findBrowser(app);
    if (!browser) {
        return {{QStringLiteral("isBrowser"), false}, {QStringLiteral("choices"), QVariantList()}, {QStringLiteral("error"), QString()}};
    }
    QString error;
    const QList<BrowserProfile> profiles = readLocalState(userDataDir(*browser), &error);
    QVariantList choices;
    for (const ProfileChoice &choice : profileChoices(profiles, profile)) {
        choices.append(QVariantMap{
            {QStringLiteral("dir"), choice.dir},
            {QStringLiteral("name"), choice.name},
            {QStringLiteral("missing"), choice.missing},
        });
    }
    return {{QStringLiteral("isBrowser"), true}, {QStringLiteral("choices"), choices}, {QStringLiteral("error"), error}};
}

bool VdwKcm::hasProfileOption(const QString &args) const
{
    return Vdw::hasProfileOption(args);
}

void VdwKcm::load()
{
    const KConfig file(QStringLiteral("vdwrc"), KConfig::SimpleConfig);
    const Config config = Config::load(file);
    m_workspaces->setWorkspaces(config.workspaces(), config.defaultWorkspace());
    // Entries the loader skipped are not in the model and would be lost on save.
    m_notes = config.warnings();
    Q_EMIT notesChanged();
    updateProblems();
    setNeedsSave(false);
}

void VdwKcm::edited()
{
    updateProblems();
    setNeedsSave(true);
}

void VdwKcm::updateProblems()
{
    m_problems = ConfigWriter::validate(m_workspaces->workspaces(), m_workspaces->defaultWorkspace());
    Q_EMIT problemsChanged();
}

void VdwKcm::setStatusText(const QString &text)
{
    m_statusText = text;
    Q_EMIT statusTextChanged();
}

void VdwKcm::save()
{
    // Writing replaces every workspace: keep the ones added on disk since the
    // load (e.g. `vdwctl capture`).
    m_workspaces->mergeNew(Config::load(KConfig(QStringLiteral("vdwrc"), KConfig::SimpleConfig)).workspaces());
    updateProblems();
    if (!m_problems.isEmpty()) {
        setStatusText(i18n("Not saved: fix the problems listed above first."));
        return;
    }
    KConfig file(QStringLiteral("vdwrc"), KConfig::SimpleConfig);
    ConfigWriter::write(file, m_workspaces->workspaces(), m_workspaces->defaultWorkspace());
    if (!file.sync()) {
        setStatusText(i18n("Could not write %1.", file.name()));
        return;
    }
    m_workspaces->markSaved();
    setNeedsSave(false);
    setStatusText(i18n("Saved."));
    reloadDaemon();
}

void VdwKcm::reloadDaemon()
{
    // Never start the daemon just to reload it: it reads vdwrc when it starts.
    QDBusMessage message =
        QDBusMessage::createMethodCall(DBusNames::service, DBusNames::managerPath, DBusNames::managerInterface, QStringLiteral("ReloadConfig"));
    message.setAutoStartService(false);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<> reply = *w;
        if (!reply.isError()) {
            setStatusText(i18n("Saved."));
            return;
        }
        const QDBusError error = reply.error();
        if (error.type() == QDBusError::ServiceUnknown) {
            setStatusText(i18n("Saved. The daemon is not running; it reads the file when it starts."));
        } else {
            setStatusText(i18n("Saved, but the daemon reported: %1", error.message()));
        }
    });
}

void VdwKcm::captureCurrentDesktop(const QString &name)
{
    QDBusMessage message =
        QDBusMessage::createMethodCall(DBusNames::service, DBusNames::managerPath, DBusNames::managerInterface, QStringLiteral("CaptureCurrentDesktop"));
    message.setArguments({name});
    message.setAutoStartService(false);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QString> reply = *w;
        if (reply.isError()) {
            setStatusText(i18n("Could not reach the workspace daemon: %1", reply.error().message()));
            return;
        }
        const QString text = reply.value();
        // Success is a bare workspace id; anything else (e.g. "error: ...") is a failure.
        if (!Config::isValidId(text)) {
            setStatusText(text.isEmpty() ? i18n("The daemon returned no workspace id.") : text);
            return;
        }
        // The daemon wrote the new workspace to vdwrc. With unsaved edits a
        // reload would throw them away, so only append what is new.
        if (needsSave()) {
            const KConfig file(QStringLiteral("vdwrc"), KConfig::SimpleConfig);
            m_workspaces->mergeNew(Config::load(file).workspaces());
        } else {
            load();
        }
        setStatusText(i18n("Saved workspace \"%1\".", text));
    });
}

void VdwKcm::openShortcuts()
{
    QProcess::startDetached(QStringLiteral("systemsettings"), {QStringLiteral("kcm_keys")});
}

#include "vdwkcm.moc"
