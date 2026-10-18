// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include "config.h"
#include "matcher.h"

#include <QDBusContext>
#include <QJsonArray>
#include <QObject>
#include <QRect>
#include <QString>

#include <functional>

namespace Vdw
{

class KWinScripts;
class Launcher;
class GestureController;

/// Implements io.github.librv.vdw.Manager on /Manager.
class Manager : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.librv.vdw.Manager")

public:
    /// `configPath` empty means the default `vdwrc`.
    Manager(const QString &configPath, KWinScripts *scripts, Launcher *launcher, QObject *parent = nullptr);

    using Reply = std::function<void(const QString &)>;

    const Config &config() const
    {
        return m_config;
    }

    /// Open workspace `id`; `reply` gets "opened", "busy" or "error: ..." (once,
    /// from the event loop). Used by OpenWorkspace and the gesture controller.
    void open(const QString &id, bool force, const Reply &reply);

    /// ScriptApi::ClaimWindow: `{rect: {x, y, width, height}, group, order}`
    /// (pixel rect, one group per open, slot index) for a window that belongs
    /// to a pending launch, else "".
    QString claimWindow(const QString &windowJson);

public Q_SLOTS:
    QString ListWorkspaces();
    QString OpenWorkspace(const QString &id, bool force);
    QString CaptureCurrentDesktop(const QString &name);
    void ReloadConfig();

Q_SIGNALS:
    /// The config was re-read (ReloadConfig).
    void configReloaded();

private:
    void openWorkspace(const Workspace &workspace, bool force, const Reply &reply);
    /// Windows of `callerPid` (the page that asked, e.g. System Settings) are left out.
    void capture(const QString &name, qint64 callerPid, const Reply &reply);
    QString saveCapture(const QString &name, const QRect &area, const QJsonArray &windows, qint64 callerPid);
    void launchSlots(const Workspace &workspace, const QRect &area, const Reply &reply);
    void expirePending();

    Config m_config;
    QString m_configPath;
    KWinScripts *m_scripts;
    Launcher *m_launcher;
    Matcher m_matcher;
    int m_nextLaunchId = 0;
    int m_nextOpenId = 0;
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

public:
    explicit DebugApi(GestureController *controller, QObject *parent = nullptr);

public Q_SLOTS:
    void Press(const QString &shortcut);
    void Release(const QString &shortcut);
    void ModifiersHeld(bool held);

private:
    GestureController *m_controller;
};

} // namespace Vdw
