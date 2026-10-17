#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>

class QDBusMessage;
class QDBusServiceWatcher;
class QTimer;

namespace Vdw
{

/// Loads the KWin scripts through org.kde.kwin.Scripting: the long-lived
/// placer once at startup (and again if KWin restarts), and one-shot query
/// scripts per request (docs/ARCHITECTURE.md, "KWin script").
class KWinScripts : public QObject
{
    Q_OBJECT

public:
    /// `error` is empty on success; `result` is the JSON object the script sent.
    using QueryCallback = std::function<void(const QJsonObject &result, const QString &error)>;

    explicit KWinScripts(QObject *parent = nullptr);

    /// Write the scripts to `$XDG_RUNTIME_DIR/vdw/` and load the placer (now,
    /// and whenever org.kde.KWin appears on the bus).
    void start();

    /// Run the one-shot script `kind` ("empty" or "area"). `callback` is
    /// called exactly once, from the event loop.
    void query(const QString &kind, QueryCallback callback, int timeoutMs = 5000);

    /// Called by ScriptApi::QueryResult.
    void handleResult(const QString &requestId, const QString &json);

private:
    struct PendingQuery {
        QueryCallback callback;
        QTimer *timeout = nullptr;
        QString scriptName;
        QString file;
    };

    void loadPlacer();
    void loadScript(const QString &path, const QString &name, std::function<void(bool ok)> done);
    void unloadScript(const QString &name);
    void finishQuery(const QString &requestId, const QJsonObject &result, const QString &error);

    QString m_dir;
    QDBusServiceWatcher *m_watcher = nullptr;
    QHash<QString, PendingQuery> m_queries;
    int m_nextRequest = 0;
    bool m_placerLoading = false;
    bool m_placerReload = false;
};

}
