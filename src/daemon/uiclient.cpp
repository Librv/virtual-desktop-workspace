// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "uiclient.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QProcess>
#include <QTimer>

Q_DECLARE_LOGGING_CATEGORY(VDW_DAEMON)

namespace Vdw
{

namespace
{
QString compact(const QJsonObject &object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}
}

UiClient::UiClient(QObject *parent)
    : QObject(parent)
{
}

void UiClient::trackModifiers(int mask)
{
    send(QStringLiteral("mods %1").arg(mask));
}

void UiClient::showMenu(QPoint center, const QJsonArray &items)
{
    send(QStringLiteral("menu %1 %2 %3").arg(center.x()).arg(center.y()).arg(QString::fromUtf8(QJsonDocument(items).toJson(QJsonDocument::Compact))));
}

void UiClient::hideMenu()
{
    send(QStringLiteral("hidemenu"), false);
}

void UiClient::select(SelectCallback callback)
{
    if (!m_process) {
        QTimer::singleShot(0, this, [callback = std::move(callback)] {
            callback(QString());
        });
        return;
    }
    m_selects.append(std::move(callback));
    send(QStringLiteral("select"));
}

void UiClient::showToast(const QString &icon, const QString &text, int chord)
{
    m_toastVisible = true;
    send(QStringLiteral("toast ") + compact({{QStringLiteral("icon"), icon}, {QStringLiteral("text"), text}, {QStringLiteral("chord"), chord}}));
}

void UiClient::hideToast()
{
    m_toastVisible = false;
    send(QStringLiteral("hidetoast"), false);
    quitIfIdle();
}

void UiClient::launch(const QString &app, const QString &args, LaunchCallback callback)
{
    const int n = m_nextLaunch++;
    m_launches.insert(n, std::move(callback));
    send(QStringLiteral("launch %1 ").arg(n) + compact({{QStringLiteral("app"), app}, {QStringLiteral("args"), args}}));
}

void UiClient::setGestureActive(bool active)
{
    m_gestureActive = active;
    quitIfIdle();
}

void UiClient::send(const QString &line, bool start)
{
    if (!m_process) {
        if (!start) {
            return;
        }
        startProcess();
    }
    if (m_ready) {
        m_process->write(line.toUtf8() + '\n');
    } else {
        m_queue.append(line);
    }
}

void UiClient::startProcess()
{
    auto *process = new QProcess(this);
    m_process = process;
    m_ready = false;
    m_output.clear();
    m_queue.clear();
    process->setProcessChannelMode(QProcess::ForwardedErrorChannel);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
        readOutput(process);
    });
    connect(process, &QProcess::finished, this, [this, process] {
        processEnded(process, QStringLiteral("exited"));
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            processEnded(process, QStringLiteral("failed to start"));
        }
    });
    process->start(QCoreApplication::applicationDirPath() + QStringLiteral("/vdw-ui"), {});
}

void UiClient::readOutput(QProcess *process)
{
    if (process != m_process) {
        return;
    }
    m_output += process->readAllStandardOutput();
    qsizetype end;
    while ((end = m_output.indexOf('\n')) >= 0) {
        const QString line = QString::fromUtf8(m_output.left(end));
        m_output.remove(0, end + 1);
        handleLine(line);
        if (process != m_process) {
            return; // a callback made it quit
        }
    }
}

void UiClient::handleLine(const QString &line)
{
    const QString command = line.section(QLatin1Char(' '), 0, 0);
    const QString rest = line.section(QLatin1Char(' '), 1);
    if (command == QLatin1String("ready")) {
        m_ready = true;
        for (const QString &queued : std::as_const(m_queue)) {
            m_process->write(queued.toUtf8() + '\n');
        }
        m_queue.clear();
    } else if (command == QLatin1String("mods")) {
        Q_EMIT modifiersHeldChanged(rest == QLatin1String("1"));
    } else if (command == QLatin1String("selected")) {
        if (!m_selects.isEmpty()) {
            m_selects.takeFirst()(rest == QLatin1String("-") ? QString() : rest);
        }
        quitIfIdle();
    } else if (command == QLatin1String("launched") || command == QLatin1String("failed")) {
        const int n = rest.section(QLatin1Char(' '), 0, 0).toInt();
        const QString value = rest.section(QLatin1Char(' '), 1);
        const LaunchCallback callback = m_launches.take(n);
        if (callback) {
            if (command == QLatin1String("launched")) {
                callback(true, value.toLongLong(), QString());
            } else {
                callback(false, 0, QJsonDocument::fromJson(QStringLiteral("[%1]").arg(value).toUtf8()).array().first().toString());
            }
        }
        quitIfIdle();
    } else {
        qCWarning(VDW_DAEMON) << "vdw-ui: unexpected line" << line;
    }
}

void UiClient::processEnded(QProcess *process, const QString &why)
{
    if (process != m_process) {
        return;
    }
    qCWarning(VDW_DAEMON) << "vdw-ui" << why << "while still needed";
    m_process = nullptr;
    m_ready = false;
    m_queue.clear();
    m_toastVisible = false;
    process->deleteLater();
    // Modifiers count as released and the selection as empty.
    Q_EMIT modifiersHeldChanged(false);
    const QList<SelectCallback> selects = std::exchange(m_selects, {});
    for (const SelectCallback &callback : selects) {
        callback(QString());
    }
    const QHash<int, LaunchCallback> launches = std::exchange(m_launches, {});
    for (const LaunchCallback &callback : launches) {
        callback(false, 0, QStringLiteral("the UI helper stopped"));
    }
}

void UiClient::quitIfIdle()
{
    if (!m_process || m_gestureActive || m_toastVisible || !m_launches.isEmpty() || !m_selects.isEmpty()) {
        return;
    }
    QProcess *process = m_process;
    m_process = nullptr;
    process->disconnect(this);
    connect(process, &QProcess::finished, process, &QObject::deleteLater);
    if (process->state() == QProcess::NotRunning) {
        process->deleteLater(); // failed to start: finished never comes
    } else if (m_ready) {
        process->write("quit\n");
        // Gone with the process (deleteLater on finished) if it quits in time.
        QTimer::singleShot(2000, process, [process] {
            if (process->state() != QProcess::NotRunning) {
                process->kill();
            }
        });
    } else {
        process->kill(); // nothing is on screen before `ready`
    }
    m_ready = false;
    m_queue.clear();
}

}
