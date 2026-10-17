// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT
// Pager-style preview of a workspace: a mini screen with one rectangle per
// slot at its fractional geometry.
import QtQuick
import QtQuick.Window
import org.kde.kirigami as Kirigami

Item {
    id: root

    // Rows expose geoX, geoY, geoWidth, geoHeight (fractions 0..1).
    property var model
    // Called with the delegate's `model`; returns an icon name.
    property var iconFor: row => row.icon
    // Screen size is not stored, so slots are drawn at the current screen's aspect.
    property real aspect: Screen.width / Screen.height

    // Fixed per-slot color (theme colors dim while the window is unfocused).
    // Golden-ratio hue steps keep neighbouring slots far apart.
    function colorFor(index) {
        return Qt.hsla((index * 0.618034) % 1, 0.7, 0.45, 1);
    }

    Rectangle {
        id: screenRect
        anchors.centerIn: parent
        width: Math.min(root.width, root.height * root.aspect)
        height: width / root.aspect
        radius: Kirigami.Units.cornerRadius
        // Fixed colors: theme colors dim while System Settings is unfocused.
        color: Qt.rgba(0.5, 0.5, 0.5, 0.15)
        border.width: 1
        border.color: Qt.rgba(0.5, 0.5, 0.5, 0.6)

        Repeater {
            model: root.model
            delegate: Rectangle {
                id: win
                required property var model
                required property int index
                x: model.geoX * screenRect.width
                y: model.geoY * screenRect.height
                width: model.geoWidth * screenRect.width
                height: model.geoHeight * screenRect.height
                color: Qt.alpha(root.colorFor(index), 0.6)
                border.width: 1
                border.color: root.colorFor(index)
                Kirigami.Icon {
                    readonly property real size: Math.min(win.width, win.height) * 0.6
                    anchors.centerIn: parent
                    width: Math.min(size, Kirigami.Units.iconSizes.large)
                    height: width
                    visible: size >= Kirigami.Units.iconSizes.small / 2
                    source: root.iconFor(win.model) || "application-x-executable"
                }
            }
        }
    }
}
