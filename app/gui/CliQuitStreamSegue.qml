import QtQuick 2.15
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import ComputerManager 1.0

import "style"

Item {
    id: cliQuit

    function onSearchingComputer() {
        stageLabel.text = qsTr("Finding the rig…")
    }
    function onQuittingApp() {
        stageLabel.text = qsTr("Stopping the application on the rig…")
    }
    function onFailure(message) {
        errorDialog.text = message
        errorDialog.open()
    }

    StackView.onActivated: {
        if (!launcher.isExecuted()) {
            launcher.searchingComputer.connect(onSearchingComputer)
            launcher.quittingApp.connect(onQuittingApp)
            launcher.failed.connect(onFailure)
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
            running: true
            width: Tokens.dp(48)
            height: width
        }

        Label {
            id: stageLabel
            Layout.fillWidth: true
            text: qsTr("Preparing the rig…")
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
}
