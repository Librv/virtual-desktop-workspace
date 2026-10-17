// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT
// S2 spike: radial menu overlay prototype. Throwaway code.
//
// A full-screen transparent LayerShellQt surface on the overlay layer, drawn
// in QML with Kirigami theme colors. Control it with lines on stdin or with
// D-Bus calls on the session bus (service org.kde.vdw.S2Spike, path /S2):
//   show     map the overlay and wait for the pointer position
//   release  print the selected slice and hide (stands in for "shortcut released")
//   cancel   hide without selecting
//   hide     hide
//   status   print state
//   quit
// With --shortcut SEQ it also registers a KGlobalAccel action: press shows
// the overlay after --hold-ms, release prints the selection.
//
// Output lines start with "S2 " so tests can grep them.
#include "radialmath.h"

#include <KGlobalAccel>
#include <LayerShellQt/Window>

#include <QAction>
#include <QCommandLineParser>
#include <QCursor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QSocketNotifier>
#include <QStandardPaths>
#include <QTimer>

#include <cstdio>
#include <numbers>
#include <unistd.h>

static void out(const QString &line)
{
    std::printf("S2 %s\n", qPrintable(line));
    std::fflush(stdout);
}

// State the QML scene binds to.
class Menu : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList slices READ slices CONSTANT)
    Q_PROPERTY(bool centerKnown READ centerKnown NOTIFY centerChanged)
    Q_PROPERTY(qreal centerX READ centerX NOTIFY centerChanged)
    Q_PROPERTY(qreal centerY READ centerY NOTIFY centerChanged)
    Q_PROPERTY(qreal pointerX READ pointerX NOTIFY pointerChanged)
    Q_PROPERTY(qreal pointerY READ pointerY NOTIFY pointerChanged)
    Q_PROPERTY(int selected READ selected NOTIFY selectedChanged)
    Q_PROPERTY(qreal deadZone READ deadZone CONSTANT)
    Q_PROPERTY(bool showDebug MEMBER m_showDebug CONSTANT)

public:
    explicit Menu(const QVariantList &slices, qreal deadZone, bool showDebug)
        : m_slices(slices)
        , m_deadZone(deadZone)
        , m_showDebug(showDebug)
    {
    }

    QVariantList slices() const
    {
        return m_slices;
    }
    bool centerKnown() const
    {
        return m_centerKnown;
    }
    qreal centerX() const
    {
        return m_center.x();
    }
    qreal centerY() const
    {
        return m_center.y();
    }
    qreal pointerX() const
    {
        return m_pointer.x();
    }
    qreal pointerY() const
    {
        return m_pointer.y();
    }
    int selected() const
    {
        return m_selected;
    }
    qreal deadZone() const
    {
        return m_deadZone;
    }
    bool hasPointer() const
    {
        return m_hasPointer;
    }

    void reset(QElapsedTimer *since)
    {
        m_since = since;
        m_centerKnown = false;
        m_hasPointer = false;
        m_eventCount = 0;
        m_selected = -1;
        Q_EMIT centerChanged();
        Q_EMIT selectedChanged();
    }

    void setCenter(QPointF c, const QString &source)
    {
        if (m_centerKnown) {
            return;
        }
        m_center = c;
        m_centerKnown = true;
        out(QStringLiteral("center %1,%2 from %3 after %4 ms").arg(c.x()).arg(c.y()).arg(source).arg(elapsed()));
        Q_EMIT centerChanged();
        updateSelection();
    }

    // Slice center angle in degrees, clockwise from up.
    Q_INVOKABLE qreal sliceAngle(int index) const
    {
        return RadialMath::sliceCenterAngle(index, m_slices.size()) * 180.0 / std::numbers::pi;
    }

    Q_INVOKABLE void pointerEvent(qreal x, qreal y, const QString &kind)
    {
        ++m_eventCount;
        if (!m_hasPointer || m_eventCount <= 3 || m_verbose) {
            out(QStringLiteral("pointer %1 %2,%3 after %4 ms").arg(kind).arg(x).arg(y).arg(elapsed()));
        }
        m_hasPointer = true;
        m_pointer = QPointF(x, y);
        Q_EMIT pointerChanged();
        setCenter(m_pointer, QStringLiteral("first pointer event (%1)").arg(kind));
        updateSelection();
    }

    Q_INVOKABLE void keyEvent(int key, const QString &text)
    {
        out(QStringLiteral("key event in overlay: key=0x%1 text='%2'").arg(key, 0, 16).arg(text));
        if (key == Qt::Key_Escape) {
            Q_EMIT escapePressed();
        }
    }

    QString selectedName() const
    {
        if (m_selected < 0 || m_selected >= m_slices.size()) {
            return {};
        }
        return m_slices.at(m_selected).toMap().value(QStringLiteral("name")).toString();
    }

    bool m_verbose = false;

