// Visible host-side stop transition. It preserves the route while the host
// closes an application and never references retired shell chrome.
import QtQuick 2.15
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import ComputerManager 1.0
import Session 1.0

import "style"

Item {
    id: quitView

    property string appName: ""
    property var quitRunningAppFn
    property Session nextSession: null
    property string nextAppName: ""

    objectName: qsTr("Stop application")

    function quitAppCompleted(error)
    {
        if (error !== undefined) {
            errorDialog.text = error
            errorDialog.open()
            console.error(error)
        }

        if (error === undefined && nextSession !== null) {
            var component = Qt.createComponent("StreamSegue.qml")
            var segue = component.createObject(stackView, {
                                                   "appName": nextAppName,
                                                   "session": nextSession
                                               })
            stackView.replace(segue)
        } else {
            stackView.pop()
        }
    }

    StackView.onActivated: {
        ComputerManager.quitAppCompleted.connect(quitAppCompleted)
        if (quitRunningAppFn)
            quitRunningAppFn()
    }

    StackView.onDeactivating:
        ComputerManager.quitAppCompleted.disconnect(quitAppCompleted)

    Rectangle {
        anchors.fill: parent
        color: Tokens.night
    }

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
            Layout.fillWidth: true
            text: qsTr("Stopping %1 on the rig…").arg(appName)
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
            text: qsTr("The stream stays closed until the rig confirms.")
            font.family: Tokens.familyBody
            font.pixelSize: Tokens.tMeta
            color: Tokens.textSecondary
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }

    ErrorMessageDialog {
        id: errorDialog
    }
}
