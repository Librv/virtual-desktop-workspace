// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT
#include "radialmenu.h"

#include "radialmath.h"

#include <LayerShellQt/Window>

#include <QDebug>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QScreen>
#include <QVariantMap>

#include <numbers>

namespace
{
constexpr qreal deadZoneRadius = 48;
}

// State the QML scene binds to (the `menu` context object).
class RadialMenuScene : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList slices READ slices CONSTANT)
    Q_PROPERTY(qreal centerX READ centerX CONSTANT)
    Q_PROPERTY(qreal centerY READ centerY CONSTANT)
    Q_PROPERTY(int selected READ selected NOTIFY selectedChanged)
    Q_PROPERTY(qreal deadZone READ deadZone CONSTANT)
    // True once the window has drawn its first frame.
    Q_PROPERTY(bool shown READ shown NOTIFY shownChanged)

public:
    RadialMenuScene(QPoint center, const QVector<RadialMenu::Item> &items)
        : m_center(center)
        , m_items(items)
    {
        for (const RadialMenu::Item &item : items) {
            m_slices.append(QVariantMap{{QStringLiteral("name"), item.name}, {QStringLiteral("icon"), item.icon}});
        }
    }

    QVariantList slices() const
    {
        return m_slices;
    }
    qreal centerX() const
    {
        return m_center.x();
    }
    qreal centerY() const
    {
        return m_center.y();
    }
    int selected() const
    {
        return m_selected;
    }
    qreal deadZone() const
    {
        return deadZoneRadius;
    }

    bool shown() const
    {
        return m_shown;
    }

    void markShown()
    {
        if (!m_shown) {
            m_shown = true;
            Q_EMIT shownChanged();
        }
    }

    QString selectedId() const
    {
        return m_selected >= 0 && m_selected < m_items.size() ? m_items.at(m_selected).id : QString();
    }

    // Slice center angle in degrees, clockwise from up.
    Q_INVOKABLE qreal sliceAngle(int index) const
    {
        return RadialMath::sliceCenterAngle(index, m_items.size()) * 180.0 / std::numbers::pi;
    }

    Q_INVOKABLE void pointerEvent(qreal x, qreal y)
    {
        const int s = RadialMath::sliceAt(m_center, QPointF(x, y), m_items.size(), deadZoneRadius);
        if (s != m_selected) {
            m_selected = s;
            Q_EMIT selectedChanged();
        }
    }

Q_SIGNALS:
    void selectedChanged();
    void shownChanged();

private:
    QPointF m_center;
    QVector<RadialMenu::Item> m_items;
    QVariantList m_slices;
    int m_selected = -1;
    bool m_shown = false;
};

RadialMenu::RadialMenu(QObject *parent)
    : QObject(parent)
{
}

RadialMenu::~RadialMenu() = default;

void RadialMenu::show(QPoint center, const QVector<Item> &items)
{
    hide();
    m_scene = std::make_unique<RadialMenuScene>(center, items);

    m_view = std::make_unique<QQuickView>();
    m_view->setColor(Qt::transparent);
    m_view->setResizeMode(QQuickView::SizeRootObjectToView);
    m_view->setTitle(QStringLiteral("vdw radial menu"));
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        m_view->resize(screen->size());
    }
    m_view->rootContext()->setContextProperty(QStringLiteral("menu"), m_scene.get());
    m_view->setSource(QUrl(QStringLiteral("qrc:/vdwmenu/RadialMenu.qml")));
    // A failed load would still map an empty full-screen overlay that eats
    // the pointer, so never show it.
    if (m_view->status() == QQuickView::Error) {
        qWarning() << "vdw radial menu: cannot load QML:" << m_view->errors();
        hide();
        return;
    }

    // Must be configured before the platform window is created (first show).
    auto *layerWindow = LayerShellQt::Window::get(m_view.get());
    layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
    layerWindow->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop | LayerShellQt::Window::AnchorBottom
                                                          | LayerShellQt::Window::AnchorLeft | LayerShellQt::Window::AnchorRight));
    layerWindow->setExclusiveZone(-1);
    layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    layerWindow->setActivateOnShow(false);
    // KWin maps an unknown scope to a normal window, and its scale effect then
    // zooms the whole full-screen overlay around the screen center. "utility"
    // is skipped by the scale, fade and fadingpopups effects, so only the QML
    // animation runs, centered on the pointer.
    layerWindow->setScope(QStringLiteral("utility"));

    // frameSwapped comes from the render thread, so queue it to the scene.
    connect(m_view.get(), &QQuickWindow::frameSwapped, m_scene.get(), &RadialMenuScene::markShown, Qt::QueuedConnection);
    m_view->show();
}

void RadialMenu::hide()
{
    m_view.reset();
    m_scene.reset();
}

QString RadialMenu::selectedId() const
{
    return m_scene ? m_scene->selectedId() : QString();
}

#include "radialmenu.moc"
