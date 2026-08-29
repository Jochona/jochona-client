// Night Route connection and recovery surface. It owns the visible lifecycle
// before/after the native stream window, with one route and explicit recovery.
import QtQuick 2.15
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import Session 1.0

import ".."
import "../style"

Item {
    id: root
    anchors.fill: parent

    property Session session: null
    property string hostLabel: qsTr("Rig")
    property string destinationLabel: qsTr("Game")
    property int maxAutoReconnectAttempts: 3
    property int autoReconnectDelayMs: 2000

    // disconnected | attempting | connected | reconnecting | failed
    state: "disconnected"
    readonly property bool active: state === "attempting"
                                   || state === "reconnecting"
                                   || state === "failed"
    visible: active
    focus: visible
    z: 100

    property string stageText: ""
    property int errorCode: 0
    property string failingPorts: ""
    property int reconnectAttempt: 0
    property int portTestResult: -1
    property string failureTitle: ""
    property string failureDetail: ""

    signal reconnectRequested()
    signal quitRequested()
    signal detailsRequested()

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

    function guidance(code) {
        if (failureTitle.length > 0) {
            return { title: failureTitle, detail: failureDetail }
        }
        switch (code) {
        case -100:
            return {
                title: qsTr("No video arrived from the rig"),
                detail: qsTr("A router or firewall is probably blocking the "
                             + "video path. Open connection details, then try again.")
            }
        case -101:
            return {
                title: qsTr("The connection is too unstable"),
                detail: qsTr("Lower the bitrate or move to a faster, steadier network.")
            }
        case -102:
            return {
                title: qsTr("The stream stopped during startup"),
                detail: qsTr("Restart the host service or close software using "
                             + "the rig’s GPU, then try again.")
            }
        case -103:
            return {
                title: qsTr("Protected content blocked the stream"),
                detail: qsTr("Close DRM-protected media on the rig, then try again.")
            }
        case -104:
            return {
                title: qsTr("The rig could not encode this video mode"),
                detail: qsTr("Disable HDR, lower the resolution, or change the "
                             + "rig’s display mode.")
            }
        default:
            return {
                title: qsTr("Connection failed"),
                detail: qsTr("Jochona received error code %1. Open connection "
                             + "details or try again.").arg(code)
            }
        }
    }

    onSessionChanged: {
        if (session) {
            state = "attempting"
            stageText = qsTr("Connecting")
            errorCode = 0
            failingPorts = ""
            failureTitle = ""
            failureDetail = ""
            portTestResult = -1
        } else {
            state = "disconnected"
        }
    }

    Keys.onEscapePressed: {
        if (state === "failed" || state === "reconnecting")
            root.quitRequested()
    }
    Keys.onBackPressed: {
        if (state === "failed" || state === "reconnecting")
            root.quitRequested()
    }

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
            if (code === 0) {
                root.reconnectAttempt = 0
                root.state = "disconnected"
            } else {
                root.attemptAutoReconnect()
            }
        }
        function onSessionFinished(result) {
            root.portTestResult = result
        }
        function onQuitStarting() {
            root.state = "disconnected"
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Tokens.night
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.gutter
        spacing: Tokens.gutter

        Item { Layout.fillHeight: true }

        Label {
            Layout.fillWidth: true
            text: {
                if (root.state === "attempting")
                    return root.stageText.length > 0
                           ? qsTr("Starting %1…").arg(root.stageText)
                           : qsTr("Connecting…")
                if (root.state === "reconnecting")
                    return qsTr("Reconnecting — attempt %1 of %2")
                           .arg(root.reconnectAttempt)
                           .arg(root.maxAutoReconnectAttempts)
                if (root.state === "failed")
                    return root.guidance(root.errorCode).title
                return ""
            }
            font.family: Tokens.familyDisplay
            font.pixelSize: Tokens.tTitle
            font.weight: Font.Medium
            color: Tokens.textPrimary
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.Heading
        }

        Label {
            Layout.fillWidth: true
            Layout.maximumWidth: Tokens.dp(760)
            Layout.alignment: Qt.AlignHCenter
            visible: root.state === "failed"
            text: root.guidance(root.errorCode).detail
            font.family: Tokens.familyBody
            font.pixelSize: Tokens.tMeta
            color: Tokens.textSecondary
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Item {
            id: connectionRoute
            Layout.fillWidth: true
            Layout.maximumWidth: Tokens.dp(820)
            Layout.preferredHeight: Tokens.dp(104)
            Layout.alignment: Qt.AlignHCenter

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: parent.width / 6
                anchors.rightMargin: parent.width / 6
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -Tokens.dp(16)
                height: Tokens.routeStroke
                color: root.state === "failed" ? Tokens.statusOffline : Tokens.link
            }

            Row {
                anchors.fill: parent

                Repeater {
                    model: [qsTr("This device"), root.hostLabel,
                            root.destinationLabel]

                    delegate: Item {
                        required property var modelData
                        required property int index
                        width: connectionRoute.width / 3
                        height: connectionRoute.height

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.verticalCenterOffset: -Tokens.dp(16)
                            width: Tokens.dp(index === 1 ? 14 : 9)
                            height: width
                            radius: width / 2
                            color: root.state === "failed" && index >= 1
                                   ? Tokens.statusOffline
                                   : index === 1 ? Tokens.moon : Tokens.link
                        }

                        Label {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.verticalCenter
                            text: modelData
                            font.family: Tokens.familyBody
                            font.pixelSize: Tokens.tMicro
                            color: Tokens.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: root.state === "attempting" || root.state === "reconnecting"
            running: visible
            width: Tokens.dp(48)
            height: width
        }

        Flow {
            Layout.fillWidth: true
            Layout.maximumWidth: Tokens.dp(900)
            Layout.alignment: Qt.AlignHCenter
            spacing: Tokens.gutterTight
            visible: root.state === "failed" || root.state === "reconnecting"

            NavigableButton {
                id: reconnectButton
                visible: root.state === "failed"
                text: qsTr("Reconnect")
                primary: true
                onClicked: root.manualReconnect()
            }

            NavigableButton {
                visible: root.state === "failed"
                text: qsTr("Connection details")
                onClicked: root.detailsRequested()
            }

            NavigableButton {
                text: root.state === "reconnecting"
                      ? qsTr("Cancel reconnect") : qsTr("End session")
                destructive: true
                onClicked: {
                    autoReconnectTimer.stop()
                    root.quitRequested()
                }
            }
        }

        Item { Layout.fillHeight: true }
    }

    onStateChanged: {
        if (state === "failed")
            Qt.callLater(function() { reconnectButton.forceActiveFocus() })
    }
}