Q_SIGNALS:
    void centerChanged();
    void pointerChanged();
    void selectedChanged();
    void escapePressed();

private:
    qint64 elapsed() const
    {
        return m_since && m_since->isValid() ? m_since->elapsed() : -1;
    }

    void updateSelection()
    {
        if (!m_centerKnown || !m_hasPointer) {
            return;
        }
        const int s = RadialMath::sliceAt(m_center, m_pointer, m_slices.size(), m_deadZone);
        if (s != m_selected) {
            m_selected = s;
            out(QStringLiteral("highlight %1 %2").arg(s).arg(s < 0 ? QStringLiteral("(dead zone)") : selectedName()));
            Q_EMIT selectedChanged();
        }
    }

    QVariantList m_slices;
    qreal m_deadZone;
    bool m_showDebug;
    QElapsedTimer *m_since = nullptr;
    bool m_centerKnown = false;
    bool m_hasPointer = false;
    int m_eventCount = 0;
    QPointF m_center;
    QPointF m_pointer;
    int m_selected = -1;
};

struct Options {
    bool createOnDemand = false;
    LayerShellQt::Window::KeyboardInteractivity kbd = LayerShellQt::Window::KeyboardInteractivityNone;
    QString centerSource = QStringLiteral("enter"); // enter, kwin, qcursor
    LayerShellQt::Window::Layer layer = LayerShellQt::Window::LayerOverlay;
};

class Spike : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.vdw.S2Spike")

public:
    Spike(Menu *menu, const Options &opts)
        : m_menu(menu)
        , m_opts(opts)
    {
        connect(menu, &Menu::escapePressed, this, [this] {
            out(QStringLiteral("CANCELLED (Esc)"));
            Hide();
        });
        if (!m_opts.createOnDemand) {
            createView();
        }
    }

public Q_SLOTS:
    void Show()
    {
        if (m_shown) {
            return;
        }
        m_shown = true;
        m_timer.start();
        m_menu->reset(&m_timer);
        if (!m_view) {
            createView();
            out(QStringLiteral("view created on demand in %1 ms").arg(m_timer.elapsed()));
        }
        m_firstFrame = true;
        m_view->show();
        out(QStringLiteral("show() returned after %1 ms").arg(m_timer.elapsed()));
        if (m_opts.centerSource == QLatin1String("qcursor")) {
            m_menu->setCenter(QCursor::pos(), QStringLiteral("QCursor::pos()"));
        } else if (m_opts.centerSource == QLatin1String("kwin")) {
            askKWinForCursor();
        }
        QTimer::singleShot(500, this, [this] {
            if (m_shown && !m_menu->hasPointer()) {
                out(QStringLiteral("no pointer event 500 ms after show"));
            }
        });
    }

    QString Release()
    {
        QString result;
        if (!m_shown) {
            result = QStringLiteral("NOT SHOWN");
        } else if (!m_menu->hasPointer() && !m_menu->centerKnown()) {
            result = QStringLiteral("CANCELLED (no pointer position)");
        } else if (m_menu->selected() < 0) {
            result = QStringLiteral("CANCELLED (dead zone)");
        } else {
            result = QStringLiteral("SELECTED %1 %2").arg(m_menu->selected()).arg(m_menu->selectedName());
        }
        out(result);
        Hide();
        return result;
    }

    void Cancel()
    {
        out(QStringLiteral("CANCELLED (cancel)"));
        Hide();
    }

    void Hide()
    {
        if (!m_shown) {
            return;
        }
        m_shown = false;
        if (m_opts.createOnDemand) {
            m_view->deleteLater();
            m_view = nullptr;
        } else {
            m_view->hide();
        }
        out(QStringLiteral("hidden"));
    }

    // Called back by the KWin script (callDBus) with workspace.cursorPos.
    // KWin's callDBus marshals JS numbers as int32 ('ii') when they are
    // whole, so the int overload is the one that is hit.
    void CursorPos(int x, int y)
    {
        CursorPos(double(x), double(y));
    }

    void CursorPos(double x, double y)
    {
        out(QStringLiteral("kwin workspace.cursorPos = %1,%2 after %3 ms").arg(x).arg(y).arg(m_timer.elapsed()));
        if (m_shown) {
            m_menu->setCenter(QPointF(x, y), QStringLiteral("KWin workspace.cursorPos"));
        }
        QDBusMessage unload = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                             QStringLiteral("/Scripting"),
                                                             QStringLiteral("org.kde.kwin.Scripting"),
                                                             QStringLiteral("unloadScript"));
        unload << m_scriptName;
        QDBusConnection::sessionBus().asyncCall(unload);
    }

    QString Status()
    {
        const QString s = QStringLiteral("shown=%1 center=%2 %3,%4 pointer=%5 %6,%7 selected=%8 view=%9 viewSize=%10x%11 active=%12")
                              .arg(m_shown)
                              .arg(m_menu->centerKnown())
                              .arg(m_menu->centerX())
                              .arg(m_menu->centerY())
                              .arg(m_menu->hasPointer())
                              .arg(m_menu->pointerX())
                              .arg(m_menu->pointerY())
                              .arg(m_menu->selected())
                              .arg(m_view != nullptr)
                              .arg(m_view ? m_view->width() : 0)
                              .arg(m_view ? m_view->height() : 0)
                              .arg(m_view && m_view->isActive());
        out(s);
        return s;
    }

    void Quit()
    {
        QCoreApplication::quit();
    }

