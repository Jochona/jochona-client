import QtQuick 2.15
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import ComputerManager 1.0

import "style"

Item {
    id: cliPair

    function onSearchingComputer() {
        stageLabel.text = qsTr("Finding the rig…")
    }
    function onPairing(rigName, pin) {
        stageLabel.text = qsTr("Enter %1 on %2.").arg(pin).arg(rigName)
    }
    function onFailed(message) {
        stageIndicator.visible = false
        errorDialog.text = message
        errorDialog.open()
    }
    function onSuccess(appName) {
        stageIndicator.visible = false
        pairCompleteDialog.open()
    }

    Keys.onEscapePressed: Qt.quit()
    Keys.onBackPressed: Qt.quit()
    Keys.onCancelPressed: Qt.quit()

    StackView.onActivated: {
        if (!launcher.isExecuted()) {
            launcher.searchingComputer.connect(onSearchingComputer)
            launcher.pairing.connect(onPairing)
            launcher.failed.connect(onFailed)
            launcher.success.connect(onSuccess)
            launcher.execute(ComputerManager)
        }
    }

    Rectangle { anchors.fill: parent; color: Tokens.night }

    ColumnLayout {
        id: stageIndicator
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
            text: qsTr("Preparing pairing…")
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
        id: pairCompleteDialog
        closePolicy: Popup.CloseOnEscape
        text: qsTr("Pairing is complete.")
        standardButtons: Dialog.Ok
        okText: qsTr("Done")
        onClosed: Qt.quit()
    }
}
