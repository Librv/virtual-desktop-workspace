// SPDX-FileCopyrightText: 2026 Librv
// SPDX-License-Identifier: MIT

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami
import org.kde.kcmutils as KCM

KCM.AbstractKCM {
    id: root

    implicitWidth: Kirigami.Units.gridUnit * 50
    implicitHeight: Kirigami.Units.gridUnit * 34

    readonly property var workspaces: kcm.workspaces
    readonly property var current: list.currentItem
    readonly property int row: list.currentIndex

    // The slot editor shows the workspace that is selected in the list.
    Binding {
        target: kcm.slots
        property: "workspaceRow"
        value: root.row
    }

    function formatFraction(value) {
        return (value / 1000).toFixed(3);
    }

    header: ColumnLayout {
        visible: kcm.problems.length > 0 || kcm.notes.length > 0
        spacing: 0
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: kcm.problems.length > 0
            type: Kirigami.MessageType.Warning
            text: kcm.problems.join("\n")
        }
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: kcm.notes.length > 0
            type: Kirigami.MessageType.Information
            text: i18n("Some entries in vdwrc were skipped and will be lost when you save:\n%1", kcm.notes.join("\n"))
        }
    }

    footer: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Separator {
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.margins: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing

            QQC2.TextField {
                id: captureName
                Layout.preferredWidth: Kirigami.Units.gridUnit * 12
                placeholderText: i18n("Name for the new workspace")
                onAccepted: if (captureButton.enabled) captureButton.clicked()
            }
            QQC2.Button {
                id: captureButton
                icon.name: "camera-photo-symbolic"
                text: i18n("Save current desktop as workspace")
                // The daemon needs a name; an enabled button that only fails looks broken.
                enabled: captureName.text.trim().length > 0
                onClicked: kcm.captureCurrentDesktop(captureName.text)
            }
            QQC2.Button {
                icon.name: "preferences-desktop-keyboard-shortcut"
                text: i18n("Configure shortcuts...")
                onClicked: kcm.openShortcuts()
            }
            QQC2.Label {
                Layout.fillWidth: true
                text: kcm.statusText
                elide: Text.ElideRight
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Workspace list and its buttons
        ColumnLayout {
            Layout.preferredWidth: Kirigami.Units.gridUnit * 16
            Layout.maximumWidth: Kirigami.Units.gridUnit * 16
            Layout.fillHeight: true
            spacing: 0

            QQC2.ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true

                ListView {
                    id: list
                    model: root.workspaces
                    clip: true
                    currentIndex: 0

                    Kirigami.PlaceholderMessage {
                        anchors.centerIn: parent
                        width: parent.width - Kirigami.Units.gridUnit * 2
                        visible: list.count === 0
                        text: i18n("No workspaces yet")
                        explanation: i18n("Add one, or save the current desktop as a workspace.")
                    }

                    delegate: QQC2.ItemDelegate {
                        id: item
                        required property int index
                        required property string wsId
                        required property string wsName
                        required property string wsIcon
                        required property bool isDefault
                        required property int slotCount

                        width: ListView.view.width
                        highlighted: ListView.isCurrentItem
                        onClicked: list.currentIndex = index

                        contentItem: RowLayout {
                            spacing: Kirigami.Units.smallSpacing
                            Kirigami.Icon {
                                source: item.wsIcon || "window-duplicate"
                                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                                implicitHeight: Kirigami.Units.iconSizes.smallMedium
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 0
                                QQC2.Label {
                                    Layout.fillWidth: true
                                    text: item.wsName || i18n("(no name)")
                                    elide: Text.ElideRight
                                }
                                QQC2.Label {
                                    Layout.fillWidth: true
                                    text: i18np("%1 app", "%1 apps", item.slotCount)
                                    opacity: 0.7
                                    font: Kirigami.Theme.smallFont
                                }
                            }
                            Kirigami.Icon {
                                visible: item.isDefault
                                source: "emblem-favorite"
                                implicitWidth: Kirigami.Units.iconSizes.small
                                implicitHeight: Kirigami.Units.iconSizes.small
                                QQC2.ToolTip.text: i18n("Default workspace")
                                QQC2.ToolTip.visible: defaultHover.hovered
                                HoverHandler {
                                    id: defaultHover
                                }
                            }
                        }
                    }
                }
            }

            Kirigami.Separator {
                Layout.fillWidth: true
            }
            RowLayout {
                Layout.margins: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing

                QQC2.ToolButton {
                    icon.name: "list-add"
                    text: i18n("Add")
                    onClicked: {
                        list.currentIndex = root.workspaces.addWorkspace(i18n("New workspace"));
                        nameField.forceActiveFocus();
                        nameField.selectAll();
                    }
                }
                QQC2.ToolButton {
                    icon.name: "edit-delete"
                    display: QQC2.AbstractButton.IconOnly
                    text: i18n("Delete workspace")
                    enabled: root.row >= 0
                    onClicked: root.workspaces.removeWorkspace(root.row)
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                }
                Item {
                    Layout.fillWidth: true
                }
                QQC2.ToolButton {
                    icon.name: "go-up"
                    display: QQC2.AbstractButton.IconOnly
                    text: i18n("Move up")
                    enabled: root.row > 0
                    onClicked: list.currentIndex = root.workspaces.moveWorkspace(root.row, -1)
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                }
                QQC2.ToolButton {
                    icon.name: "go-down"
                    display: QQC2.AbstractButton.IconOnly
                    text: i18n("Move down")
                    enabled: root.row >= 0 && root.row < list.count - 1
                    onClicked: list.currentIndex = root.workspaces.moveWorkspace(root.row, 1)
                    QQC2.ToolTip.text: text
                    QQC2.ToolTip.visible: hovered
                }
            }
        }

        Kirigami.Separator {
            Layout.fillHeight: true
        }

        // Editor for the selected workspace
        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: availableWidth

            ColumnLayout {
                width: parent.width
                spacing: Kirigami.Units.largeSpacing

                GridLayout {
                    Layout.fillWidth: true
                    // Form beside the preview when wide enough, else stacked.
                    columns: width > Kirigami.Units.gridUnit * 32 ? 2 : 1
                    columnSpacing: Kirigami.Units.largeSpacing

                    Kirigami.FormLayout {
                        Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                        Layout.fillWidth: parent.columns === 1
                        enabled: root.current !== null

                        QQC2.TextField {
                            id: nameField
                            Kirigami.FormData.label: i18n("Name:")
                            text: root.current ? root.current.wsName : ""
                            onTextEdited: root.workspaces.setName(root.row, text)
                        }
                        RowLayout {
                            Kirigami.FormData.label: i18n("Icon:")
                            Kirigami.Icon {
                                source: iconField.text || "window-duplicate"
                                implicitWidth: Kirigami.Units.iconSizes.medium
                                implicitHeight: Kirigami.Units.iconSizes.medium
                            }
                            QQC2.TextField {
                                id: iconField
                                text: root.current ? root.current.wsIcon : ""
                                placeholderText: i18n("Icon theme name")
                                onTextEdited: root.workspaces.setIcon(root.row, text)
                            }
                        }
                        QQC2.CheckBox {
                            Kirigami.FormData.label: i18n("Default:")
                            text: i18n("Open this workspace on a short press")
                            checked: root.current ? root.current.isDefault : false
                            onToggled: root.workspaces.setDefaultRow(checked ? root.row : -1)
                        }
                        QQC2.Label {
                            Kirigami.FormData.label: i18n("Id:")
                            text: root.current ? root.current.wsId : ""
                            opacity: 0.7
                        }
                    }

                    WindowPreview {
                        id: preview
                        Layout.fillWidth: true
                        Layout.minimumWidth: Kirigami.Units.gridUnit * 8
                        Layout.preferredHeight: Math.max(Kirigami.Units.gridUnit * 6, Math.min(Kirigami.Units.gridUnit * 12, width / aspect))
                        visible: root.current !== null
                        model: kcm.slots
                        iconFor: row => kcm.appInfo(row.app).icon
                    }
                }

                Kirigami.Heading {
                    level: 3
                    text: i18n("Apps")
                    visible: root.current !== null
                }

                Repeater {
                    id: slotRepeater
                    model: kcm.slots

                    delegate: Kirigami.AbstractCard {
                        id: card
                        required property int index
                        required property string app
                        required property string args
                        required property string profile
                        required property real geoX
                        required property real geoY
                        required property real geoWidth
                        required property real geoHeight

                        Layout.fillWidth: true
                        readonly property var info: kcm.appInfo(app)
                        readonly property var profiles: kcm.browserProfiles(app, profile)
                        readonly property var profileEntries: {
                            const entries = [{dir: "", text: i18n("Last used")}];
                            for (const c of profiles.choices) {
                                entries.push({dir: c.dir, text: c.missing ? i18n("%1 (missing)", c.dir) : c.name});
                            }
                            return entries;
                        }

                        // Same color as this app's rectangle in the preview.
                        Rectangle {
                            anchors.fill: parent
                            z: 1
                            color: "transparent"
                            radius: Kirigami.Units.cornerRadius
                            border.width: 1
                            border.color: preview.colorFor(card.index)
                        }

                        contentItem: RowLayout {
                            spacing: Kirigami.Units.largeSpacing

                            Kirigami.FormLayout {
                                Layout.fillWidth: true
                                twinFormLayouts: []

                                RowLayout {
                                    Kirigami.FormData.label: i18n("App:")
                                    QQC2.Button {
                                        icon.name: card.info.icon
                                        text: card.app === "" ? i18n("Choose app...") : card.info.name
                                        onClicked: {
                                            appSheet.slotIndex = card.index;
                                            appSheet.open();
                                        }
                                    }
                                    QQC2.Label {
                                        visible: card.app !== "" && !card.info.found
                                        text: i18n("Not installed")
                                        color: Kirigami.Theme.negativeTextColor
                                    }
                                }
                                QQC2.TextField {
                                    Kirigami.FormData.label: i18n("Arguments:")
                                    Layout.fillWidth: true
                                    text: card.args
                                    placeholderText: i18n("Extra command line arguments")
                                    onTextEdited: kcm.slots.setField(card.index, "args", text)
                                }
                                QQC2.ComboBox {
                                    id: profileCombo
                                    Kirigami.FormData.label: i18n("Profile:")
                                    visible: card.profiles.isBrowser
                                    model: card.profileEntries
                                    textRole: "text"
                                    valueRole: "dir"
                                    // From the entries, not indexOfValue(): that binding is not
                                    // evaluated again when the model is filled.
                                    currentIndex: Math.max(0, card.profileEntries.findIndex(e => e.dir === card.profile))
                                    onActivated: kcm.slots.setField(card.index, "profile", currentValue)
                                }
                                QQC2.Label {
                                    visible: profileCombo.visible && card.profiles.error !== ""
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    text: i18n("Cannot read the browser profiles: %1", card.profiles.error)
                                    color: Kirigami.Theme.negativeTextColor
                                    font: Kirigami.Theme.smallFont
                                }
                                QQC2.Label {
                                    visible: profileCombo.visible && card.profile !== "" && kcm.hasProfileOption(card.args)
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    text: i18n("The profile replaces the --profile-directory option in Arguments.")
                                    font: Kirigami.Theme.smallFont
                                }
                                GridLayout {
                                    Kirigami.FormData.label: i18n("Position:")
                                    columns: 2
                                    columnSpacing: Kirigami.Units.largeSpacing
                                    rowSpacing: Kirigami.Units.smallSpacing
                                    Repeater {
                                        model: [
                                            {
                                                field: "geoX",
                                                label: i18n("X"),
                                                min: 0
                                            },
                                            {
                                                field: "geoY",
                                                label: i18n("Y"),
                                                min: 0
                                            },
                                            {
                                                field: "geoWidth",
                                                label: i18n("Width"),
                                                min: 1
                                            },
                                            {
                                                field: "geoHeight",
                                                label: i18n("Height"),
                                                min: 1
                                            }
                                        ]
                                        delegate: RowLayout {
                                            id: geo
                                            required property var modelData
                                            QQC2.Label {
                                                Layout.preferredWidth: Kirigami.Units.gridUnit * 3
                                                text: geo.modelData.label
                                            }
                                            // Fractions of the usable area, edited in thousandths.
                                            QQC2.SpinBox {
                                                id: spin
                                                from: geo.modelData.min
                                                to: 1000
                                                editable: true
                                                // The default IntValidator rejects the decimal point.
                                                validator: RegularExpressionValidator {
                                                    regularExpression: /[0-9]*([.,][0-9]{0,3})?/
                                                }
                                                // Breeze's own field commits on every keystroke, so a typed "0"
                                                // was clamped and reformatted before the "." could follow.
                                                // Same field, committed on Enter or focus loss instead.
                                                contentItem: T.TextField {
                                                    readonly property TextMetrics _textMetrics: TextMetrics {
                                                        text: spin.textFromValue(spin.to, spin.locale)
                                                        font: spin.font
                                                    }
                                                    implicitWidth: Math.max(_textMetrics.width + 2, Math.round(contentWidth)) + leftPadding + rightPadding
                                                    implicitHeight: Math.round(contentHeight) + topPadding + bottomPadding
                                                    z: 2
                                                    font: spin.font
                                                    palette: spin.palette
                                                    text: spin.displayText
                                                    color: Kirigami.Theme.textColor
                                                    selectionColor: Kirigami.Theme.highlightColor
                                                    selectedTextColor: Kirigami.Theme.highlightedTextColor
                                                    selectByMouse: true
                                                    hoverEnabled: false
                                                    verticalAlignment: Qt.AlignVCenter
                                                    validator: spin.validator
                                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                                    onEditingFinished: {
                                                        // To the model, never to spin.value: assigning it would break its binding.
                                                        const typed = Math.max(spin.from, Math.min(spin.to, spin.valueFromText(text, spin.locale)));
                                                        if (typed !== spin.value) {
                                                            kcm.slots.setField(card.index, geo.modelData.field, typed / 1000);
                                                        }
                                                        // Show the clamped, formatted value even when it did not change.
                                                        text = Qt.binding(() => spin.displayText);
                                                    }
                                                    Accessible.name: spin.Accessible.name
                                                }
                                                value: Math.round(card[geo.modelData.field] * 1000)
                                                textFromValue: function (v) {
                                                    return root.formatFraction(v);
                                                }
                                                valueFromText: function (t) {
                                                    const n = parseFloat(t.replace(",", "."));
                                                    return isNaN(n) ? value : Math.round(n * 1000);
                                                }
                                                onValueModified: kcm.slots.setField(card.index, geo.modelData.field, value / 1000)
                                            }
                                        }
                                    }
                                }
                            }
                            ColumnLayout {
                                Layout.alignment: Qt.AlignTop
                                spacing: 0
                                QQC2.ToolButton {
                                    icon.name: "go-up"
                                    display: QQC2.AbstractButton.IconOnly
                                    text: i18n("Move up")
                                    enabled: card.index > 0
                                    onClicked: root.workspaces.moveSlot(root.row, card.index, card.index - 1)
                                    QQC2.ToolTip.text: text
                                    QQC2.ToolTip.visible: hovered
                                }
                                QQC2.ToolButton {
                                    icon.name: "go-down"
                                    display: QQC2.AbstractButton.IconOnly
                                    text: i18n("Move down")
                                    enabled: card.index < slotRepeater.count - 1
                                    onClicked: root.workspaces.moveSlot(root.row, card.index, card.index + 1)
                                    QQC2.ToolTip.text: text
                                    QQC2.ToolTip.visible: hovered
                                }
                            }
                            QQC2.ToolButton {
                                Layout.alignment: Qt.AlignTop
                                icon.name: "edit-delete"
                                display: QQC2.AbstractButton.IconOnly
                                text: i18n("Remove app")
                                onClicked: root.workspaces.removeSlot(root.row, card.index)
                                QQC2.ToolTip.text: text
                                QQC2.ToolTip.visible: hovered
                            }
                        }
                    }
                }

                QQC2.Label {
                    visible: root.current !== null && slotRepeater.count > 1
                    text: i18n("Apps lower in the list open on top.")
                    opacity: 0.7
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                QQC2.Button {
                    visible: root.current !== null
                    icon.name: "list-add"
                    text: i18n("Add app")
                    onClicked: root.workspaces.addSlot(root.row)
                }
            }
        }
    }

    Kirigami.OverlaySheet {
        id: appSheet
        property int slotIndex: -1

        title: i18n("Choose an app")
        onOpened: {
            search.text = "";
            search.forceActiveFocus();
        }
        onClosed: kcm.apps.setFilter("")

        header: Kirigami.SearchField {
            id: search
            onTextChanged: kcm.apps.setFilter(text)
        }

        ListView {
            implicitWidth: Kirigami.Units.gridUnit * 24
            implicitHeight: Kirigami.Units.gridUnit * 20
            clip: true
            model: kcm.apps
            delegate: QQC2.ItemDelegate {
                required property string appId
                required property string appName
                required property string appIcon
                width: ListView.view.width
                icon.name: appIcon
                text: appName
                onClicked: {
                    kcm.slots.setField(appSheet.slotIndex, "app", appId);
                    appSheet.close();
                }
            }
        }
    }
}
