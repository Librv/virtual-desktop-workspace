// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT
// Radial menu scene. `menu` is the context object set by RadialMenu (C++).
import QtQuick
import QtQuick.Shapes
import org.kde.kirigami as Kirigami

Item {
    id: root

    readonly property real outerRadius: Kirigami.Units.gridUnit * 11
    readonly property real innerRadius: menu.deadZone
    readonly property real gapDegrees: 1.5
    readonly property int count: menu.slices.length

    Kirigami.Theme.colorSet: Kirigami.Theme.Window
    Kirigami.Theme.inherit: false

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.AllButtons
        onEntered: menu.pointerEvent(mouseX, mouseY)
        onPositionChanged: mouse => menu.pointerEvent(mouse.x, mouse.y)
    }

    Item {
        id: ring
        x: menu.centerX
        y: menu.centerY
        NumberAnimation on opacity { from: 0; to: 1; duration: Kirigami.Units.shortDuration }
        NumberAnimation on scale { from: 0.85; to: 1; duration: Kirigami.Units.shortDuration; easing.type: Easing.OutCubic }

        Repeater {
            model: root.count
            delegate: Item {
                id: slice
                required property int index
                readonly property var info: menu.slices[index]
                readonly property bool selected: menu.selected === index
                readonly property real center: menu.sliceAngle(index)
                readonly property real sweep: 360 / root.count
                // Qt Shapes angles: 0 is 3 o'clock, clockwise. Ours: 0 is up.
                readonly property real start: center - sweep / 2 - 90 + (root.count > 1 ? root.gapDegrees / 2 : 0)
                readonly property real span: sweep - (root.count > 1 ? root.gapDegrees : 0)

                Shape {
                    preferredRendererType: Shape.CurveRenderer
                    ShapePath {
                        fillColor: slice.selected ? Kirigami.Theme.highlightColor
                                                  : Qt.alpha(Kirigami.Theme.backgroundColor, 0.92)
                        strokeColor: Qt.alpha(Kirigami.Theme.textColor, 0.25)
                        strokeWidth: 1
                        PathAngleArc {
                            centerX: 0; centerY: 0
                            radiusX: root.outerRadius; radiusY: root.outerRadius
                            startAngle: slice.start
                            sweepAngle: root.count > 1 ? slice.span : 359.99
                        }
                        PathAngleArc {
                            moveToStart: false
                            centerX: 0; centerY: 0
                            radiusX: root.innerRadius + 4; radiusY: root.innerRadius + 4
                            startAngle: slice.start + (root.count > 1 ? slice.span : 359.99)
                            sweepAngle: root.count > 1 ? -slice.span : -359.99
                        }
                    }
                }

                Column {
                    readonly property real r: (root.outerRadius + root.innerRadius) / 2
                    x: r * Math.sin(slice.center * Math.PI / 180) - width / 2
                    y: -r * Math.cos(slice.center * Math.PI / 180) - height / 2
                    spacing: Kirigami.Units.smallSpacing
                    Kirigami.Icon {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Kirigami.Units.iconSizes.large
                        height: width
                        source: slice.info.icon
                        selected: slice.selected
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: slice.info.name
                        font: Kirigami.Theme.defaultFont
                        color: slice.selected ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                    }
                }
            }
        }

        // Center dead zone: shows "cancel" while no slice is selected.
        Rectangle {
            visible: root.count > 0
            x: -width / 2
            y: -height / 2
            width: root.innerRadius * 2
            height: width
            radius: width / 2
            color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.92)
            border.color: Qt.alpha(Kirigami.Theme.textColor, 0.25)
            Kirigami.Icon {
                anchors.centerIn: parent
                width: Kirigami.Units.iconSizes.medium
                height: width
                source: "dialog-cancel"
                opacity: menu.selected < 0 ? 1 : 0.35
            }
        }

        // Empty state.
        Rectangle {
            visible: root.count === 0
            x: -width / 2
            y: -height / 2
            width: hint.implicitWidth + Kirigami.Units.gridUnit * 2
            height: hint.implicitHeight + Kirigami.Units.gridUnit
            radius: Kirigami.Units.cornerRadius
            color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.95)
            border.color: Qt.alpha(Kirigami.Theme.textColor, 0.25)
            Text {
                id: hint
                anchors.centerIn: parent
                text: "No workspaces yet"
                font: Kirigami.Theme.defaultFont
                color: Kirigami.Theme.textColor
            }
        }
    }
}
