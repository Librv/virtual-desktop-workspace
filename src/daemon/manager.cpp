// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "manager.h"

#include "configwriter.h"
#include "gesturecontroller.h"
#include "kwinscripts.h"
#include "launcher.h"
#include "layout.h"

#include <KConfig>
#include <KService>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QTimer>
#include <algorithm>
#include <limits>

#include <memory>

Q_LOGGING_CATEGORY(VDW_DAEMON, "vdw.daemon")

namespace Vdw
{

namespace
{
Config loadConfig(const QString &path)
{
    if (path.isEmpty()) {
        KSharedConfig::Ptr shared = KSharedConfig::openConfig(QStringLiteral("vdwrc"), KConfig::SimpleConfig);
        shared->reparseConfiguration();
        return Config::load(*shared);
    }
    return Config::load(KConfig(path, KConfig::SimpleConfig));
}

/// The usable area of the current desktop; `error` is empty on success.
void queryArea(KWinScripts *scripts, const std::function<void(const QRect &area, const QString &error)> &callback)
{
    scripts->query(QStringLiteral("area"), [callback](const QJsonObject &area, const QString &error) {
        if (!error.isEmpty()) {
            callback({}, error);
            return;
        }
        const QRect rect(area.value(QLatin1String("x")).toInt(),
                         area.value(QLatin1String("y")).toInt(),
                         area.value(QLatin1String("width")).toInt(),
                         area.value(QLatin1String("height")).toInt());
        if (rect.isEmpty()) {
            callback({}, QStringLiteral("KWin reported an empty usable area"));
            return;
        }
        callback(rect, QString());
    });
}
}

Manager::Manager(const QString &configPath, KWinScripts *scripts, Launcher *launcher, QObject *parent)
    : QObject(parent)
    , m_config(loadConfig(configPath))
    , m_configPath(configPath)
    , m_scripts(scripts)
    , m_launcher(launcher)
    , m_matcher(m_config.placementTimeoutMs())
{
    m_expiryTimer.setSingleShot(true);
    connect(&m_expiryTimer, &QTimer::timeout, this, &Manager::expirePending);
    qCInfo(VDW_DAEMON) << "loaded" << m_config.workspaces().size() << "workspaces," << m_config.warnings().size() << "warnings";
}

QString Manager::ListWorkspaces()
{
    return QString::fromUtf8(m_config.toJson());
}

QString Manager::OpenWorkspace(const QString &id, bool force)
{
    qCInfo(VDW_DAEMON) << "OpenWorkspace" << id << "force" << force;
    if (!Config::isValidId(id) || !m_config.find(id)) {
        return QStringLiteral("error: unknown workspace");
    }

    // The reply goes out once the launches have started (see openWorkspace).
    setDelayedReply(true);
    const QDBusMessage message = this->message();
    const QDBusConnection connection = this->connection();
    open(id, force, [message, connection](const QString &result) {
        qCInfo(VDW_DAEMON) << "OpenWorkspace reply" << result;
        connection.send(message.createReply(result));
    });
    return QString();
}

void Manager::open(const QString &id, bool force, const Reply &reply)
{
    const Workspace *workspace = Config::isValidId(id) ? m_config.find(id) : nullptr;
    if (!workspace) {
        QTimer::singleShot(0, this, [reply] {
            reply(QStringLiteral("error: unknown workspace"));
        });
        return;
    }
    openWorkspace(*workspace, force, reply);
}

void Manager::openWorkspace(const Workspace &workspace, bool force, const Reply &reply)
{
    // A previous open whose windows are still on their way counts as busy: the
    // desktop only looks empty because they have not appeared yet.
    if (!force && (m_opening || m_matcher.pendingCount() > 0)) {
        reply(QStringLiteral("busy"));
        return;
    }
    // Two non-forced opens within the query latency would both see an empty
    // desktop: the first holds m_opening until its launches are registered.
    // Every failure path ends in `reply`, which clears it too.
    Reply done = reply;
    if (!force) {
        m_opening = true;
        done = [this, reply](const QString &text) {
            m_opening = false;
            reply(text);
        };
    }
    m_scripts->query(QStringLiteral("empty"), [this, workspace, force, reply = done](const QJsonObject &result, const QString &error) {
        if (!error.isEmpty()) {
            reply(QStringLiteral("error: ") + error);
            return;
        }
        if (!force && !result.value(QLatin1String("empty")).toBool()) {
            reply(QStringLiteral("busy"));
            return;
        }
        queryArea(m_scripts, [this, workspace, force, reply](const QRect &rect, const QString &areaError) {
            if (!areaError.isEmpty()) {
                reply(QStringLiteral("error: ") + areaError);
                return;
            }
            launchSlots(workspace, rect, reply);
            // Registered now: from here pendingCount() answers for this open.
            if (!force) {
                m_opening = false;
            }
        });
    });
}

void Manager::launchSlots(const Workspace &workspace, const QRect &area, const Reply &reply)
{
    if (workspace.slots.isEmpty()) {
        reply(QStringLiteral("opened"));
        return;
    }

    struct State {
        int remaining;
        int failed = 0;
        QString firstError;
    };
    auto state = std::make_shared<State>();
    state->remaining = workspace.slots.size();
    const int total = workspace.slots.size();
    const int group = ++m_nextOpenId;
    int order = 0;

    for (const Slot &slot : workspace.slots) {
        // Register before launching: the window can in theory arrive before
        // the job reports its pid. The pid is filled in below.
        const int launchId = ++m_nextLaunchId;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        m_matcher.addPending({launchId, 0, slot.app, Layout::slotRect(area, slot.geometry), now, group, order++});
        armExpiry();

        m_launcher->launch(slot.app, slot.args, [this, state, launchId, total, reply](bool ok, qint64 pid, const QString &error) {
            if (ok) {
                m_matcher.setPid(launchId, pid);
            } else {
                qCWarning(VDW_DAEMON) << "launch failed:" << error;
                m_matcher.remove(launchId);
                armExpiry();
                if (state->failed++ == 0) {
                    state->firstError = error;
                }
            }
            if (--state->remaining > 0) {
                return;
            }
            if (state->failed == total) {
                reply(QStringLiteral("error: ") + state->firstError);
            } else {
                reply(QStringLiteral("opened"));
            }
        });
    }
}

void Manager::expirePending()
{
    const auto expired = m_matcher.expire(QDateTime::currentMSecsSinceEpoch());
    for (const PendingLaunch &launch : expired) {
        qCInfo(VDW_DAEMON) << "no window for" << launch.appId << "pid" << launch.pid << "(expired)";
    }
    armExpiry();
}

void Manager::armExpiry()
{
    const std::optional<qint64> deadline = m_matcher.nextDeadlineMs();
    if (!deadline) {
        m_expiryTimer.stop();
        return;
    }
    const qint64 delayMs = std::max<qint64>(0, *deadline - QDateTime::currentMSecsSinceEpoch());
    m_expiryTimer.start(static_cast<int>(std::min<qint64>(delayMs + 100, std::numeric_limits<int>::max())));
}

// The reply of ClaimWindow, and of ReleaseWindow when `windowId` is not empty.
static QString placementJson(const PendingLaunch &launch, const QString &windowId)
{
    const QJsonObject rect{{QStringLiteral("x"), launch.rect.x()},
                           {QStringLiteral("y"), launch.rect.y()},
                           {QStringLiteral("width"), launch.rect.width()},
                           {QStringLiteral("height"), launch.rect.height()}};
    QJsonObject reply{{QStringLiteral("rect"), rect}, {QStringLiteral("group"), launch.group}, {QStringLiteral("order"), launch.order}};
    if (!windowId.isEmpty()) {
        reply.insert(QStringLiteral("id"), windowId);
    }
    return QString::fromUtf8(QJsonDocument(reply).toJson(QJsonDocument::Compact));
}

QString Manager::claimWindow(const QString &windowJson)
{
    const QJsonObject object = QJsonDocument::fromJson(windowJson.toUtf8()).object();
    ObservedWindow window;
    window.pid = static_cast<qint64>(object.value(QLatin1String("pid")).toDouble());
    window.desktopFileName = object.value(QLatin1String("desktopFileName")).toString();
    window.resourceClass = object.value(QLatin1String("resourceClass")).toString();
    window.internalId = object.value(QLatin1String("id")).toString();
    window.timeMs = QDateTime::currentMSecsSinceEpoch();
    window.resizeable = object.value(QLatin1String("resizeable")).toBool(true);

    std::optional<PendingLaunch> newlyHeld;
    const std::optional<PendingLaunch> launch = m_matcher.claim(window, &newlyHeld);
    armExpiry();
    if (newlyHeld) {
        qCInfo(VDW_DAEMON) << "fixed-size window" << window.desktopFileName << "pid" << window.pid << "holds launch" << newlyHeld->launchId
                           << "for its main window";
    }
    if (!launch) {
        qCInfo(VDW_DAEMON) << "window" << window.desktopFileName << "pid" << window.pid << "matches no launch";
        return QString();
    }
    qCInfo(VDW_DAEMON) << "window" << window.desktopFileName << "pid" << window.pid << "claimed by launch" << launch->launchId << launch->rect;
    return placementJson(*launch, QString());
}

QString Manager::releaseWindow(const QString &windowJson)
{
    const QJsonObject object = QJsonDocument::fromJson(windowJson.toUtf8()).object();
    const QString id = object.value(QLatin1String("id")).toString();
    const std::optional<Handover> handover = m_matcher.release(id, QDateTime::currentMSecsSinceEpoch());
    armExpiry();
    if (!handover) {
        return QString();
    }
    qCInfo(VDW_DAEMON) << "window" << handover->windowId << "takes the slot of closed window" << id << "for launch" << handover->launch.launchId
                       << handover->launch.rect;
    return placementJson(handover->launch, handover->windowId);
}

QString Manager::CaptureCurrentDesktop(const QString &name)
{
    qCInfo(VDW_DAEMON) << "CaptureCurrentDesktop" << name;
    if (name.trimmed().isEmpty()) {
        return QStringLiteral("error: a name is required");
    }

    // The reply goes out once the KWin queries are answered and vdwrc is written.
    setDelayedReply(true);
    const QDBusMessage message = this->message();
    const QDBusConnection connection = this->connection();
    const qint64 callerPid = connection.interface()->servicePid(message.service()).value();
    capture(name.trimmed(), callerPid, [message, connection](const QString &result) {
        qCInfo(VDW_DAEMON) << "CaptureCurrentDesktop reply" << result;
        connection.send(message.createReply(result));
    });
    return QString();
}

void Manager::capture(const QString &name, qint64 callerPid, const Reply &reply)
{
    queryArea(m_scripts, [this, name, callerPid, reply](const QRect &area, const QString &areaError) {
        if (!areaError.isEmpty()) {
            reply(QStringLiteral("error: ") + areaError);
            return;
        }
        m_scripts->query(QStringLiteral("list"), [this, name, callerPid, area, reply](const QJsonObject &result, const QString &error) {
            if (!error.isEmpty()) {
                reply(QStringLiteral("error: ") + error);
                return;
            }
            reply(saveCapture(name, area, result.value(QLatin1String("windows")).toArray(), callerPid));
        });
    });
}

QString Manager::saveCapture(const QString &name, const QRect &area, const QJsonArray &windows, qint64 callerPid)
{
    QList<Slot> slots;
    QString icon;
    for (const QJsonValue &value : windows) {
        const QJsonObject window = value.toObject();
        if (callerPid > 0 && static_cast<qint64>(window.value(QLatin1String("pid")).toDouble()) == callerPid) {
            continue;
        }
        const QString appId = window.value(QLatin1String("desktopFileName")).toString();
        const KService::Ptr service = appId.isEmpty() ? KService::Ptr() : KService::serviceByDesktopName(appId);
        if (!service) {
            qCInfo(VDW_DAEMON) << "capture skips" << window.value(QLatin1String("caption")).toString() << "class"
                               << window.value(QLatin1String("resourceClass")).toString() << "(no known app:" << appId << ")";
            continue;
        }
        const QRectF rect(window.value(QLatin1String("x")).toDouble(),
                          window.value(QLatin1String("y")).toDouble(),
                          window.value(QLatin1String("width")).toDouble(),
                          window.value(QLatin1String("height")).toDouble());
        if (slots.isEmpty()) {
            icon = service->icon();
        }
        slots.append({service->desktopEntryName(), QString(), Layout::fractionsOf(area, rect)});
    }
    if (slots.isEmpty()) {
        return QStringLiteral("error: no windows to capture");
    }

    // Start from what is on disk: the KCM may have saved since the last reload.
    m_config = loadConfig(m_configPath);
    QList<Workspace> workspaces = m_config.workspaces();
    QStringList ids;
    for (const Workspace &existing : std::as_const(workspaces)) {
        ids << existing.id;
    }
    Workspace workspace;
    workspace.id = ConfigWriter::slugFromName(name, ids);
    workspace.name = name;
    workspace.icon = icon;
    workspace.order = workspaces.size();
    workspace.slots = slots;
    workspaces.append(workspace);

    const QStringList problems = ConfigWriter::validate(workspaces, m_config.defaultWorkspace());
    if (!problems.isEmpty()) {
        return QStringLiteral("error: ") + problems.first();
    }
    KConfig file(m_configPath.isEmpty() ? QStringLiteral("vdwrc") : m_configPath, KConfig::SimpleConfig);
    ConfigWriter::write(file, workspaces, m_config.defaultWorkspace());
    if (!file.sync()) {
        return QStringLiteral("error: cannot write the config file");
    }
    qCInfo(VDW_DAEMON) << "captured" << slots.size() << "windows as" << workspace.id;
    ReloadConfig();
    return workspace.id;
}

void Manager::ReloadConfig()
{
    m_config = loadConfig(m_configPath);
    m_matcher.setTimeoutMs(m_config.placementTimeoutMs());
    armExpiry();
    qCInfo(VDW_DAEMON) << "reloaded:" << m_config.workspaces().size() << "workspaces," << m_config.warnings().size() << "warnings";
    Q_EMIT configReloaded();
}

ScriptApi::ScriptApi(Manager *manager, KWinScripts *scripts, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
    , m_scripts(scripts)
{
}

QString ScriptApi::ClaimWindow(const QString &windowJson)
{
    return m_manager->claimWindow(windowJson);
}

QString ScriptApi::ReleaseWindow(const QString &windowJson)
{
    return m_manager->releaseWindow(windowJson);
}

void ScriptApi::QueryResult(const QString &requestId, const QString &json)
{
    m_scripts->handleResult(requestId, json);
}

void ScriptApi::CursorPos(int x, int y)
{
    qCInfo(VDW_DAEMON) << "CursorPos" << x << y;
    m_scripts->handleCursor(x, y);
}

DebugApi::DebugApi(GestureController *controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
{
}

void DebugApi::Press(const QString &shortcut)
{
    qCInfo(VDW_DAEMON) << "Debug Press" << shortcut;
    m_controller->press(shortcutFromName(shortcut));
}

void DebugApi::Release(const QString &shortcut)
{
    qCInfo(VDW_DAEMON) << "Debug Release" << shortcut;
    m_controller->release(shortcutFromName(shortcut));
}

void DebugApi::ModifiersHeld(bool held)
{
    qCInfo(VDW_DAEMON) << "Debug ModifiersHeld" << held;
    m_controller->debugSetModifiersHeld(held);
}

} // namespace Vdw
