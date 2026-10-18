// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

#include "shortcuts.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QLoggingCategory>
#include <QSet>

Q_DECLARE_LOGGING_CATEGORY(VDW_DAEMON)

namespace Vdw
{

namespace
{
const QString componentName = QStringLiteral("vdw");
const QString componentDisplayName = QStringLiteral("Virtual Desktop Workspace");
const QString mainName = QStringLiteral("main");
const QString workspacePrefix = QStringLiteral("open-");
constexpr int mainDefault = (Qt::CTRL | Qt::META | Qt::Key_D).toCombined();

const QString accelService = QStringLiteral("org.kde.kglobalaccel");
const QString accelPath = QStringLiteral("/kglobalaccel");
const QString accelInterface = QStringLiteral("org.kde.KGlobalAccel");
const QString componentPath = QStringLiteral("/component/") + componentName;
const QString componentInterface = QStringLiteral("org.kde.kglobalaccel.Component");

// KGlobalAccel's setShortcutKeys flags, as the client library sends them.
constexpr uint SetPresent = 2;
constexpr uint NoAutoloading = 4;
constexpr uint IsDefault = 8;

/// One key sequence on the wire: `(ai)`, up to four chords.
struct KeySequence {
    QList<int> keys;
};

QDBusArgument &operator<<(QDBusArgument &argument, const KeySequence &sequence)
{
    argument.beginStructure();
    argument << sequence.keys;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, KeySequence &sequence)
{
    argument.beginStructure();
    argument >> sequence.keys;
    argument.endStructure();
    return argument;
}

/// The `a(ai)` for at most one chord (none: an empty array).
QList<KeySequence> sequences(int chord)
{
    return chord == 0 ? QList<KeySequence>() : QList<KeySequence>{{{chord, 0, 0, 0}}};
}

int firstChord(const QList<KeySequence> &keys)
{
    return keys.isEmpty() || keys.first().keys.isEmpty() ? 0 : keys.first().keys.first();
}

QStringList actionId(const QString &name, const QString &text)
{
    return {componentName, name, componentDisplayName, text};
}

QDBusMessage accelCall(const QString &method, const QVariantList &arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(accelService, accelPath, accelInterface, method);
    message.setArguments(arguments);
    return message;
}

QVariantList setKeysArguments(const QString &name, const QString &text, int chord, uint flags)
{
    return {actionId(name, text), QVariant::fromValue(sequences(chord)), flags};
}
}

Shortcuts::Shortcuts(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<KeySequence>();
    qDBusRegisterMetaType<QList<KeySequence>>();
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(accelService,
                componentPath,
                componentInterface,
                QStringLiteral("globalShortcutPressed"),
                this,
                SLOT(onShortcutPressed(QString, QString, qlonglong)));
    bus.connect(accelService,
                componentPath,
                componentInterface,
                QStringLiteral("globalShortcutReleased"),
                this,
                SLOT(onShortcutReleased(QString, QString, qlonglong)));

    bus.connect(accelService, accelPath, accelInterface, QStringLiteral("yourShortcutsChanged"), this, SLOT(onKeysChanged(QDBusMessage)));

    // kglobalacceld restarted: it forgot us, register everything again.
    m_watcher = new QDBusServiceWatcher(accelService, bus, QDBusServiceWatcher::WatchForRegistration, this);
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, [this] {
        for (auto it = m_actions.cbegin(); it != m_actions.cend(); ++it) {
            registerAction(it.key(), it.value());
        }
    });
}

void Shortcuts::onShortcutPressed(const QString &component, const QString &action, qlonglong)
{
    emitFor(component, action, true);
}

void Shortcuts::onShortcutReleased(const QString &component, const QString &action, qlonglong)
{
    emitFor(component, action, false);
}

void Shortcuts::onKeysChanged(const QDBusMessage &message)
{
    const QVariantList arguments = message.arguments();
    if (arguments.size() != 2) {
        return;
    }
    const QStringList id = arguments.at(0).toStringList();
    if (id.size() < 2 || id.first() != componentName || !m_actions.contains(id.at(1))) {
        return;
    }
    QList<KeySequence> keys;
    arguments.at(1).value<QDBusArgument>() >> keys;
    m_chords.insert(id.at(1), firstChord(keys));
    qCDebug(VDW_DAEMON) << "shortcut keys of" << id.at(1) << "changed, chord" << m_chords.value(id.at(1));
}

