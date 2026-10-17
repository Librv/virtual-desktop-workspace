// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QObject>
#include <QPoint>
#include <QStringList>

#include <functional>

class QProcess;

namespace Vdw
{

/// The daemon's end of the vdw-ui helper (docs/spikes/S5.md): starts it on
/// demand, queues lines until it says `ready`, and quits it when nothing needs
/// it any more (gesture running, toast up, selects or launches pending).
class UiClient : public QObject
{
    Q_OBJECT

public:
    using LaunchCallback = std::function<void(bool ok, qint64 pid, const QString &error)>;
    using SelectCallback = std::function<void(const QString &id)>;

    explicit UiClient(QObject *parent = nullptr);

    /// Starts the helper and tracks these Qt::KeyboardModifier bits; it
    /// answers with modifiersHeldChanged at once and on every change.
    void trackModifiers(int mask);
    /// `center` x = -1 centers the menu on the screen; items are {id,name,icon}.
    void showMenu(QPoint center, const QJsonArray &items);
    void hideMenu();
    /// `callback` gets the id under the pointer, empty for none. Always
    /// called, asynchronously (empty when there is no helper or it dies).
    void select(SelectCallback callback);
    void showToast(const QString &icon, const QString &text);
    void hideToast();
    /// `callback` is called exactly once, also when the helper cannot run.
    void launch(const QString &app, const QString &args, LaunchCallback callback);
    /// Whether the gesture machine is not Idle.
    void setGestureActive(bool active);

Q_SIGNALS:
    /// Also emitted with false when the helper dies.
    void modifiersHeldChanged(bool held);

private:
    void send(const QString &line, bool start = true);
    void startProcess();
    void readOutput(QProcess *process);
    void handleLine(const QString &line);
    void processEnded(QProcess *process, const QString &why);
    void quitIfIdle();

    QProcess *m_process = nullptr;
    bool m_ready = false;
    QStringList m_queue; ///< lines waiting for `ready`
    QByteArray m_output;
    int m_mask = -1;
    bool m_gestureActive = false;
    bool m_toastVisible = false;
    QList<SelectCallback> m_selects;
    QHash<int, LaunchCallback> m_launches;
    int m_nextLaunch = 1;
};

}
