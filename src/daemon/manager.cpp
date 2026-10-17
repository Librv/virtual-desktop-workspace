#include "manager.h"

#include <KConfig>
#include <KSharedConfig>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(VDW_DAEMON, "vdw.daemon")

namespace Vdw
{

namespace
{
Config loadConfig(const QString &path)
{
    if (path.isEmpty()) {
        KSharedConfig::Ptr shared = KSharedConfig::openConfig(QStringLiteral("vdwrc"));
        shared->reparseConfiguration();
        return Config::load(*shared);
    }
    return Config::load(KConfig(path, KConfig::SimpleConfig));
}
}

Manager::Manager(const QString &configPath, QObject *parent)
    : QObject(parent)
    , m_config(loadConfig(configPath))
    , m_configPath(configPath)
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
    if (!Config::isValidId(id) || !m_config.find(id)) {
        return QStringLiteral("error: unknown workspace");
    }
    return QStringLiteral("error: not implemented"); // T2
}

QString Manager::CaptureCurrentDesktop(const QString &name)
{
    qCInfo(VDW_DAEMON) << "CaptureCurrentDesktop" << name;
    return QStringLiteral("error: not implemented"); // T6
}

void Manager::ReloadConfig()
{
    m_config = loadConfig(m_configPath);
    qCInfo(VDW_DAEMON) << "reloaded:" << m_config.workspaces().size() << "workspaces," << m_config.warnings().size() << "warnings";
    // T3 re-registers shortcuts here.
}

QString ScriptApi::ClaimWindow(const QString &windowJson)
{
    qCInfo(VDW_DAEMON) << "ClaimWindow" << windowJson;
    return QString();
}

void ScriptApi::WindowPlaced(const QString &json)
{
    qCInfo(VDW_DAEMON) << "WindowPlaced" << json;
}

void ScriptApi::QueryResult(const QString &requestId, const QString &json)
{
    qCInfo(VDW_DAEMON) << "QueryResult" << requestId << json;
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
