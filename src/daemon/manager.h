#pragma once

#include "config.h"
#include "matcher.h"

#include <QDBusContext>
#include <QObject>
#include <QRect>
#include <QString>

#include <functional>

namespace Vdw
{

class KWinScripts;
class Launcher;

/// Implements io.github.librv.vdw.Manager on /Manager.
class Manager : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.librv.vdw.Manager")

public:
    /// `configPath` empty means the default `vdwrc`.
    Manager(const QString &configPath, KWinScripts *scripts, Launcher *launcher, QObject *parent = nullptr);

    /// ScriptApi::ClaimWindow: the pixel rect JSON `{x, y, width, height}` for
    /// a window that belongs to a pending launch, else "".
    QString claimWindow(const QString &windowJson);

public Q_SLOTS:
    QString ListWorkspaces();
    QString OpenWorkspace(const QString &id, bool force);
    QString CaptureCurrentDesktop(const QString &name);
    void ReloadConfig();

private:
    using Reply = std::function<void(const QString &)>;

    void openWorkspace(const Workspace &workspace, bool force, const Reply &reply);
    void launchSlots(const Workspace &workspace, const QRect &area, const Reply &reply);
    void expirePending();

    Config m_config;
    QString m_configPath;
    KWinScripts *m_scripts;
    Launcher *m_launcher;
    Matcher m_matcher;
    int m_nextLaunchId = 0;
    bool m_opening = false; ///< a non-forced open is between its checks and its launches
};

/// Implements io.github.librv.vdw.Script on /Script (called by the KWin script).
class ScriptApi : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.librv.vdw.Script")

public:
    ScriptApi(Manager *manager, KWinScripts *scripts, QObject *parent = nullptr);

public Q_SLOTS:
    QString ClaimWindow(const QString &windowJson);
    void WindowPlaced(const QString &json);
    void QueryResult(const QString &requestId, const QString &json);
    void CursorPos(int x, int y);

private:
    Manager *m_manager;
    KWinScripts *m_scripts;
};

/// Implements io.github.librv.vdw.Debug on /Debug (only with --debug).
class DebugApi : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.librv.vdw.Debug")

public Q_SLOTS:
    void Press();
    void Release();
    void ModifiersHeld(bool held);
};

} // namespace Vdw
