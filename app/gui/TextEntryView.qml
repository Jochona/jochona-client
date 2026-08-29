// Dedicated controller-first text entry. Full-screen by design: typing is a
// focused task, and the same surface supports physical keyboards and pointer.
import QtQuick 2.15
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import "style"

FocusScope {
    id: entry

    property string title: qsTr("Enter text")
    property string prompt: ""
    property string initialText: ""
    property string placeholderText: ""
    property string submitLabel: qsTr("Save")
    property bool allowEmpty: false
    property alias text: keyboard.text
    property string errorText: ""

    signal accepted(string value)
    signal cancelled()

    objectName: title

    function submit() {
        var value = keyboard.text.trim()
        if (!allowEmpty && value.length === 0) {
            errorText = qsTr("Enter a value before continuing.")
            return
        }
        errorText = ""
        accepted(value)
    }

    Component.onCompleted: {
        keyboard.text = initialText
        keyboard.forceActiveFocus()
    }

    StackView.onActivated: keyboard.forceActiveFocus()

    Keys.onEscapePressed: function(event) {
        cancelled()
        event.accepted = true
    }
    Keys.onBackPressed: function(event) {
        cancelled()
        event.accepted = true
    }

    Rectangle {
        anchors.fill: parent
        color: Tokens.night
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Tokens.gutter

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Tokens.gutterTight

            Label {
                Layout.fillWidth: true
                text: entry.title
                font.family: Tokens.familyDisplay
                font.pixelSize: Tokens.tTitle
                font.weight: Font.Medium
                color: Tokens.textPrimary
                Accessible.role: Accessible.Heading
            }

            Label {
                Layout.fillWidth: true
                visible: entry.prompt.length > 0
                text: entry.prompt
                font.family: Tokens.familyBody
                font.pixelSize: Tokens.tMeta
                color: Tokens.textSecondary
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(Tokens.actionHeight,
                                                 valueLabel.implicitHeight
                                                 + Tokens.gutterTight * 2)
                radius: Tokens.radiusControl
                color: Tokens.surface
                border.width: keyboard.activeFocus
                              ? Tokens.focusStroke : Tokens.routeStroke
                border.color: keyboard.activeFocus
                             ? Tokens.borderFocus : Tokens.border

                Label {
                    id: valueLabel
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: Tokens.gutterTight
                    text: keyboard.text.length > 0
                          ? keyboard.text : entry.placeholderText
                    font.family: Tokens.familyBody
                    font.pixelSize: Tokens.tMeta
                    color: keyboard.text.length > 0
                           ? Tokens.textPrimary : Tokens.textSecondary
                    elide: Text.ElideMiddle
                    Accessible.role: Accessible.EditableText
                    Accessible.name: entry.prompt.length > 0
                                     ? entry.prompt : entry.title
                    Accessible.value: keyboard.text
                }
            }

            Label {
                Layout.fillWidth: true
                visible: entry.errorText.length > 0
                text: entry.errorText
                font.family: Tokens.familyBody
                font.pixelSize: Tokens.tChip
                color: Tokens.statusOffline
                wrapMode: Text.WordWrap
                Accessible.role: Accessible.AlertMessage
            }
        }

        OnScreenKeyboard {
            id: keyboard
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.maximumWidth: Tokens.dp(900)
            Layout.alignment: Qt.AlignHCenter
            doneLabel: entry.submitLabel
            onAccepted: entry.submit()
            onNavigateDown: saveButton.forceActiveFocus()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Tokens.gutterTight

            Item { Layout.fillWidth: true }

            NavigableButton {
                id: cancelButton
                text: qsTr("Cancel")
                Keys.onUpPressed: keyboard.forceActiveFocus()
                onClicked: entry.cancelled()
            }

            NavigableButton {
                id: saveButton
                text: entry.submitLabel
                primary: true
                Keys.onUpPressed: keyboard.forceActiveFocus()
                onClicked: entry.submit()
            }
        }
    }
}