void Shortcuts::emitFor(const QString &component, const QString &name, bool isPressed)
{
    if (component != componentName || !m_actions.contains(name)) {
        return;
    }
    const Vdw::Shortcut shortcut = name == mainName ? Vdw::Shortcut::main() : Vdw::Shortcut::workspace(name.mid(workspacePrefix.size()));
    qCInfo(VDW_DAEMON) << "shortcut" << name << (isPressed ? "pressed" : "released");
    if (isPressed) {
        Q_EMIT pressed(shortcut);
    } else {
        Q_EMIT released(shortcut);
    }
}

void Shortcuts::registerAction(const QString &name, const Action &action)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.asyncCall(accelCall(QStringLiteral("doRegister"), {actionId(name, action.text)}));
    // Default without autoloading, current with it: a key the user chose in
    // System Settings survives restarts (S1).
    bus.asyncCall(accelCall(QStringLiteral("setShortcutKeys"), setKeysArguments(name, action.text, action.defaultKey, IsDefault | NoAutoloading)));
    // The reply is the effective keys: fills the chord cache.
    auto *watcher = new QDBusPendingCallWatcher(
        bus.asyncCall(accelCall(QStringLiteral("setShortcutKeys"), setKeysArguments(name, action.text, action.defaultKey, SetPresent))),
        this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, name] {
        watcher->deleteLater();
        const QDBusPendingReply<QList<KeySequence>> reply = *watcher;
        if (reply.isError() || !m_actions.contains(name)) {
            return;
        }
        m_chords.insert(name, firstChord(reply.value()));
        qCDebug(VDW_DAEMON) << "shortcut keys of" << name << "registered, chord" << m_chords.value(name);
    });
}

void Shortcuts::renameAction(const QString &name, const QString &text)
{
    // doRegister with an existing action id updates its name and keeps its keys.
    QDBusConnection::sessionBus().asyncCall(accelCall(QStringLiteral("doRegister"), {actionId(name, text)}));
}

void Shortcuts::removeStaleActions()
{
    // Actions of workspaces deleted while vdwd was not running are still
    // registered in kglobalacceld. Never touches "main".
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"),
                                                          QStringLiteral("/component/") + componentName,
                                                          QStringLiteral("org.kde.kglobalaccel.Component"),
                                                          QStringLiteral("shortcutNames"));
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        const QDBusPendingReply<QStringList> reply = *watcher;
        if (reply.isError()) {
            qCWarning(VDW_DAEMON) << "cannot list the registered shortcuts:" << reply.error().message();
            return;
        }
        for (const QString &name : reply.value()) {
            if (!name.startsWith(workspacePrefix) || m_actions.contains(name)) {
                continue;
            }
            qCInfo(VDW_DAEMON) << "removing the stale shortcut" << name;
            QDBusMessage unregister = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"),
                                                                     QStringLiteral("/kglobalaccel"),
                                                                     QStringLiteral("org.kde.KGlobalAccel"),
                                                                     QStringLiteral("unregister"));
            unregister.setArguments({componentName, name});
            QDBusConnection::sessionBus().asyncCall(unregister);
        }
    });
}

void Shortcuts::sync(const Config &config)
{
    if (!m_actions.contains(mainName)) {
        const Action action{QStringLiteral("Open default workspace, hold for the workspace menu"), mainDefault};
        m_actions.insert(mainName, action);
        registerAction(mainName, action);
        removeStaleActions();
    }

    QSet<QString> present;
    for (const Workspace &workspace : config.workspaces()) {
        const QString name = workspacePrefix + workspace.id;
        present.insert(name);
        const QString text = QStringLiteral("Open workspace: ") + workspace.name;
        auto it = m_actions.find(name);
        if (it == m_actions.end()) {
            const Action action{text, 0};
            m_actions.insert(name, action);
            registerAction(name, action);
        } else if (it->text != text) {
            it->text = text;
            renameAction(name, text);
        }
    }

    // Only the shortcuts of deleted workspaces go; the others keep their key.
    for (auto it = m_actions.begin(); it != m_actions.end();) {
        if (it.key() == mainName || present.contains(it.key())) {
            ++it;
            continue;
        }
        qCInfo(VDW_DAEMON) << "removing the shortcut of deleted workspace" << it.key();
        QDBusConnection::sessionBus().asyncCall(accelCall(QStringLiteral("unregister"), {componentName, it.key()}));
        m_chords.remove(it.key());
        it = m_actions.erase(it);
    }
}

int Shortcuts::chord(const Vdw::Shortcut &shortcut) const
{
    const QString name = shortcut.isMain ? mainName : workspacePrefix + shortcut.workspaceId;
    return m_chords.value(name, 0);
}

}
