// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QObject>
#include <QPoint>
#include <QString>
#include <QVector>

#include <memory>

class QQuickView;

class RadialMenuScene;

// The radial menu overlay: a transparent full-screen QQuickView on the
// layer-shell overlay layer. The window is created on show() and destroyed on
// hide(). The pointer position is tracked from hover events on the surface.
class RadialMenu : public QObject
{
    Q_OBJECT

public:
    struct Item {
        QString id;
        QString name;
        QString icon;
    };

    explicit RadialMenu(QObject *parent = nullptr);
    ~RadialMenu() override;

    // Maps the menu with its ring centered on `center` (global coordinates,
    // equal to surface coordinates for a full-screen surface on one monitor).
    // An empty `items` shows the "No workspaces yet" hint.
    void show(QPoint center, const QVector<Item> &items);
    void hide();
    bool isVisible() const;

    // Id of the slice under the last pointer position, or an empty string in
    // the dead zone, before any pointer event, or when not shown.
    QString selectedId() const;

private:
    std::unique_ptr<RadialMenuScene> m_scene;
    std::unique_ptr<QQuickView> m_view;
};
