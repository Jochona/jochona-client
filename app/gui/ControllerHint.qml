import QtQuick 2.15
import QtQuick.Controls 2.2

import "style"

// Jochona: one controller-legend chip — a drawn face-button badge plus the
// action it performs on this screen ("A Select", "B Back"). Self-contained
// so pairing-era screens don't depend on the glyph font packs.
Row {
    // Face-button letters are hardware labels, not translatable copy.
    property string button: "A"
    property alias label: actionLabel.text

    spacing: 7

    Rectangle {
        width: 22
        height: 22
        radius: 11
        color: "transparent"
        border.width: 1
        border.color: Tokens.textSecondary
        anchors.verticalCenter: parent.verticalCenter

        Label {
            anchors.centerIn: parent
            text: button
            font.pointSize: Tokens.sizeMicro
            font.family: Tokens.familyBody
            font.bold: true
            color: Tokens.textSecondary
        }
    }

    Label {
        id: actionLabel
        anchors.verticalCenter: parent.verticalCenter
        font.pointSize: Tokens.sizeMicro
        font.family: Tokens.familyBody
        color: Tokens.textSecondary
    }
}
