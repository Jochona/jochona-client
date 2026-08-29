import QtQuick 2.15
import QtQuick.Controls 2.2

import "style"

// Input-adaptive action hint. Existing call sites may still set `button: "A"`;
// the logical name resolves to the active controller family's authored glyph.
Row {
    id: hint

    property string button: "A"
    property string logicalButton: button.toLowerCase()
    property alias label: actionLabel.text
    readonly property string glyphValue: Glyphs.glyph(Glyphs.family, logicalButton)

    spacing: Tokens.dp(8)
    visible: Tokens.inputMode !== "pointer"

    Accessible.role: Accessible.StaticText
    Accessible.name: button + " " + actionLabel.text

    Rectangle {
        width: Tokens.dp(28)
        height: width
        radius: width / 2
        color: Tokens.surface
        border.width: Tokens.routeStroke
        border.color: Tokens.border
        anchors.verticalCenter: parent.verticalCenter

        Label {
            anchors.centerIn: parent
            text: hint.glyphValue.length > 0 ? hint.glyphValue : hint.button
            font.pixelSize: hint.glyphValue.length > 0 ? Tokens.dp(18) : Tokens.tMicro
            font.family: hint.glyphValue.length > 0
                         ? Glyphs.fontFamily(Glyphs.family)
                         : Tokens.familyBody
            font.bold: hint.glyphValue.length === 0
            color: Tokens.textPrimary
        }
    }

    Label {
        id: actionLabel
        anchors.verticalCenter: parent.verticalCenter
        font.pixelSize: Tokens.tMicro
        font.family: Tokens.familyBody
        font.weight: Font.Medium
        color: Tokens.textSecondary
    }
}
