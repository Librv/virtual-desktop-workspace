// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QString>

#include <memory>

class QQuickView;

// A short message at the top center of the screen: a small QQuickView on the
// layer-shell overlay layer, anchored to the top edge only (the compositor
// centers it horizontally) and sized to its content. It never takes input
// focus. The window is created on show() and destroyed on hide().
class Toast : public QObject
{
    Q_OBJECT

public:
    explicit Toast(QObject *parent = nullptr);
    ~Toast() override;

    // Shows `text` with the theme icon `icon`, replacing any toast already up.
    void show(const QString &icon, const QString &text);
    void hide();
    bool isVisible() const;

private:
    std::unique_ptr<QQuickView> m_view;
};
