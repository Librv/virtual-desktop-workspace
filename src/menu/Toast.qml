// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT
// Toast scene: an icon and a line of text. `icon` and `message` are initial
// properties set by Toast (C++). The window is sized to this item.
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Item {
    id: root

    required property string icon
    required property string message

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    implicitWidth: row.implicitWidth + Kirigami.Units.largeSpacing * 2
    implicitHeight: row.implicitHeight + Kirigami.Units.largeSpacing * 2

    Rectangle {
        anchors.fill: parent
        radius: Kirigami.Units.cornerRadius
        color: Kirigami.Theme.backgroundColor
        border.width: 1
        border.color: Qt.alpha(Kirigami.Theme.textColor, 0.25)
    }

    RowLayout {
        id: row
        anchors.centerIn: parent
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Icon {
            source: root.icon
            implicitWidth: Kirigami.Units.iconSizes.medium
            implicitHeight: Kirigami.Units.iconSizes.medium
        }
        Text {
            text: root.message
            color: Kirigami.Theme.textColor
            font: Kirigami.Theme.defaultFont
            wrapMode: Text.WordWrap
            Layout.preferredWidth: Math.min(implicitWidth, Kirigami.Units.gridUnit * 30)
        }
    }
}