private:
    void createView()
    {
        m_view = new QQuickView;
        m_view->setColor(Qt::transparent);
        m_view->setResizeMode(QQuickView::SizeRootObjectToView);
        m_view->setTitle(QStringLiteral("vdw S2 radial overlay"));
        if (QScreen *screen = QGuiApplication::primaryScreen()) {
            m_view->resize(screen->size());
        }
        m_view->rootContext()->setContextProperty(QStringLiteral("menu"), m_menu);
        m_view->setSource(QUrl(QStringLiteral("qrc:/Overlay.qml")));
        if (m_view->status() == QQuickView::Error) {
            for (const auto &e : m_view->errors()) {
                out(QStringLiteral("QML error: %1").arg(e.toString()));
            }
        }

        // Must happen before the platform window is created (first show).
        auto *lsw = LayerShellQt::Window::get(m_view);
        lsw->setLayer(m_opts.layer);
        lsw->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                                                      | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
        lsw->setExclusiveZone(-1);
        lsw->setKeyboardInteractivity(m_opts.kbd);
        lsw->setActivateOnShow(m_opts.kbd != LayerShellQt::Window::KeyboardInteractivityNone);
        lsw->setScope(QStringLiteral("vdw-s2-radial"));

        connect(m_view, &QQuickWindow::frameSwapped, this, [this] {
            if (m_firstFrame) {
                m_firstFrame = false;
                out(QStringLiteral("first frame swapped %1 ms after Show, window %2x%3 at %4,%5")
                        .arg(m_timer.elapsed())
                        .arg(m_view->width())
                        .arg(m_view->height())
                        .arg(m_view->x())
                        .arg(m_view->y()));
            }
        });
        connect(m_view, &QWindow::activeChanged, this, [this] {
            if (m_view) {
                out(QStringLiteral("overlay window active=%1 (has keyboard focus)").arg(m_view->isActive()));
            }
        });
    }

    void askKWinForCursor()
    {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        const QString path = dir + QStringLiteral("/vdw-s2-cursor.js");
        QFile f(path);
        if (!f.exists()) {
            if (!f.open(QIODevice::WriteOnly)) {
                out(QStringLiteral("cannot write %1").arg(path));
                return;
            }
            f.write("callDBus('org.kde.vdw.S2Spike', '/S2', 'org.kde.vdw.S2Spike', 'CursorPos',"
                    " workspace.cursorPos.x, workspace.cursorPos.y);\n");
            f.close();
        }
        m_scriptName = QStringLiteral("vdw-s2-cursor-%1").arg(++m_scriptSerial);
        QDBusMessage load = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                           QStringLiteral("/Scripting"),
                                                           QStringLiteral("org.kde.kwin.Scripting"),
                                                           QStringLiteral("loadScript"));
        load << path << m_scriptName;
        const QDBusMessage reply = QDBusConnection::sessionBus().call(load);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            out(QStringLiteral("loadScript failed: %1").arg(reply.errorMessage()));
            return;
        }
        const int id = reply.arguments().value(0).toInt();
        QDBusMessage run = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
                                                          QStringLiteral("/Scripting/Script%1").arg(id),
                                                          QStringLiteral("org.kde.kwin.Script"),
                                                          QStringLiteral("run"));
        QDBusConnection::sessionBus().asyncCall(run);
        out(QStringLiteral("KWin cursor script %1 loaded and started after %2 ms").arg(id).arg(m_timer.elapsed()));
    }

    Menu *m_menu;
    Options m_opts;
    QQuickView *m_view = nullptr;
    bool m_shown = false;
    bool m_firstFrame = false;
    QElapsedTimer m_timer;
    QString m_scriptName;
    int m_scriptSerial = 0;
};

