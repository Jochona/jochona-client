import QtQuick 2.15
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import ComputerManager 1.0

import "style"

Item {
    id: cliStart

    function onSearchingComputer() {
        stageLabel.text = qsTr("Finding the rig…")
    }
    function onSearchingApp() {
        stageLabel.text = qsTr("Reading the rig’s library…")
    }
    function onSessionCreated(appName, session) {
        var component = Qt.createComponent("StreamSegue.qml")
        var segue = component.createObject(stackView, {
            "appName": appName,
            "session": session,
            "quitAfter": true
        })
        stackView.push(segue)
    }
    function onLaunchFailed(message) {
        errorDialog.text = message
        errorDialog.open()
        console.error(message)
    }
    function onAppQuitRequired(appName) {
        quitAppDialog.appName = appName
        quitAppDialog.open()
    }

    StackView.onActivated: {
        if (!launcher.isExecuted()) {
            launcher.searchingComputer.connect(onSearchingComputer)
            launcher.searchingApp.connect(onSearchingApp)
            launcher.sessionCreated.connect(onSessionCreated)
            launcher.failed.connect(onLaunchFailed)
            launcher.appQuitRequired.connect(onAppQuitRequired)
            launcher.execute(ComputerManager)
        }
    }

    Rectangle { anchors.fill: parent; color: Tokens.night }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Tokens.gutter * 2, Tokens.dp(720))
        spacing: Tokens.gutter

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: visible
            width: Tokens.dp(48)
            height: width
        }

        Label {
            id: stageLabel
            Layout.fillWidth: true
            text: qsTr("Preparing the stream…")
            font.family: Tokens.familyDisplay
            font.pixelSize: Tokens.tTitle
            font.weight: Font.Medium
            color: Tokens.textPrimary
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.Heading
        }
    }

    ErrorMessageDialog {
        id: errorDialog
        onClosed: Qt.quit()
    }

    NavigableMessageDialog {
        id: quitAppDialog
        property string appName: ""

        text: qsTr("Stop %1 on the rig before starting the new stream?")
              .arg(appName)
        standardButtons: Dialog.Yes | Dialog.No
        yesText: qsTr("Stop application")
        noText: qsTr("Cancel")

        function quitApp() {
            var component = Qt.createComponent("QuitSegue.qml")
            var parameters = {
                "appName": appName,
                "quitRunningAppFn": function() { launcher.quitRunningApp() }
            }
            stackView.push(component.createObject(stackView, parameters))
        }

        onAccepted: quitApp()
        onRejected: Qt.quit()
    }
}
