// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
import QtQuick 2.0

import "/gui/style"

// Jochona M3 (session resilience, proposal §6.8): controller-navigable
// replacement for the plain-text "Test Network" result dialog (see
// HomeView.qml/PcView.qml's testConnectionDialog). Renders a per-port
// checklist with plain-language guidance instead of one paragraph, so a
// blocked port maps directly to what needs forwarding.
//
// Wire exactly like the dialog it replaces:
//   computerModel.testConnectionForComputer(index)
//   computerModel.connectionTestCompleted.connect(dialog.connectionTestComplete)
//   dialog.open()
// and connect dialog.retryRequested() back to testConnectionForComputer()
// if the host wants the "Test Again" button to work.
Item {
    id: root
    anchors.fill: parent
    z: 1000
    visible: false

    // Optional: item to restore focus to when the dialog closes.
    property Item returnFocusItem: null

    // -1 = inconclusive (test servers unreachable) or not run yet.
    // Otherwise a bitmask of BLOCKED ports; 0 means every tested port passed.
    property int result: -1
    property bool testing: false

    signal retryRequested()

    // Port catalog mirrors moonlight-common-c's Limelight.h ML_PORT_FLAG_*
    // values (LiGetPortFromPortFlagIndex()/LiGetProtocolFromPortFlagIndex()
    // assign UDP to flag indices 8+, TCP below). Keep in sync with that
    // header; ML_PORT_FLAG_ALL is what DeferredTestConnectionTask tests.
    readonly property var portCatalog: [
        { flag: 0x0001, proto: qsTr("TCP"), port: 47984, purpose: qsTr("Discovery & legacy pairing"),
          guidance: qsTr("Forward or unblock TCP port 47984. Older GameStream hosts use it for discovery and pairing.") },
        { flag: 0x0002, proto: qsTr("TCP"), port: 47989, purpose: qsTr("Pairing & app list"),
          guidance: qsTr("Forward or unblock TCP port 47989. Jochona uses it to pair with your host and load its app list.") },
        { flag: 0x0004, proto: qsTr("TCP"), port: 48010, purpose: qsTr("Stream handshake"),
          guidance: qsTr("Forward or unblock TCP port 48010. It negotiates the stream before playback starts.") },
        { flag: 0x0100, proto: qsTr("UDP"), port: 47998, purpose: qsTr("Video stream"),
          guidance: qsTr("Forward or unblock UDP port 47998. This carries the video feed from your host.") },
        { flag: 0x0200, proto: qsTr("UDP"), port: 47999, purpose: qsTr("Controller & input"),
          guidance: qsTr("Forward or unblock UDP port 47999. This carries your controller, mouse, and keyboard input.") },
        { flag: 0x0400, proto: qsTr("UDP"), port: 48000, purpose: qsTr("Audio stream"),
          guidance: qsTr("Forward or unblock UDP port 48000. This carries the audio feed from your host.") },
        { flag: 0x0800, proto: qsTr("UDP"), port: 48010, purpose: qsTr("Mic & handshake ping"),
          guidance: qsTr("Forward or unblock UDP port 48010. Newer hosts use it to finish the handshake and, on Sunshine, to carry microphone audio.") }
    ]

    readonly property int blockedCount: {
        var count = 0
        if (result > 0) {
            for (var i = 0; i < portCatalog.length; i++) {
                if ((result & portCatalog[i].flag) !== 0) {
                    count += 1
                }
            }
        }
        return count
    }

    function open() {
        testing = true
        result = -1
        visible = true
        forceActiveFocus()
        closeButton.forceActiveFocus(Qt.TabFocus)
    }

    function close() {
        visible = false
        if (returnFocusItem) {
            returnFocusItem.forceActiveFocus()
        }
    }

    // Drop-in handler for ComputerModel::connectionTestCompleted(result,
    // blockedPorts). blockedPorts is accepted for signature compatibility
    // with the signal this replaces; the per-port state below is derived
    // from `result` directly so it can never disagree with the guidance text.
    function connectionTestComplete(testResult, blockedPorts) {
        testing = false
        result = testResult
    }

    Keys.onEscapePressed: root.close()
    Keys.onBackPressed: root.close()

    Rectangle {
        anchors.fill: parent
        color: Tokens.surface
        opacity: 0.85

        MouseArea {
            // Swallow clicks to the scrim; only the Close action dismisses.
            anchors.fill: parent
        }
    }

    // Controller-focusable CTA, matching SessionStatusOverlay's ActionButton.
    component ActionButton: Rectangle {
        id: btn

        property alias text: label.text
        property bool primary: false

        signal activated()

        width: Math.max(200, label.implicitWidth + 56)
        height: Tokens.rowHeight
        radius: height / 2
        color: primary
               ? (activeFocus || hoverArea.containsMouse ? Tokens.accentFocus : Tokens.accent)
               : (activeFocus || hoverArea.containsMouse ? Tokens.surfaceFocus : Tokens.surface)
        border.width: activeFocus ? 2 : 1
        border.color: activeFocus
                      ? (primary ? Tokens.textPrimary : Tokens.borderFocus)
                      : (primary ? Tokens.accentFocus : Tokens.border)

        activeFocusOnTab: true

        Behavior on color {
            ColorAnimation { duration: Tokens.motion(Tokens.durationFast) }
        }

        Text {
            id: label
            anchors.centerIn: parent
            font.family: Tokens.familyBody
            font.pointSize: Tokens.sizeBody
            font.weight: Font.DemiBold
            color: Tokens.textPrimary
        }

        MouseArea {
            id: hoverArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: btn.activated()
        }

        Keys.onReturnPressed: btn.activated()
        Keys.onEnterPressed: btn.activated()
    }

    component Spinner: Item {
        id: spinner
        width: 48
        height: 48

        Repeater {
            model: 8
            delegate: Rectangle {
                width: 6
                height: 6
                radius: 3
                color: Tokens.accent
                x: spinner.width / 2 - width / 2 + Math.cos(index / 8 * 2 * Math.PI) * (spinner.width / 2 - width)
                y: spinner.height / 2 - height / 2 + Math.sin(index / 8 * 2 * Math.PI) * (spinner.height / 2 - height)
                opacity: 0.25

                SequentialAnimation on opacity {
                    running: spinner.visible && Tokens.motion(1) > 0
                    loops: Animation.Infinite
                    PauseAnimation { duration: index * 80 }
                    NumberAnimation { to: 1.0; duration: 220 }
                    NumberAnimation { to: 0.25; duration: 480 }
                }
            }
        }
    }

    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(root.width - Tokens.gutter * 4, 640)
        height: Math.min(root.height - Tokens.gutter * 4, contentColumn.implicitHeight + Tokens.gutter * 2)
        radius: Tokens.radiusCard
        color: Tokens.surfaceFocus
        border.width: 1
        border.color: Tokens.border

        Flickable {
            anchors.fill: parent
            anchors.margins: Tokens.gutter
            contentWidth: width
            contentHeight: contentColumn.implicitHeight
            clip: true

            Column {
                id: contentColumn
                width: parent.width
                spacing: Tokens.gutter

                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: Tokens.textPrimary
                    font.family: Tokens.familyDisplay
                    font.pointSize: Tokens.sizeTitle
                    font.bold: true
                    text: qsTr("Connection Test")
                }

                Spinner {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: root.testing
                }

                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    color: Tokens.textPrimary
                    font.family: Tokens.familyBody
                    font.pointSize: Tokens.sizeBody
                    text: {
                        if (root.testing) {
                            return qsTr("Testing your network connection to determine if any required ports are blocked. This may take a few seconds…")
                        }
                        if (root.result === -1) {
                            return qsTr("The test could not be completed because none of Jochona's connection-testing servers were reachable. Check your Internet connection and try again.")
                        }
                        if (root.result === 0) {
                            return qsTr("This network does not appear to be blocking Jochona. If you still can't connect, check your PC's firewall settings.")
                        }
                        return qsTr("Your network is blocking %1 of the %2 ports Jochona needs. Streaming may fail or be unreliable until they're forwarded or unblocked.").arg(root.blockedCount).arg(root.portCatalog.length)
                    }
                }

                Repeater {
                    model: (!root.testing && root.result !== -1) ? root.portCatalog : []

                    delegate: Rectangle {
                        id: portRow
                        width: contentColumn.width
                        height: portLayout.implicitHeight + Tokens.gutter
                        radius: Tokens.radiusCard / 2
                        color: Tokens.surface
                        border.width: 1
                        border.color: Tokens.border

                        readonly property bool blocked: (root.result & modelData.flag) !== 0

                        Row {
                            id: portLayout
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: Tokens.gutter / 2
                            spacing: Tokens.gutter / 2

                            Rectangle {
                                width: 14
                                height: 14
                                radius: 7
                                anchors.verticalCenter: parent.verticalCenter
                                color: portRow.blocked ? Tokens.statusOffline : Tokens.statusOnline
                            }

                            Column {
                                width: portLayout.width - 14 - portLayout.spacing
                                spacing: 2

                                Text {
                                    width: parent.width
                                    elide: Text.ElideRight
                                    color: Tokens.textPrimary
                                    font.family: Tokens.familyBody
                                    font.bold: true
                                    font.pointSize: Tokens.sizeBody
                                    text: qsTr("%1 %2 — %3").arg(modelData.proto).arg(modelData.port).arg(modelData.purpose)
                                }

                                Text {
                                    width: parent.width
                                    wrapMode: Text.Wrap
                                    visible: portRow.blocked
                                    color: Tokens.textSecondary
                                    font.family: Tokens.familyBody
                                    font.pointSize: Tokens.sizeMicro
                                    text: modelData.guidance
                                }
                            }
                        }
                    }
                }

                Row {
                    width: parent.width
                    spacing: Tokens.gutter

                    ActionButton {
                        id: closeButton
                        primary: true
                        text: Glyphs.glyph(Glyphs.family, "b") + "  " + qsTr("Close")
                        onActivated: root.close()
                        Keys.onRightPressed: if (retryButton.visible) retryButton.forceActiveFocus(Qt.TabFocus)
                    }

                    ActionButton {
                        id: retryButton
                        visible: !root.testing
                        text: Glyphs.glyph(Glyphs.family, "a") + "  " + qsTr("Test Again")
                        onActivated: {
                            root.testing = true
                            root.result = -1
                            root.retryRequested()
                        }
                        Keys.onLeftPressed: closeButton.forceActiveFocus(Qt.TabFocus)
                    }
                }
            }
        }
    }
}
