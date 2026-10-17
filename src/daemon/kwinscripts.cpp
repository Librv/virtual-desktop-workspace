// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "kwinscripts.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QTimer>

Q_DECLARE_LOGGING_CATEGORY(VDW_DAEMON)

namespace Vdw
{

namespace
{
const QString kwinService = QStringLiteral("org.kde.KWin");
const QString scriptingPath = QStringLiteral("/Scripting");
const QString scriptingInterface = QStringLiteral("org.kde.kwin.Scripting");
const QString placerName = QStringLiteral("vdw-placer");

QDBusMessage scriptingCall(const QString &method, const QVariantList &args)
{
    QDBusMessage message = QDBusMessage::createMethodCall(kwinService, scriptingPath, scriptingInterface, method);
    message.setArguments(args);
    return message;
}

/// Async call; `done` gets the reply (check `type()` for an error).
void callAsync(QObject *context, const QDBusMessage &message, std::function<void(const QDBusMessage &reply)> done)
{
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), context);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, context, [watcher, done = std::move(done)] {
        const QDBusMessage reply = watcher->reply();
        watcher->deleteLater();
        done(reply);
    });
}

QString readResource(const QString &name)
{
    QFile file(QStringLiteral(":/kwin/") + name);
    if (!file.open(QIODevice::ReadOnly)) {
        qCCritical(VDW_DAEMON) << "missing embedded script" << name;
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

bool writeFile(const QString &path, const QString &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCWarning(VDW_DAEMON) << "cannot write" << path << file.errorString();
        return false;
    }
    file.write(content.toUtf8());
    return true;
}
}

KWinScripts::KWinScripts(QObject *parent)
    : QObject(parent)
{
}

void KWinScripts::start()
{
    QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (runtime.isEmpty()) {
        runtime = QDir::tempPath();
    }
    m_dir = runtime + QStringLiteral("/vdw");
    if (!QDir().mkpath(m_dir)) {
        qCWarning(VDW_DAEMON) << "cannot create" << m_dir;
    }

    m_watcher = new QDBusServiceWatcher(kwinService, QDBusConnection::sessionBus(), QDBusServiceWatcher::WatchForRegistration, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, &KWinScripts::loadPlacer);

    if (QDBusConnection::sessionBus().interface()->isServiceRegistered(kwinService)) {
        loadPlacer();
    } else {
        qCWarning(VDW_DAEMON) << "org.kde.KWin is not on the bus yet, the placer loads when it appears";
    }
}

void KWinScripts::loadPlacer()
{
    // A second request while one is in flight runs once more afterwards.
    if (m_placerLoading) {
        m_placerReload = true;
        return;
    }
    m_placerLoading = true;
    const QString path = m_dir + QStringLiteral("/placer.js");
    if (!writeFile(path, readResource(QStringLiteral("placer.js")))) {
        m_placerLoading = false;
        return;
    }
    // Loading a name that is still loaded fails, so unload first.
    callAsync(this, scriptingCall(QStringLiteral("isScriptLoaded"), {placerName}), [this, path](const QDBusMessage &reply) {
        const bool loaded = reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty() && reply.arguments().at(0).toBool();
        const auto load = [this, path] {
            loadScript(path, placerName, [this](bool ok) {
                m_placerLoading = false;
                if (m_placerReload) {
                    m_placerReload = false;
                    loadPlacer();
                    return;
                }
                if (ok) {
                    qCInfo(VDW_DAEMON) << "placer script loaded";
                } else {
                    qCWarning(VDW_DAEMON) << "cannot load the placer script";
                }
            });
        };
        if (!loaded) {
            load();
            return;
        }
        callAsync(this, scriptingCall(QStringLiteral("unloadScript"), {placerName}), [load](const QDBusMessage &) {
            load();
        });
    });
}

