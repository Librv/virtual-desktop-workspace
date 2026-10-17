#include "shortcuts.h"

#include "shortcutkeys.h"

#include <KGlobalAccel>
#include <KModifierKeyInfo>

#include <QAction>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
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
const QKeySequence mainDefault(Qt::CTRL | Qt::META | Qt::Key_D);
}

Shortcuts::Shortcuts(QObject *parent)
    : QObject(parent)
    , m_modifierInfo(new KModifierKeyInfo(this))
{
    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutActiveChanged, this, [this](QAction *action, bool active) {
        vdw::Shortcut shortcut;
        if (action == m_main) {
            shortcut = vdw::Shortcut::main();
        } else {
            const QString id = m_workspaces.key(action);
            if (id.isEmpty()) {
                return;
            }
            shortcut = vdw::Shortcut::workspace(id);
        }
        qCInfo(VDW_DAEMON) << "shortcut" << action->objectName() << (active ? "pressed" : "released");
        if (active) {
            Q_EMIT pressed(shortcut);
        } else {
            Q_EMIT released(shortcut);
        }
    });
    // The modifiers to watch come from the main shortcut's current sequence,
    // which the user can change in System Settings.
    connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutChanged, this, [this](QAction *action, const QKeySequence &) {
        if (action == m_main) {
            updateModifiers();
        }
    });
    connect(m_modifierInfo, &KModifierKeyInfo::keyPressed, this, &Shortcuts::updateModifiers);
}

QAction *Shortcuts::registerAction(const QString &name, const QString &text, const QKeySequence &defaultKey)
{
    auto *action = new QAction(text, this);
    action->setObjectName(name);
    action->setProperty("componentName", componentName);
    action->setProperty("componentDisplayName", componentDisplayName);
    // Only active changes drive the machine; auto-repeat would add triggers.
    action->setAutoRepeat(false);
    const QList<QKeySequence> defaults = defaultKey.isEmpty() ? QList<QKeySequence>() : QList<QKeySequence>{defaultKey};
    KGlobalAccel *accel = KGlobalAccel::self();
    // Default without autoloading, current with it: a key the user chose in
    // System Settings survives restarts (S1).
    accel->setDefaultShortcut(action, defaults, KGlobalAccel::NoAutoloading);
    accel->setShortcut(action, defaults, KGlobalAccel::Autoloading);
    return action;
}

void Shortcuts::renameAction(const QString &name, const QString &text)
{
    // KGlobalAccel only sends the friendly name when an action is first
    // registered, so tell kglobalacceld about the new one directly: doRegister
    // with an existing action id updates its name and keeps its keys.
    QDBusMessage message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.kglobalaccel"),
                                                          QStringLiteral("/kglobalaccel"),
                                                          QStringLiteral("org.kde.KGlobalAccel"),
                                                          QStringLiteral("doRegister"));
    message.setArguments({QStringList{componentName, name, componentDisplayName, text}});
    QDBusConnection::sessionBus().asyncCall(message);
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
            if (!name.startsWith(workspacePrefix) || m_workspaces.contains(name.mid(workspacePrefix.size()))) {
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
    if (!m_main) {
        m_main = registerAction(mainName, QStringLiteral("Open default workspace, hold for the workspace menu"), mainDefault);
        updateModifiers();
        removeStaleActions();
    }

    QSet<QString> present;
    for (const Workspace &workspace : config.workspaces()) {
        present.insert(workspace.id);
        // The visible name is what kglobalaccel shows; QAction text needs "&"
        // doubled for QAction.
        const QString visible = QStringLiteral("Open workspace: ") + workspace.name;
        QString text = visible;
        text.replace(QLatin1Char('&'), QStringLiteral("&&"));
        if (QAction *existing = m_workspaces.value(workspace.id)) {
            if (existing->text() != text) {
                existing->setText(text);
                renameAction(existing->objectName(), visible);
            }
        } else {
            QAction *action = registerAction(workspacePrefix + workspace.id, text, QKeySequence());
            m_workspaces.insert(workspace.id, action);
            if (text != visible) {
                // KGlobalAccel drops every "&" of the text on registration, so
                // send the visible name the same way a rename does.
                renameAction(action->objectName(), visible);
            }
        }
    }

    // Only the shortcuts of deleted workspaces go; the others keep their key.
    for (auto it = m_workspaces.begin(); it != m_workspaces.end();) {
        if (present.contains(it.key())) {
            ++it;
            continue;
        }
        qCInfo(VDW_DAEMON) << "removing the shortcut of deleted workspace" << it.key();
        KGlobalAccel::self()->removeAllShortcuts(it.value());
        delete it.value();
        it = m_workspaces.erase(it);
    }
}

QKeySequence Shortcuts::sequence(const vdw::Shortcut &shortcut) const
{
    QAction *action = shortcut.isMain ? m_main : m_workspaces.value(shortcut.workspaceId);
    if (!action) {
        return {};
    }
    const QList<QKeySequence> keys = KGlobalAccel::self()->shortcut(action);
    return keys.isEmpty() ? QKeySequence() : keys.first();
}

void Shortcuts::updateModifiers()
{
    const QList<Qt::Key> keys = ShortcutKeys::modifierKeys(sequence(vdw::Shortcut::main()));
    bool held = true; // a shortcut without modifiers never cancels a confirm
    for (const Qt::Key key : keys) {
        held = held && m_modifierInfo->isKeyPressed(key);
    }
    if (held != m_modifiersHeld) {
        m_modifiersHeld = held;
        Q_EMIT modifiersHeldChanged(held);
    }
}

}
