// Message-dialog compatibility on the Night Route context-panel primitive.
// Safe/cancel actions receive initial focus; copy names actions at call sites.
import QtQuick 2.15
import QtQuick.Controls 2.2

import "style"

QuickSheet {
    id: dialog

    property string text: ""
    property bool showSpinner: false
    property url imageSrc: ""
    property string helpText: ""
    property string helpUrl: ""
    property string helpTextSeparator: " "
    property int standardButtons: Dialog.Ok
    property string okText: qsTr("Done")
    property string yesText: qsTr("Continue")
    property string noText: qsTr("Cancel")
    property string cancelText: qsTr("Cancel")

    initialFocusItem: noButton.visible ? noButton
                      : cancelButton.visible ? cancelButton
                      : okButton.visible ? okButton : yesButton

    Row {
        width: parent.width
        spacing: Tokens.gutterTight

        BusyIndicator {
            visible: dialog.showSpinner
            running: visible
            width: Tokens.dp(38)
            height: width
            anchors.verticalCenter: parent.verticalCenter
        }

        Image {
            visible: !dialog.showSpinner && dialog.imageSrc.toString().length > 0
            source: dialog.imageSrc
            sourceSize.width: Tokens.dp(42)
            sourceSize.height: Tokens.dp(42)
            anchors.verticalCenter: parent.verticalCenter
        }

        Label {
            width: parent.width
                   - (dialog.showSpinner
                      || dialog.imageSrc.toString().length > 0
                      ? Tokens.dp(52) : 0)
            text: dialog.text
                  + (dialog.helpText.length > 0
                     && (dialog.standardButtons & Dialog.Help)
                     ? dialog.helpTextSeparator + dialog.helpText : "")
            font.family: Tokens.familyBody
            font.pixelSize: Tokens.tMeta
            color: Tokens.textPrimary
            wrapMode: Text.WordWrap
        }
    }

    Flow {
        width: parent.width
        spacing: Tokens.gutterTight

        NavigableButton {
            id: helpButton
            visible: (dialog.standardButtons & Dialog.Help) !== 0
                     && dialog.helpUrl.length > 0
            text: qsTr("Open help")
            onClicked: {
                Qt.openUrlExternally(dialog.helpUrl)
                dialog.close()
            }
        }

        NavigableButton {
            id: yesButton
            visible: (dialog.standardButtons & Dialog.Yes) !== 0
            text: dialog.yesText
            onClicked: dialog.accept()
        }

        NavigableButton {
            id: okButton
            visible: (dialog.standardButtons & Dialog.Ok) !== 0
            text: dialog.okText
            primary: true
            onClicked: dialog.accept()
        }

        NavigableButton {
            id: noButton
            visible: (dialog.standardButtons & Dialog.No) !== 0
            text: dialog.noText
            primary: visible
            onClicked: dialog.reject()
        }

        NavigableButton {
            id: cancelButton
            visible: (dialog.standardButtons & Dialog.Cancel) !== 0
            text: dialog.cancelText
            primary: visible && !noButton.visible
            onClicked: dialog.reject()
        }
    }
}