void KWinScripts::loadScript(const QString &path, const QString &name, std::function<void(bool ok)> done)
{
    callAsync(this, scriptingCall(QStringLiteral("loadScript"), {path, name}), [this, done](const QDBusMessage &reply) {
        const int id = reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty() ? reply.arguments().at(0).toInt() : -1;
        if (id < 0) {
            done(false);
            return;
        }
        // Not Script<id>.run: KWin hands out ids by counting loaded scripts, so
        // after an unload a new script can get the id of another one and its
        // object path is never registered. Scripting.start() runs every loaded
        // script that is not running yet.
        const QDBusMessage run = scriptingCall(QStringLiteral("start"), {});
        callAsync(this, run, [done](const QDBusMessage &runReply) {
            done(runReply.type() != QDBusMessage::ErrorMessage);
        });
    });
}

void KWinScripts::unloadScript(const QString &name)
{
    callAsync(this, scriptingCall(QStringLiteral("unloadScript"), {name}), [](const QDBusMessage &) { });
}

QString KWinScripts::query(const QString &kind, QueryCallback callback, int timeoutMs)
{
    const QString requestId = QString::number(++m_nextRequest);
    QString source = readResource(kind + QStringLiteral(".js"));
    if (source.isEmpty()) {
        QTimer::singleShot(0, this, [callback = std::move(callback)] {
            callback({}, QStringLiteral("unknown query"));
        });
        return requestId;
    }
    source.replace(QStringLiteral("@REQUEST_ID@"), requestId);
    source.replace(QStringLiteral("@OWN_PID@"), QString::number(QCoreApplication::applicationPid()));

    PendingQuery pending;
    pending.callback = std::move(callback);
    pending.scriptName = QStringLiteral("vdw-query-") + requestId;
    pending.file = m_dir + QStringLiteral("/query-") + requestId + QStringLiteral(".js");
    pending.timeout = new QTimer(this);
    pending.timeout->setSingleShot(true);
    connect(pending.timeout, &QTimer::timeout, this, [this, requestId] {
        finishQuery(requestId, {}, QStringLiteral("timed out waiting for KWin"));
    });
    pending.timeout->start(timeoutMs);
    const QString file = pending.file;
    const QString name = pending.scriptName;
    m_queries.insert(requestId, std::move(pending));

    if (!writeFile(file, source)) {
        finishQuery(requestId, {}, QStringLiteral("cannot write the query script"));
        return requestId;
    }
    loadScript(file, name, [this, requestId](bool ok) {
        if (!ok) {
            finishQuery(requestId, {}, QStringLiteral("cannot load the query script into KWin"));
        }
    });
    return requestId;
}

void KWinScripts::queryCursor(QueryCallback callback, int timeoutMs)
{
    // A request still in flight keeps its place and fails with a timeout.
    m_cursorRequest = query(QStringLiteral("cursor"), std::move(callback), timeoutMs);
}

void KWinScripts::handleCursor(int x, int y)
{
    if (m_cursorRequest.isEmpty()) {
        return;
    }
    finishQuery(m_cursorRequest, QJsonObject{{QStringLiteral("x"), x}, {QStringLiteral("y"), y}}, QString());
}

void KWinScripts::handleResult(const QString &requestId, const QString &json)
{
    if (!m_queries.contains(requestId)) {
        qCWarning(VDW_DAEMON) << "QueryResult for unknown request" << requestId;
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (!doc.isObject()) {
        finishQuery(requestId, {}, QStringLiteral("bad query result: ") + parseError.errorString());
        return;
    }
    finishQuery(requestId, doc.object(), QString());
}

void KWinScripts::finishQuery(const QString &requestId, const QJsonObject &result, const QString &error)
{
    auto it = m_queries.find(requestId);
    if (it == m_queries.end()) {
        return;
    }
    PendingQuery pending = std::move(it.value());
    m_queries.erase(it);
    if (requestId == m_cursorRequest) {
        m_cursorRequest.clear();
    }

    pending.timeout->stop();
    pending.timeout->deleteLater();
    unloadScript(pending.scriptName);
    QFile::remove(pending.file);
    pending.callback(result, error);
}

}
