// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import ControllerManager 1.0
import ControllerProfileStore 1.0

import "/gui"
import "/gui/style"

// Jochona: M2 controller-first screen (proposal §5.3, controller manager
// per §6.7). One full-width row per SDL-detected gamepad: family glyph,
// name, connection status (color plus text, never color alone), and a
// live input visualization strip fed by ControllerManager's ~30Hz poll.
// Enter opens a calibration panel; only the left-stick deadzone slider is
// wired to ControllerProfileStore for M2, to prove the persistence loop
// end to end. The rest of the panel (remaining deadzones, response
// curves, button remap table) is a later M2 pass.
Item {
    id: root

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Label {
            Layout.leftMargin: Tokens.gutter
            Layout.topMargin: 20
            Layout.bottomMargin: 4
            text: qsTr("Controllers")
            font.pointSize: Tokens.sizeTitle
            font.family: Tokens.familyDisplay
            font.bold: true
            color: Tokens.textPrimary
        }

        CenteredGridView {
            id: controllerGrid

            Layout.fillWidth: true
            Layout.fillHeight: true

            focus: true
            activeFocusOnTab: true
            topMargin: 4
            bottomMargin: 8
            cellWidth: controllerGrid.availableWidth > 0 ? Math.min(controllerGrid.availableWidth, Tokens.listMaxWidth) : Tokens.listMaxWidth
            cellHeight: Tokens.rowHeight
            objectName: qsTr("Controllers")

            model: ControllerManager.controllers

            // deviceIndex -> {buttons: {...}, axes: {...}}; refreshed from
            // ControllerManager.controllerLiveUpdate() every poll tick
            // (~30Hz). Kept separate from the `controllers` list property
            // so the visualization strip animates without ever forcing
            // this GridView to rebuild its model.
            property var liveState: ({})

            // Index of the row whose calibration panel is open, or -1.
            property int detailIndex: -1

            Component.onCompleted: {
                currentIndex = -1
                ControllerManager.start()
            }

            Component.onDestruction: {
                ControllerManager.stop()
            }

            Connections {
                target: ControllerManager

                function onControllerLiveUpdate(deviceIndex) {
                    var next = controllerGrid.liveState
                    next[deviceIndex] = ControllerManager.controllerSnapshot(deviceIndex)
                    controllerGrid.liveState = next
                }

                function onControllersChanged() {
                    // Topology changed: indices may now refer to a
                    // different physical controller, so stale live state
                    // and any open calibration panel are both dropped
                    // rather than silently relabeled.
                    controllerGrid.liveState = ({})
                    controllerGrid.detailIndex = -1
                }
            }

            function familyGlyph(family) {
                var g = Glyphs.glyph(family, "controller")
                return g.length > 0 ? g : Glyphs.glyph(family, "a")
            }

            function statusColor(connected) {
                return connected ? Tokens.statusOnline : Tokens.statusOffline
            }

            function statusText(connected) {
                return connected ? qsTr("Connected") : qsTr("Disconnected")
            }

            Label {
                anchors.centerIn: parent
                visible: controllerGrid.count === 0
                text: qsTr("No controllers detected. Connect a controller to configure it here.")
                color: Tokens.textSecondary
                font.pointSize: Tokens.sizeBody
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                width: Math.min(parent.width - 2 * Tokens.gutter, 480)
            }

            delegate: NavigableItemDelegate {
                id: controllerRow

                width: controllerGrid.cellWidth
                height: controllerGrid.cellHeight
                grid: controllerGrid

                readonly property var live: controllerGrid.liveState[index] || { "buttons": ({}), "axes": ({}) }

                function openDetail() {
                    controllerGrid.detailIndex = (controllerGrid.detailIndex === index) ? -1 : index
                }

                // Overrides NavigableItemDelegate's default clicked()-firing
                // handlers: this screen has no context menu, Enter always
                // toggles the calibration panel for this row.
                Keys.onReturnPressed: openDetail()
                Keys.onEnterPressed: openDetail()

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 4
                    radius: Tokens.radiusCard
                    color: parent.highlighted ? Tokens.surfaceFocus : Tokens.surface
                    border.width: parent.highlighted ? 2 : 1
                    border.color: parent.highlighted ? Tokens.borderFocus : Tokens.border
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Tokens.gutter
                    anchors.rightMargin: 16
                    spacing: 18

                    // Family glyph chip
                    Rectangle {
                        Layout.preferredWidth: 44
                        Layout.preferredHeight: 44
                        Layout.alignment: Qt.AlignVCenter
                        radius: 8
                        color: controllerRow.highlighted ? Tokens.accentFocus : Tokens.surfaceFocus
                        border.width: 1
                        border.color: Tokens.border

                        Text {
                            anchors.centerIn: parent
                            text: controllerGrid.familyGlyph(model.family)
                            font.family: Glyphs.fontFamily(model.family)
                            font.pixelSize: 22
                            color: Tokens.textPrimary
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 2

                        Label {
                            text: model.name
                            font.pointSize: Tokens.sizeSection
                            font.family: Tokens.familyDisplay
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
                                Layout.preferredWidth: 10
                                Layout.preferredHeight: 10
                                radius: 5
                                color: controllerGrid.statusColor(model.connected)
                            }

                            Label {
                                Layout.fillWidth: true
                                text: controllerGrid.statusText(model.connected) + " · " + qsTr("Slot %1").arg(model.slot + 1)
                                font.pointSize: Tokens.sizeBody
                                color: Tokens.textSecondary
                                elide: Text.ElideRight
                            }
                        }
                    }

                    // Live input visualization strip: one dot per digital
                    // button in the fixed logical vocabulary, lit with the
                    // accent color while held.
                    Row {
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 4

                        Repeater {
                            model: ["a", "b", "x", "y", "lb", "rb", "lt", "rt",
                                    "dpad_up", "dpad_down", "dpad_left", "dpad_right"]

                            Rectangle {
                                width: 10
                                height: 10
                                radius: 5
                                color: (controllerRow.live.buttons && controllerRow.live.buttons[modelData])
                                       ? Tokens.accent : Tokens.border

                                Behavior on color {
                                    ColorAnimation { duration: Tokens.motion(Tokens.durationFast) }
                                }
                            }
                        }
                    }

                    Label {
                        Layout.alignment: Qt.AlignVCenter
                        visible: controllerGrid.detailIndex === index
                        text: "▾"
                        font.pointSize: 20
                        color: Tokens.accent
                    }
                }
            }
        }

        // Calibration panel shell. Only the left-stick deadzone slider is
        // wired to ControllerProfileStore for M2 (see file header).
        Rectangle {
            id: detailPanel

            Layout.fillWidth: true
            Layout.preferredHeight: controllerGrid.detailIndex >= 0 ? 140 : 0
            clip: true
            color: Tokens.surface
            border.width: controllerGrid.detailIndex >= 0 ? 1 : 0
            border.color: Tokens.border

            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: Tokens.motion(Tokens.durationBase); easing.type: Easing.OutCubic }
            }

            readonly property var entry: (controllerGrid.detailIndex >= 0 && controllerGrid.detailIndex < controllerGrid.count)
                                          ? ControllerManager.controllers[controllerGrid.detailIndex] : null

            onEntryChanged: {
                if (entry) {
                    deadzoneSlider.value = ControllerProfileStore.profileFor(entry.path).calibration.deadzoneLeftStick
                    deadzoneSlider.forceActiveFocus()
                } else {
                    controllerGrid.forceActiveFocus()
                }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Tokens.gutter
                spacing: 8
                visible: detailPanel.entry !== null

                Label {
                    text: detailPanel.entry ? qsTr("Calibration — %1").arg(detailPanel.entry.name) : ""
                    font.pointSize: Tokens.sizeBody
                    font.bold: true
                    color: Tokens.textPrimary
                }

                Label {
                    text: qsTr("Left stick deadzone")
                    font.pointSize: Tokens.sizeMicro
                    color: Tokens.textSecondary
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    Slider {
                        id: deadzoneSlider

                        Layout.fillWidth: true
                        from: 0.0
                        to: 0.5
                        stepSize: 0.01

                        Keys.onEscapePressed: controllerGrid.detailIndex = -1

                        onMoved: {
                            if (detailPanel.entry) {
                                ControllerProfileStore.setDeadzone(detailPanel.entry.path, "leftStick", value)
                            }
                        }
                    }

                    Label {
                        text: Math.round(deadzoneSlider.value * 100) + "%"
                        font.pointSize: Tokens.sizeBody
                        color: Tokens.textSecondary
                    }
                }

                Label {
                    text: qsTr("More calibration controls land in a later M2 pass.")
                    font.pointSize: Tokens.sizeMicro
                    font.italic: true
                    color: Tokens.textSecondary
                }
            }
        }
    }
}
