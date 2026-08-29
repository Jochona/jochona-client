import QtQuick 2.15
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import Session 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0

import ".."
import "../style"

Item {
    id: root
    anchors.fill: parent
    visible: false
    focus: visible
    z: 1200

    property Session session: null
    property int selectedWidth: 1920
    property int selectedHeight: 1080
    property int selectedFps: 60
    property int selectedBitrateKbps: 20000
    property string selectedCodec: "auto"
    property bool selectedHdr: false
    property bool selectedVirtualDisplay: false
    property int selectedAudioConfig: StreamingPreferences.AC_STEREO
    property string selectedAudioDevice: ""
    property string saveScope: "session"


    function shotOpenPreview() {
        visible = true
        return true
    }
    signal closeRequested()
    signal applyRequested(var restartPatch, string saveScope)

    function open() {
        if (!session)
            return
        var settings = session.currentSettings
        selectedWidth = settings.width
        selectedHeight = settings.height
        selectedFps = settings.fps
        selectedBitrateKbps = settings.bitrateKbps
        selectedCodec = settings.videocfg === StreamingPreferences.VCC_FORCE_H264
                        ? "h264"
                        : settings.videocfg === StreamingPreferences.VCC_FORCE_HEVC
                          ? "hevc"
                          : settings.videocfg === StreamingPreferences.VCC_FORCE_AV1
                            ? "av1" : "auto"
        selectedHdr = settings.hdr
        selectedVirtualDisplay = settings.virtualdisplay === true
        selectedAudioConfig = settings.audiocfg
        selectedAudioDevice = settings.audiodevice || ""
        saveScope = "session"
        visible = true
        forceActiveFocus()
        Qt.callLater(function() { volumeSlider.forceActiveFocus() })
    }

    function close() {
        visible = false
        closeRequested()
    }

    function applyAndReconnect() {
        applyRequested({
            width: selectedWidth,
            height: selectedHeight,
            fps: selectedFps,
            bitrateKbps: selectedBitrateKbps,
            codec: selectedCodec,
            hdr: selectedHdr,
            virtualdisplay: selectedVirtualDisplay,
            audiocfg: selectedAudioConfig,
            audiodevice: selectedAudioDevice,
            sessionvolumedb: session.sessionVolumeDb
        }, saveScope)
    }

    Keys.onEscapePressed: close()
    Keys.onBackPressed: close()

    Rectangle { anchors.fill: parent; color: Tokens.night }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.gutter
        spacing: Tokens.gutter

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTr("Session settings")
                font.family: Tokens.familyDisplay
                font.pixelSize: Tokens.tTitle
                font.weight: Font.Medium
                color: Tokens.textPrimary
                Accessible.role: Accessible.Heading
            }
            Label {
                text: qsTr("LIVE + RECONNECT")
                font.family: Tokens.familyBody
                font.pixelSize: Tokens.tChip
                color: Tokens.link
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            RowLayout {
                width: root.width - Tokens.gutter * 2
                spacing: Tokens.gutter

                SettingsSection {
                    Layout.fillWidth: true
                    Layout.preferredWidth:
                        (root.width - Tokens.gutter * 3) / 2
                    Layout.alignment: Qt.AlignTop
                    title: qsTr("Live now")

                    Column {
                        anchors.fill: parent
                        spacing: Tokens.gutterTight
                        Label {
                            text: qsTr("Session Volume")
                            font.family: Tokens.familyDisplay
                            font.pixelSize: Tokens.tCard
                            color: Tokens.textPrimary
                        }
                        Row {
                            width: parent.width
                            spacing: Tokens.gutterTight
                            Slider {
                                id: volumeSlider
                                width: parent.width * 0.72
                                from: -60
                                to: 0
                                stepSize: 1
                                value: root.session ? root.session.sessionVolumeDb : 0
                                onMoved: if (root.session)
                                             root.session.setSessionVolumeDb(value)
                                Accessible.name: qsTr("Session Volume")
                            }
                            Label {
                                text: volumeSlider.value <= -60
                                      ? qsTr("Muted")
                                      : qsTr("%1 dB").arg(Math.round(volumeSlider.value))
                                color: Tokens.textSecondary
                            }
                        }
                        CheckBox {
                            width: parent.width
                            text: qsTr("Show performance overlay")
                            checked: root.session
                                     ? root.session.performanceOverlayEnabled : false
                            onToggled: if (root.session)
                                           root.session.setPerformanceOverlayEnabled(checked)
                        }
                        Label {
                            width: parent.width
                            text: root.session && root.session.hostVolumeAvailable
                                  ? qsTr("Host Volume capability detected. Session Volume remains local to this Client.")
                                  : qsTr("Host Volume is unavailable on this Host. Session Volume is local to this Client.")
                            color: Tokens.textSecondary
                            wrapMode: Text.WordWrap
                        }
                        NavigableButton {
                            width: parent.width
                            enabled: false
                            text: qsTr("Microphone forwarding — Planned")
                            description: qsTr("Not negotiated in this beta")
                        }
                    }
                }

                SettingsSection {
                    Layout.fillWidth: true
                    Layout.preferredWidth:
                        (root.width - Tokens.gutter * 3) / 2
                    Layout.alignment: Qt.AlignTop
                    title: qsTr("Reconnect required")

                    Column {
                        anchors.fill: parent
                        spacing: Tokens.gutterTight

                        Label { text: qsTr("Resolution"); color: Tokens.textPrimary }
                        AutoResizingComboBox {
                            width: parent.width
                            textRole: "name"
                            model: [
                                {name: "1920×1080", widthValue: 1920, heightValue: 1080},
                                {name: "2560×1440", widthValue: 2560, heightValue: 1440},
                                {name: "3840×2160", widthValue: 3840, heightValue: 2160}
                            ]
                            currentIndex: root.selectedWidth >= 3840 ? 2
                                          : root.selectedWidth >= 2560 ? 1 : 0
                            onActivated: {
                                root.selectedWidth = model[currentIndex].widthValue
                                root.selectedHeight = model[currentIndex].heightValue
                            }
                        }

                        Label { text: qsTr("Frame rate"); color: Tokens.textPrimary }
                        AutoResizingComboBox {
                            width: parent.width
                            model: [30, 60, 120]
                            currentIndex: root.selectedFps >= 120 ? 2
                                          : root.selectedFps >= 60 ? 1 : 0
                            onActivated: root.selectedFps = model[currentIndex]
                        }

                        Label {
                            text: qsTr("Bitrate · %1 Mbps")
                                  .arg(Math.round(root.selectedBitrateKbps / 1000))
                            color: Tokens.textPrimary
                        }
                        Slider {
                            width: parent.width
                            from: 5000
                            to: 150000
                            stepSize: 5000
                            value: root.selectedBitrateKbps
                            onMoved: root.selectedBitrateKbps = value
                            Accessible.name: qsTr("Queued bitrate")
                        }

                        Label { text: qsTr("Codec"); color: Tokens.textPrimary }
                        AutoResizingComboBox {
                            width: parent.width
                            model: [qsTr("Automatic"), "H.264", "HEVC", "AV1"]
                            currentIndex:
                                Math.max(0, ["auto", "h264", "hevc", "av1"]
                                         .indexOf(root.selectedCodec))
                            onActivated:
                                root.selectedCodec = ["auto", "h264", "hevc", "av1"]
                                                     [currentIndex]
                        }
                        CheckBox {
                            width: parent.width
                            text: qsTr("HDR")
                            enabled: SystemProperties.activeDisplaySupportsHdr
                            checked: root.selectedHdr
                            onToggled: root.selectedHdr = checked
                        }
                        CheckBox {
                            width: parent.width
                            text: qsTr("Use Host virtual display")
                            checked: root.selectedVirtualDisplay
                            onToggled: root.selectedVirtualDisplay = checked
                        }

                        Label { text: qsTr("Audio layout"); color: Tokens.textPrimary }
                        AutoResizingComboBox {
                            width: parent.width
                            model: [qsTr("Stereo"), qsTr("5.1 surround"),
                                    qsTr("7.1 surround")]
                            currentIndex: root.selectedAudioConfig
                            onActivated: root.selectedAudioConfig = currentIndex
                        }

                        Label { text: qsTr("Audio output"); color: Tokens.textPrimary }
                        AutoResizingComboBox {
                            width: parent.width
                            model: [qsTr("System default")].concat(
                                       SystemProperties.audioOutputDevices)
                            currentIndex: root.selectedAudioDevice.length === 0
                                          ? 0
                                          : Math.max(0,
                                              SystemProperties.audioOutputDevices
                                                  .indexOf(root.selectedAudioDevice) + 1)
                            onActivated:
                                root.selectedAudioDevice = currentIndex === 0
                                    ? "" : SystemProperties.audioOutputDevices[currentIndex - 1]
                        }

                        Label { text: qsTr("Save To"); color: Tokens.textPrimary }
                        AutoResizingComboBox {
                            width: parent.width
                            textRole: "name"
                            model: {
                                var choices = [
                                    {name: qsTr("This Session only"), scope: "session"},
                                    {name: qsTr("This Device ↔ Rig"), scope: "host_client_pair"}
                                ]
                                if (root.session && root.session.libraryEntryId.length > 0)
                                    choices.push({name: qsTr("Library Entry"), scope: "library_entry"})
                                choices.push({name: qsTr("Host Application"), scope: "host_application"})
                                return choices
                            }
                            onActivated: root.saveScope = model[currentIndex].scope
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Item { Layout.fillWidth: true }
            NavigableButton {
                text: qsTr("Keep current Session")
                onClicked: root.close()
            }
            NavigableButton {
                text: qsTr("Apply & reconnect")
                primary: true
                onClicked: root.applyAndReconnect()
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Open: Ctrl+Alt+Shift+O or Back+L1+R1+A")
            horizontalAlignment: Text.AlignHCenter
            color: Tokens.textSecondary
            font.pixelSize: Tokens.tMicro
        }
    }
}
