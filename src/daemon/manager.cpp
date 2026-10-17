#include "manager.h"

#include "kwinscripts.h"
#include "launcher.h"
#include "layout.h"

#include <KConfig>
#include <KSharedConfig>

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QTimer>

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
}

Manager::Manager(const QString &configPath, KWinScripts *scripts, Launcher *launcher, QObject *parent)
    : QObject(parent)
    , m_config(loadConfig(configPath))
    , m_configPath(configPath)
    , m_scripts(scripts)
    , m_launcher(launcher)
    , m_matcher(m_config.placementTimeoutMs())
{
    qCInfo(VDW_DAEMON) << "loaded" << m_config.workspaces().size() << "workspaces," << m_config.warnings().size() << "warnings";
}

QString Manager::ListWorkspaces()
{
    return QString::fromUtf8(m_config.toJson());
}

QString Manager::OpenWorkspace(const QString &id, bool force)
{
    qCInfo(VDW_DAEMON) << "OpenWorkspace" << id << "force" << force;
    const Workspace *workspace = Config::isValidId(id) ? m_config.find(id) : nullptr;
    if (!workspace) {
        return QStringLiteral("error: unknown workspace");
    }

    // The reply goes out once the launches have started (see openWorkspace).
    setDelayedReply(true);
    const QDBusMessage message = this->message();
    const QDBusConnection connection = this->connection();
    openWorkspace(*workspace, force, [message, connection](const QString &result) {
        qCInfo(VDW_DAEMON) << "OpenWorkspace reply" << result;
        connection.send(message.createReply(result));
    });
    return QString();
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
        m_scripts->query(QStringLiteral("area"), [this, workspace, force, reply](const QJsonObject &area, const QString &areaError) {
            if (!areaError.isEmpty()) {
                reply(QStringLiteral("error: ") + areaError);
                return;
            }
            const QRect rect(area.value(QLatin1String("x")).toInt(),
                             area.value(QLatin1String("y")).toInt(),
                             area.value(QLatin1String("width")).toInt(),
                             area.value(QLatin1String("height")).toInt());
            if (rect.isEmpty()) {
                reply(QStringLiteral("error: KWin reported an empty usable area"));
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

    for (const Slot &slot : workspace.slots) {
        // Register before launching: the window can in theory arrive before
        // the job reports its pid. The pid is filled in below.
        const int launchId = ++m_nextLaunchId;
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        m_matcher.addPending({launchId, 0, slot.app, Layout::slotRect(area, slot.geometry), now});
        QTimer::singleShot(m_matcher.timeoutMs() + 100, this, &Manager::expirePending);

        m_launcher->launch(slot.app, slot.args, [this, state, launchId, total, reply](bool ok, qint64 pid, const QString &error) {
            if (ok) {
                m_matcher.setPid(launchId, pid);
            } else {
                qCWarning(VDW_DAEMON) << "launch failed:" << error;
                m_matcher.remove(launchId);
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

    const std::optional<PendingLaunch> launch = m_matcher.claim(window);
    if (!launch) {
        qCInfo(VDW_DAEMON) << "window" << window.desktopFileName << "pid" << window.pid << "matches no launch";
        return QString();
    }
    qCInfo(VDW_DAEMON) << "window" << window.desktopFileName << "pid" << window.pid << "claimed by launch" << launch->launchId << launch->rect;
    const QJsonObject rect{{QStringLiteral("x"), launch->rect.x()},
                           {QStringLiteral("y"), launch->rect.y()},
                           {QStringLiteral("width"), launch->rect.width()},
                           {QStringLiteral("height"), launch->rect.height()}};
    return QString::fromUtf8(QJsonDocument(rect).toJson(QJsonDocument::Compact));
}

QString Manager::CaptureCurrentDesktop(const QString &name)
{
    qCInfo(VDW_DAEMON) << "CaptureCurrentDesktop" << name;
    return QStringLiteral("error: not implemented"); // T6
}

void Manager::ReloadConfig()
{
    m_config = loadConfig(m_configPath);
    m_matcher.setTimeoutMs(m_config.placementTimeoutMs());
    qCInfo(VDW_DAEMON) << "reloaded:" << m_config.workspaces().size() << "workspaces," << m_config.warnings().size() << "warnings";
    // T3 re-registers shortcuts here.
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

void ScriptApi::WindowPlaced(const QString &json)
{
    qCInfo(VDW_DAEMON) << "WindowPlaced" << json;
}

void ScriptApi::QueryResult(const QString &requestId, const QString &json)
{
    m_scripts->handleResult(requestId, json);
}

void ScriptApi::CursorPos(int x, int y)
{
    qCInfo(VDW_DAEMON) << "CursorPos" << x << y;
}

void DebugApi::Press()
{
    qCInfo(VDW_DAEMON) << "Debug Press";
}

void DebugApi::Release()
{
    qCInfo(VDW_DAEMON) << "Debug Release";
}

void DebugApi::ModifiersHeld(bool held)
{
    qCInfo(VDW_DAEMON) << "Debug ModifiersHeld" << held;
}

} // namespace Vdw
