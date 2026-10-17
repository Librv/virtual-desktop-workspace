#pragma once

#include "config.h"

#include <QObject>
#include <QString>

namespace Vdw
{

/// Implements io.github.librv.vdw.Manager on /Manager.
class Manager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.librv.vdw.Manager")

public:
    /// `configPath` empty means the default `vdwrc`.
    explicit Manager(const QString &configPath, QObject *parent = nullptr);

public Q_SLOTS:
    QString ListWorkspaces();
    QString OpenWorkspace(const QString &id, bool force);
    QString CaptureCurrentDesktop(const QString &name);
    void ReloadConfig();

private:
    Config m_config;
    QString m_configPath;
};

/// Implements io.github.librv.vdw.Script on /Script (called by the KWin script).
class ScriptApi : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.librv.vdw.Script")

public Q_SLOTS:
    QString ClaimWindow(const QString &windowJson);
    void WindowPlaced(const QString &json);
    void QueryResult(const QString &requestId, const QString &json);
    void CursorPos(int x, int y);
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
