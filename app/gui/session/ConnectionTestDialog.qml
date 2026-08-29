// Full-screen Night Route connection diagnostics. It can run live from a rig
// or display the completed result retained by a failed session.
import QtQuick 2.15
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import EffectiveSettings 1.0

import ".."
import "../style"

Item {
    id: root
    anchors.fill: parent
    z: 1000
    visible: false

    property Item returnFocusItem: null
    property Item capturedFocusItem: null
    property int result: -1
    property bool testing: false
    property bool retryAvailable: true
    property string pairKey: ""
    property bool pairPatchApplied: false

    function applyConservativePairPatch() {
        var bundle = EffectiveSettings.patch("host_client_pair", pairKey)
        var values = bundle.values || ({})
        values.width = 1920
        values.height = 1080
        values.fps = 60
        values.bitrateKbps = 20000
        values.codec = "auto"
        pairPatchApplied = EffectiveSettings.setPatch(
            "host_client_pair", pairKey, values,
            bundle.pins || ({}), bundle.floors || ({}))
    }

    signal retryRequested()

    readonly property var portCatalog: [
        { flag: 0x0001, proto: qsTr("TCP"), port: 47984,
          purpose: qsTr("Discovery and legacy pairing"),
          guidance: qsTr("Allow TCP 47984 for older GameStream discovery and pairing.") },
        { flag: 0x0002, proto: qsTr("TCP"), port: 47989,
          purpose: qsTr("Pairing and library"),
          guidance: qsTr("Allow TCP 47989 so Jochona can pair and read the library.") },
        { flag: 0x0004, proto: qsTr("TCP"), port: 48010,
          purpose: qsTr("Stream handshake"),
          guidance: qsTr("Allow TCP 48010 to negotiate the stream.") },
        { flag: 0x0100, proto: qsTr("UDP"), port: 47998,
          purpose: qsTr("Video"),
          guidance: qsTr("Allow UDP 47998 for video from the rig.") },
        { flag: 0x0200, proto: qsTr("UDP"), port: 47999,
          purpose: qsTr("Controller and input"),
          guidance: qsTr("Allow UDP 47999 for controller, mouse, and keyboard input.") },
        { flag: 0x0400, proto: qsTr("UDP"), port: 48000,
          purpose: qsTr("Audio"),
          guidance: qsTr("Allow UDP 48000 for audio from the rig.") },
        { flag: 0x0800, proto: qsTr("UDP"), port: 48010,
          purpose: qsTr("Microphone and handshake"),
          guidance: qsTr("Allow UDP 48010 for the final handshake and supported microphones.") }
    ]

    readonly property int blockedCount: {
        var count = 0
        if (result > 0) {
            for (var i = 0; i < portCatalog.length; i++) {
                if ((result & portCatalog[i].flag) !== 0)
                    count += 1
            }
        }
        return count
    }

    function open() {
        var win = root.Window.window
        capturedFocusItem = returnFocusItem !== null
                            ? returnFocusItem
                            : win !== null ? win.activeFocusItem : null
        testing = true
        result = -1
        pairPatchApplied = false
        visible = true
        forceActiveFocus()
        closeButton.forceActiveFocus()
    }

    function close() {
        visible = false
        var restore = capturedFocusItem
        Qt.callLater(function() {
            if (restore !== null && restore.parent !== null)
                restore.forceActiveFocus()
        })
    }

    function connectionTestComplete(testResult, blockedPorts) {
        testing = false
        result = testResult
    }

    Keys.onEscapePressed: root.close()
    Keys.onBackPressed: root.close()

    Rectangle {
        anchors.fill: parent
        color: Tokens.night
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.gutter
        spacing: Tokens.gutter

        Label {
            Layout.fillWidth: true
            text: qsTr("Connection details")
            font.family: Tokens.familyDisplay
            font.pixelSize: Tokens.tTitle
            font.weight: Font.Medium
            color: Tokens.textPrimary
            Accessible.role: Accessible.Heading
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.gutterTight

            BusyIndicator {
                visible: root.testing
                running: visible
                width: Tokens.dp(38)
                height: width
            }

            Label {
                Layout.fillWidth: true
                text: {
                    if (root.testing)
                        return qsTr("Testing the required connection paths…")
                    if (root.result === -1)
                        return qsTr("The test could not reach Jochona’s test "
                                    + "servers. Check the Internet connection "
                                    + "or review the paths below.")
                    if (root.result === 0)
                        return qsTr("The tested network paths are open.")
                    return qsTr("%1 of %2 required paths are blocked.")
                           .arg(root.blockedCount).arg(root.portCatalog.length)
                }
                font.family: Tokens.familyBody
                font.pixelSize: Tokens.tMeta
                color: Tokens.textSecondary
                wrapMode: Text.WordWrap
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Column {
                width: parent.width
                spacing: Tokens.gutterTight

                Repeater {
                    model: !root.testing && root.result !== -1
                           ? root.portCatalog : []

                    delegate: Rectangle {
                        id: pathRow
                        required property var modelData
                        width: parent.width
                        height: pathContent.implicitHeight + Tokens.gutter
                        radius: Tokens.radiusControl
                        color: blocked ? Tokens.surfaceFocus : Tokens.surface
                        border.width: Tokens.routeStroke
                        border.color: blocked
                                      ? Tokens.statusOffline : Tokens.border

                        readonly property bool blocked:
                            (root.result & modelData.flag) !== 0

                        RowLayout {
                            id: pathContent
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.margins: Tokens.gutterTight
                            spacing: Tokens.gutterTight

                            Rectangle {
                                Layout.preferredWidth: Tokens.dp(9)
                                Layout.preferredHeight: width
                                Layout.alignment: Qt.AlignVCenter
                                radius: width / 2
                                color: pathRow.blocked
                                       ? Tokens.statusOffline : Tokens.statusOnline
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Tokens.dp(4)

                                Label {
                                    Layout.fillWidth: true
                                    text: qsTr("%1 %2 — %3")
                                          .arg(modelData.proto)
                                          .arg(modelData.port)
                                          .arg(modelData.purpose)
                                    font.family: Tokens.familyBody
                                    font.pixelSize: Tokens.tMeta
                                    font.weight: Font.DemiBold
                                    color: Tokens.textPrimary
                                    elide: Text.ElideRight
                                }

                                Label {
                                    Layout.fillWidth: true
                                    visible: pathRow.blocked
                                    text: modelData.guidance
                                    font.family: Tokens.familyBody
                                    font.pixelSize: Tokens.tMicro
                                    color: Tokens.textSecondary
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: recommendation.implicitHeight
                                    + Tokens.gutter * 2
            visible: !root.testing && root.pairKey.length > 0
            radius: Tokens.radiusControl
            color: Tokens.surface
            border.color: Tokens.border
            border.width: Tokens.routeStroke

            ColumnLayout {
                id: recommendation
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: Tokens.gutter
                spacing: Tokens.gutterTight
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Optional Pair patch")
                    font.family: Tokens.familyDisplay
                    font.pixelSize: Tokens.tCard
                    color: Tokens.textPrimary
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("This port test cannot measure throughput. "
                               + "For a remote or unstable route, Jochona can "
                               + "propose 1080p60 at 20 Mbps with an automatic "
                               + "codec for only this Device ↔ Rig Pair.")
                    color: Tokens.textSecondary
                    wrapMode: Text.WordWrap
                }
                NavigableButton {
                    text: root.pairPatchApplied
                          ? qsTr("Pair patch applied")
                          : qsTr("Apply conservative Pair patch")
                    enabled: !root.pairPatchApplied
                    onClicked: root.applyConservativePairPatch()
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.gutterTight

            Item { Layout.fillWidth: true }

            NavigableButton {
                id: retryButton
                visible: root.retryAvailable && !root.testing
                text: qsTr("Test again")
                onClicked: {
                    root.testing = true
                    root.result = -1
                    root.retryRequested()
                }
            }

            NavigableButton {
                id: closeButton
                text: qsTr("Done")
                primary: true
                onClicked: root.close()
            }
        }
    }
}