static QVariantList parseSlices(const QString &spec)
{
    QVariantList list;
    for (const QString &item : spec.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const int colon = item.lastIndexOf(QLatin1Char(':'));
        QVariantMap m;
        m.insert(QStringLiteral("name"), colon < 0 ? item : item.left(colon));
        m.insert(QStringLiteral("icon"), colon < 0 ? QStringLiteral("virtual-desktops") : item.mid(colon + 1));
        list.append(m);
    }
    return list;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("vdw-s2-spike"));
    app.setDesktopFileName(QStringLiteral("vdw-s2-spike"));
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption slicesOpt(QStringLiteral("slices"),
                                 QStringLiteral("Comma-separated Name:icon list; empty for the no-workspaces hint."),
                                 QStringLiteral("list"),
                                 QStringLiteral("3 Konsoles:utilities-terminal,Web:internet-web-browser,Files:system-file-manager,"
                                                "Mail:internet-mail,Code:applications-development"));
    QCommandLineOption modeOpt(QStringLiteral("mode"), QStringLiteral("keep (create once, hide/show) or create (new surface per show)."),
                               QStringLiteral("mode"), QStringLiteral("keep"));
    QCommandLineOption kbdOpt(QStringLiteral("kbd"), QStringLiteral("Keyboard interactivity: none, exclusive, ondemand."),
                              QStringLiteral("kbd"), QStringLiteral("none"));
    QCommandLineOption centerOpt(QStringLiteral("center"), QStringLiteral("Where the center comes from: enter, kwin, qcursor."),
                                 QStringLiteral("source"), QStringLiteral("enter"));
    QCommandLineOption deadOpt(QStringLiteral("dead-zone"), QStringLiteral("Dead zone radius in px."), QStringLiteral("px"), QStringLiteral("48"));
    QCommandLineOption shortcutOpt(QStringLiteral("shortcut"), QStringLiteral("Register a global shortcut (e.g. Ctrl+Meta+F9); hold shows, release selects."),
                                   QStringLiteral("seq"));
    QCommandLineOption holdOpt(QStringLiteral("hold-ms"), QStringLiteral("Hold threshold for --shortcut."), QStringLiteral("ms"), QStringLiteral("250"));
    QCommandLineOption layerOpt(QStringLiteral("layer"), QStringLiteral("Layer: overlay or top (to compare KWin's layer values)."), QStringLiteral("layer"), QStringLiteral("overlay"));
    QCommandLineOption verboseOpt(QStringLiteral("verbose"), QStringLiteral("Print every pointer event."));
    QCommandLineOption debugOpt(QStringLiteral("debug-overlay"), QStringLiteral("Draw the pointer position and a frame around the surface."));
    parser.addOptions({slicesOpt, modeOpt, kbdOpt, centerOpt, deadOpt, shortcutOpt, holdOpt, layerOpt, verboseOpt, debugOpt});
    parser.process(app);

    Options opts;
    opts.createOnDemand = parser.value(modeOpt) == QLatin1String("create");
    const QString kbd = parser.value(kbdOpt);
    opts.kbd = kbd == QLatin1String("exclusive") ? LayerShellQt::Window::KeyboardInteractivityExclusive
        : kbd == QLatin1String("ondemand")       ? LayerShellQt::Window::KeyboardInteractivityOnDemand
                                                 : LayerShellQt::Window::KeyboardInteractivityNone;
    opts.centerSource = parser.value(centerOpt);
    if (parser.value(layerOpt) == QLatin1String("top")) {
        opts.layer = LayerShellQt::Window::LayerTop;
    }

    Menu menu(parseSlices(parser.value(slicesOpt)), parser.value(deadOpt).toDouble(), parser.isSet(debugOpt));
    menu.m_verbose = parser.isSet(verboseOpt);
    Spike spike(&menu, opts);

    out(QStringLiteral("platform=%1 WAYLAND_DISPLAY=%2 mode=%3 kbd=%4 center=%5 slices=%6")
            .arg(QGuiApplication::platformName(), qEnvironmentVariable("WAYLAND_DISPLAY"), parser.value(modeOpt), kbd, opts.centerSource)
            .arg(menu.slices().size()));

    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(QStringLiteral("/S2"), &spike, QDBusConnection::ExportAllSlots)
        || !bus.registerService(QStringLiteral("org.kde.vdw.S2Spike"))) {
        out(QStringLiteral("D-Bus registration failed: %1").arg(bus.lastError().message()));
    } else {
        out(QStringLiteral("D-Bus: org.kde.vdw.S2Spike /S2 on %1").arg(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS")));
    }

    // Optional global shortcut: the real gesture, for the live check.
    QAction *action = nullptr;
    QTimer holdTimer;
    holdTimer.setSingleShot(true);
    holdTimer.setInterval(parser.value(holdOpt).toInt());
    QObject::connect(&holdTimer, &QTimer::timeout, &spike, [&] {
        out(QStringLiteral("hold threshold reached, showing menu"));
        spike.Show();
    });
    QElapsedTimer pressTimer;
    if (parser.isSet(shortcutOpt)) {
        action = new QAction(&app);
        action->setObjectName(QStringLiteral("s2-radial-hold"));
        action->setText(QStringLiteral("S2 spike: hold for radial menu"));
        const QKeySequence seq(parser.value(shortcutOpt));
        KGlobalAccel::self()->setDefaultShortcut(action, {seq});
        const bool ok = KGlobalAccel::self()->setShortcut(action, {seq}, KGlobalAccel::NoAutoloading);
        out(QStringLiteral("global shortcut %1 registered: %2 (now %3)")
                .arg(seq.toString())
                .arg(ok)
                .arg(QKeySequence::listToString(KGlobalAccel::self()->shortcut(action))));
        QObject::connect(KGlobalAccel::self(), &KGlobalAccel::globalShortcutActiveChanged, &spike, [&](QAction *a, bool active) {
            if (a != action) {
                return;
            }
            if (active) {
                pressTimer.start();
                out(QStringLiteral("shortcut PRESSED"));
                holdTimer.start();
            } else {
                out(QStringLiteral("shortcut RELEASED after %1 ms").arg(pressTimer.elapsed()));
                if (holdTimer.isActive()) {
                    holdTimer.stop();
                    out(QStringLiteral("TAP (released before hold threshold)"));
                } else {
                    spike.Release();
                }
            }
        });
    }

    // stdin commands.
    QSocketNotifier stdinNotifier(STDIN_FILENO, QSocketNotifier::Read);
    QObject::connect(&stdinNotifier, &QSocketNotifier::activated, &spike, [&] {
        char buf[256];
        const ssize_t n = read(STDIN_FILENO, buf, sizeof buf - 1);
        if (n <= 0) {
            stdinNotifier.setEnabled(false);
            return;
        }
        buf[n] = 0;
        for (const QString &raw : QString::fromLocal8Bit(buf).split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QString cmd = raw.trimmed();
            if (cmd == QLatin1String("show")) {
                spike.Show();
            } else if (cmd == QLatin1String("release")) {
                spike.Release();
            } else if (cmd == QLatin1String("cancel")) {
                spike.Cancel();
            } else if (cmd == QLatin1String("hide")) {
                spike.Hide();
            } else if (cmd == QLatin1String("status")) {
                spike.Status();
            } else if (cmd == QLatin1String("quit")) {
                spike.Quit();
            } else {
                out(QStringLiteral("unknown command '%1'").arg(cmd));
            }
        }
    });

    const int ret = app.exec();
    if (action) {
        // Do not leave the spike's shortcut behind in kglobalshortcutsrc.
        KGlobalAccel::self()->removeAllShortcuts(action);
        out(QStringLiteral("global shortcut removed"));
    }
    return ret;
}

#include "main.moc"
