// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT
#include "toast.h"

#include <LayerShellQt/Window>

#include <QDebug>
#include <QQuickView>
#include <QVariantMap>

namespace
{
constexpr int topMargin = 24;
}

Toast::Toast(QObject *parent)
    : QObject(parent)
{
}

Toast::~Toast() = default;

void Toast::show(const QString &icon, const QString &text)
{
    hide();

    m_view = std::make_unique<QQuickView>();
    m_view->setColor(Qt::transparent);
    m_view->setResizeMode(QQuickView::SizeViewToRootObject);
    m_view->setTitle(QStringLiteral("vdw toast"));
    m_view->setInitialProperties(QVariantMap{{QStringLiteral("icon"), icon}, {QStringLiteral("message"), text}});
    m_view->setSource(QUrl(QStringLiteral("qrc:/vdwmenu/Toast.qml")));
    // A failed load would map an empty surface, so never show it.
    if (m_view->status() == QQuickView::Error) {
        qWarning() << "vdw toast: cannot load QML:" << m_view->errors();
        hide();
        return;
    }

    // Must be configured before the platform window is created (first show).
    auto *layerWindow = LayerShellQt::Window::get(m_view.get());
    layerWindow->setLayer(LayerShellQt::Window::LayerOverlay);
    layerWindow->setAnchors(LayerShellQt::Window::AnchorTop);
    layerWindow->setMargins(QMargins(0, topMargin, 0, 0));
    layerWindow->setExclusiveZone(0);
    layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    layerWindow->setActivateOnShow(false);
    layerWindow->setScope(QStringLiteral("vdw-toast"));
    // Information only: clicks go to whatever is underneath.
    m_view->setFlags(m_view->flags() | Qt::WindowTransparentForInput);

    m_view->show();
}

void Toast::hide()
{
    m_view.reset();
}
