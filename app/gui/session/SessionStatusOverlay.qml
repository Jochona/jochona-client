// SPDX-FileCopyrightText: Lunaframe Client Contributors
//
// SPDX-License-Identifier: GPL-3.0-only
//
import QtQuick 2.0

import Session 1.0

import "/gui/style"

// Jochona M3 (session resilience, proposal §6.8; controller-first §5.3):
// controller-navigable overlay that reflects Session's connection lifecycle
// in the visible Qt window. Once Session::connectionStarted() fires, the
// video takes over an entirely separate native SDL window and the Qt window
// is hidden (see StreamSegue.qml), so this overlay only ever needs to cover
// connecting / reconnecting / failed — never live gameplay.
//
// Host screens bind `session` to a live Session and react to the three
// signals below to actually do something:
//   - reconnectRequested(): Session is single-use (LiStopConnection() and
//     decoder teardown already happened by the time it fires), so the host
//     must call session.createReconnectSession(), initialize()+start() the
//     result, and assign it back to `session`. Reassigning to a *different*
//     Session object automatically resets this overlay to "attempting".
//   - quitRequested(): host should tear down and navigate away.
//   - runConnectionTestRequested(): host should run
//     ComputerModel::testConnectionForComputer() and show ConnectionTestDialog.
Item {
    id: root
    anchors.fill: parent

    // --- Public API ---

    property Session session: null

    // Auto-retry budget for a dropped/failed connection before we stop
    // retrying silently and hand control back to the user.
    property int maxAutoReconnectAttempts: 3
    property int autoReconnectDelayMs: 2000

    // "disconnected" | "attempting" | "connected" | "reconnecting" | "failed"
    state: "disconnected"

    readonly property bool active: state === "attempting" || state === "reconnecting" || state === "failed"
    visible: active

    property string stageText: ""
    property int errorCode: 0
    property string failingPorts: ""
    property int reconnectAttempt: 0
    property int portTestResult: -1

    signal reconnectRequested()
    signal quitRequested()
    signal runConnectionTestRequested()

    // Resets the auto-retry budget before asking the host to reconnect;
    // use this for the user-facing Reconnect button so a manual retry
    // always gets a fresh set of automatic attempts if it fails again.
    function manualReconnect() {
        reconnectAttempt = 0
        reconnectRequested()
    }

    function attemptAutoReconnect() {
        if (reconnectAttempt < maxAutoReconnectAttempts) {
            reconnectAttempt += 1
            state = "reconnecting"
            autoReconnectTimer.restart()
        } else {
            state = "failed"
        }
    }

    // Jochona M3 guidance table — mirrors moonlight-common-c's Limelight.h
    // ML_ERROR_* termination codes. Keep in sync with that header.
    function guidance(code) {
        switch (code) {
        case -100: // ML_ERROR_NO_VIDEO_TRAFFIC
            return {
                title: qsTr("No video from the host"),
                detail: qsTr("Your router or firewall is likely blocking the video stream. Check port forwarding, then run a guided connection test below.")
            }
        case -101: // ML_ERROR_NO_VIDEO_FRAME
            return {
                title: qsTr("Connection is too unstable to stream"),
                detail: qsTr("Try lowering your video bitrate, or move to a faster, more stable network connection.")
            }
        case -102: // ML_ERROR_UNEXPECTED_EARLY_TERMINATION
            return {
                title: qsTr("The stream stopped right after starting"),
                detail: qsTr("This is usually caused by the host PC. Try restarting the host, or check for other software using its GPU.")
            }
        case -103: // ML_ERROR_PROTECTED_CONTENT
            return {
                title: qsTr("Blocked by protected content"),
                detail: qsTr("Close any DRM-protected media (streaming apps, Blu-ray players, etc.) on the host PC, then try again.")
            }
        case -104: // ML_ERROR_FRAME_CONVERSION
            return {
                title: qsTr("The host reported a video encoding error"),
                detail: qsTr("Try disabling HDR, lowering the streaming resolution, or changing the host PC's display resolution.")
            }
        default:
            return {
                title: qsTr("Connection failed"),
                detail: qsTr("Error code %1.").arg(code)
            }
        }
    }

    onSessionChanged: {
        if (session) {
            state = "attempting"
            stageText = qsTr("Connecting…")
            errorCode = 0
            failingPorts = ""
            portTestResult = -1
        } else {
            state = "disconnected"
        }
    }

    Keys.onEscapePressed: if (state === "failed" || state === "reconnecting") root.quitRequested()
    Keys.onBackPressed: if (state === "failed" || state === "reconnecting") root.quitRequested()

    Timer {
        id: autoReconnectTimer
        interval: root.autoReconnectDelayMs
        onTriggered: root.reconnectRequested()
    }

    Connections {
        target: root.session

        function onStageStarting(stage) {
            root.state = "attempting"
            root.stageText = stage
        }

        function onConnectionStarted() {
            // The SDL video window is about to take over; nothing left for
            // this overlay to show until/unless the connection drops again.
            root.state = "connected"
        }

        function onStageFailed(stage, code, ports) {
            root.errorCode = code
            root.failingPorts = ports
            root.stageText = stage
            root.attemptAutoReconnect()
        }

        function onConnectionTerminated(code, ports) {
            root.errorCode = code
            root.failingPorts = ports

            if (code === 0) { // ML_ERROR_GRACEFUL_TERMINATION
                root.reconnectAttempt = 0
                root.state = "disconnected"
            } else {
                root.attemptAutoReconnect()
            }
        }

        function onSessionFinished(portTestResult) {
            root.portTestResult = portTestResult
        }

        function onQuitStarting() {
            root.state = "disconnected"
        }
    }

    // Full-window scrim behind the status card.
    Rectangle {
        anchors.fill: parent
        color: Tokens.surface
        opacity: 0.92
    }

    // Small fading-dots spinner; QtQuick-only (no Controls BusyIndicator).
    component Spinner: Item {
        id: spinner
        width: 64
        height: 64

        Repeater {
            model: 8
            delegate: Rectangle {
                width: 8
                height: 8
                radius: 4
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

    // Controller-focusable CTA. 96dp-tall focus target per Tokens.rowHeight;
    // visual language mirrors NavigableButton without depending on
    // QtQuick.Controls.
    component ActionButton: Rectangle {
        id: btn

        property alias text: label.text
        property bool primary: false

        signal activated()

        width: Math.max(220, label.implicitWidth + 64)
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

    Column {
        id: content
        anchors.centerIn: parent
        spacing: Tokens.gutter
        width: Math.min(root.width - Tokens.gutter * 4, 560)

        Spinner {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.state === "attempting" || root.state === "reconnecting"
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: Tokens.textPrimary
            font.family: Tokens.familyDisplay
            font.pointSize: Tokens.sizeTitle
            font.bold: true
            text: {
                switch (root.state) {
                case "attempting":
                    return root.stageText.length > 0 ? qsTr("Starting %1…").arg(root.stageText) : qsTr("Connecting…")
                case "reconnecting":
                    return qsTr("Reconnecting… (attempt %1 of %2)").arg(root.reconnectAttempt).arg(root.maxAutoReconnectAttempts)
                case "failed":
                    return root.guidance(root.errorCode).title
                default:
                    return ""
                }
            }
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            visible: text.length > 0
            color: Tokens.textSecondary
            font.family: Tokens.familyBody
            font.pointSize: Tokens.sizeBody
            text: {
                if (root.state !== "failed") {
                    return ""
                }
                var detail = root.guidance(root.errorCode).detail
                if (root.failingPorts.length > 0) {
                    detail += "\n\n" + qsTr("Ports involved: %1").arg(root.failingPorts)
                }
                if (root.portTestResult > 0) {
                    detail += "\n\n" + qsTr("This PC's Internet connection is blocking Jochona. Streaming over the Internet may not work while connected to this network.")
                }
                return detail
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Tokens.gutter
            visible: root.state === "failed"

            ActionButton {
                id: reconnectButton
                primary: true
                text: Glyphs.glyph(Glyphs.family, "a") + "  " + qsTr("Reconnect")
                onActivated: root.manualReconnect()
                Keys.onRightPressed: if (testButton.visible) testButton.forceActiveFocus(Qt.TabFocus); else quitButton.forceActiveFocus(Qt.TabFocus)
            }

            ActionButton {
                id: testButton
                visible: root.failingPorts.length > 0
                text: Glyphs.glyph(Glyphs.family, "x") + "  " + qsTr("Guided Connection Test")
                onActivated: root.runConnectionTestRequested()
                Keys.onLeftPressed: reconnectButton.forceActiveFocus(Qt.TabFocus)
                Keys.onRightPressed: quitButton.forceActiveFocus(Qt.TabFocus)
            }

            ActionButton {
                id: quitButton
                text: Glyphs.glyph(Glyphs.family, "b") + "  " + qsTr("Quit")
                onActivated: root.quitRequested()
                Keys.onLeftPressed: if (testButton.visible) testButton.forceActiveFocus(Qt.TabFocus); else reconnectButton.forceActiveFocus(Qt.TabFocus)
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            visible: root.state === "reconnecting"

            ActionButton {
                text: Glyphs.glyph(Glyphs.family, "b") + "  " + qsTr("Cancel")
                onActivated: {
                    autoReconnectTimer.stop()
                    root.quitRequested()
                }
            }
        }
    }

    onStateChanged: {
        if (state === "failed") {
            reconnectButton.forceActiveFocus(Qt.TabFocus)
        }
    }
}
