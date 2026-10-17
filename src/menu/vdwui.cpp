// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT
// vdw-ui: the short-lived GUI helper of vdwd (docs/spikes/S5.md). Reads one
// command per line on stdin and answers on stdout.

#include "execargs.h"
#include "radialmenu.h"
#include "toast.h"

#include <KIO/ApplicationLauncherJob>
#include <KModifierKeyInfo>
#include <KService>
#include <KShell>

#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickWindow>
#include <QScreen>
#include <QSocketNotifier>

#include <cstdio>
#include <unistd.h>

namespace
{
void reply(const QString &line)
{
    std::fprintf(stdout, "%s\n", qPrintable(line));
    std::fflush(stdout);
}

QString jsonString(const QString &text)
{
    const QByteArray json = QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}

class Helper : public QObject
{
public:
    Helper()
    {
        connect(&m_modifierInfo, &KModifierKeyInfo::keyPressed, this, [this] {
            reportModifiers(false);
        });
        m_notifier = new QSocketNotifier(STDIN_FILENO, QSocketNotifier::Read, this);
        connect(m_notifier, &QSocketNotifier::activated, this, &Helper::readInput);
    }

private:
    void readInput()
    {
        char chunk[4096];
        const ssize_t n = read(STDIN_FILENO, chunk, sizeof chunk);
        if (n <= 0) {
            m_notifier->setEnabled(false);
            QCoreApplication::quit();
            return;
        }
        m_buffer.append(chunk, n);
        qsizetype end;
        while ((end = m_buffer.indexOf('\n')) >= 0) {
            const QString line = QString::fromUtf8(m_buffer.left(end)).trimmed();
            m_buffer.remove(0, end + 1);
            if (!line.isEmpty()) {
                handle(line);
            }
        }
    }

    void handle(const QString &line)
    {
        const QString command = line.section(QLatin1Char(' '), 0, 0);
        const QString rest = line.section(QLatin1Char(' '), 1);
        if (command == QLatin1String("mods")) {
            m_mask = rest.toInt();
            reportModifiers(true);
        } else if (command == QLatin1String("menu")) {
            QVector<RadialMenu::Item> items;
            for (const QJsonValue &value : QJsonDocument::fromJson(rest.section(QLatin1Char(' '), 2).toUtf8()).array()) {
                const QJsonObject item = value.toObject();
                items.append({item.value(QLatin1String("id")).toString(), item.value(QLatin1String("name")).toString(), item.value(QLatin1String("icon")).toString()});
            }
            QPoint center(rest.section(QLatin1Char(' '), 0, 0).toInt(), rest.section(QLatin1Char(' '), 1, 1).toInt());
            if (center.x() < 0) {
                if (const QScreen *screen = QGuiApplication::primaryScreen()) {
                    center = screen->geometry().center();
                }
            }
            m_menu.show(center, items);
        } else if (command == QLatin1String("hidemenu")) {
            m_menu.hide();
        } else if (command == QLatin1String("select")) {
            const QString id = m_menu.selectedId();
            reply(QStringLiteral("selected ") + (id.isEmpty() ? QStringLiteral("-") : id));
        } else if (command == QLatin1String("toast")) {
            const QJsonObject toast = QJsonDocument::fromJson(rest.toUtf8()).object();
            m_toast.show(toast.value(QLatin1String("icon")).toString(), toast.value(QLatin1String("text")).toString());
        } else if (command == QLatin1String("hidetoast")) {
            m_toast.hide();
        } else if (command == QLatin1String("launch")) {
            const QJsonObject spec = QJsonDocument::fromJson(rest.section(QLatin1Char(' '), 1).toUtf8()).object();
            launch(rest.section(QLatin1Char(' '), 0, 0), spec.value(QLatin1String("app")).toString(), spec.value(QLatin1String("args")).toString());
        } else if (command == QLatin1String("quit")) {
            QCoreApplication::quit();
        }
    }

    /// Mask 0 always reads as held (a shortcut without modifiers never cancels).
    void reportModifiers(bool force)
    {
        bool held = true;
        const struct {
            Qt::KeyboardModifier modifier;
            Qt::Key key;
        } table[] = {{Qt::ControlModifier, Qt::Key_Control}, {Qt::MetaModifier, Qt::Key_Meta}, {Qt::ShiftModifier, Qt::Key_Shift}, {Qt::AltModifier, Qt::Key_Alt}};
        for (const auto &entry : table) {
            if (m_mask & entry.modifier) {
                held = held && m_modifierInfo.isKeyPressed(entry.key);
            }
        }
        if (force || held != m_held) {
            m_held = held;
            reply(QStringLiteral("mods %1").arg(held ? 1 : 0));
        }
    }

    // Same logic as the daemon's launcher before S5.
    void launch(const QString &n, const QString &appId, const QString &args)
    {
        const auto fail = [n](const QString &error) {
            reply(QStringLiteral("failed %1 %2").arg(n, jsonString(error)));
        };
        KService::Ptr original = KService::serviceByDesktopName(appId);
        if (!original) {
            fail(QStringLiteral("unknown app %1").arg(appId));
            return;
        }
        KService::Ptr service(new KService(*original));
        if (!args.trimmed().isEmpty()) {
            KShell::Errors status = KShell::NoError;
            const QStringList split = KShell::splitArgs(args, KShell::AbortOnMeta | KShell::TildeExpand, &status);
            if (status != KShell::NoError) {
                fail(QStringLiteral("cannot parse the arguments of %1: %2").arg(appId, args));
                return;
            }
            // The Exec line is parsed again by KIO: quote each argument so it
            // stays one argument.
            service->setExec(original->exec() + QLatin1Char(' ') + Vdw::quoteExecArgs(split));
        }
        auto *job = new KIO::ApplicationLauncherJob(service, this);
        connect(job, &KJob::result, this, [job, n, fail] {
            if (job->error()) {
                fail(job->errorString());
                return;
            }
            reply(QStringLiteral("launched %1 %2").arg(n).arg(job->pid()));
        });
        job->start();
    }

    RadialMenu m_menu;
    Toast m_toast;
    KModifierKeyInfo m_modifierInfo;
    QSocketNotifier *m_notifier = nullptr;
    QByteArray m_buffer;
    int m_mask = 0;
    bool m_held = true;
};
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("vdw-ui"));
    app.setDesktopFileName(QStringLiteral("vdw-ui"));
    app.setQuitOnLastWindowClosed(false);
    // The GPU renderer keeps Mesa loaded after the first menu (about 35 MB more
    // per process); the menu and toast are simple enough to draw on the CPU.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Software);

    Helper helper;
    reply(QStringLiteral("ready"));
    return app.exec();
}
