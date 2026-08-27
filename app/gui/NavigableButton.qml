import QtQuick 2.15
import QtQuick.Controls 2.2

import "style"

// Jochona: controller-first CTA button for the modern screens (M2).
// Follows the shipped focus language (HomeView rows): resting card surface,
// focused = brighter fill + 2px borderFocus ring. `primary` fills with the
// accent so one action per screen reads as the default. Return/Enter (gamepad
// A) activates; left/right walk the focus chain like NavigableToolButton.
Button {
    id: control

    property bool primary: false

    activeFocusOnTab: true

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    Keys.onRightPressed: {
        nextItemInFocusChain(true).forceActiveFocus(Qt.TabFocus)
    }

    Keys.onLeftPressed: {
        nextItemInFocusChain(false).forceActiveFocus(Qt.TabFocus)
    }

    leftPadding: 26
    rightPadding: 26
    topPadding: 13
    bottomPadding: 13

    contentItem: Label {
        text: control.text
        font.pointSize: Tokens.sizeBody
        font.family: Tokens.familyBody
        font.weight: Font.DemiBold
        // The lit (focused/hovered) primary fill is bright, so its label
        // flips dark — derived from the fill itself, keeping 4.5:1 in any
        // theme without a new literal.
        color: control.primary && (control.activeFocus || control.hovered)
               ? Qt.darker(Tokens.accentFocus, 3.4)
               : Tokens.textPrimary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight

        Behavior on color {
            ColorAnimation { duration: Tokens.motion(Tokens.durationFast) }
        }
    }

    background: Rectangle {
        radius: height / 2
        color: control.primary
               ? (control.activeFocus || control.hovered ? Tokens.accentFocus : Tokens.accent)
               : (control.activeFocus || control.hovered ? Tokens.surfaceFocus : Tokens.surface)
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus
                      ? (control.primary ? Tokens.textPrimary : Tokens.borderFocus)
                      : (control.primary ? Tokens.accentFocus : Tokens.border)
        opacity: control.enabled ? 1.0 : 0.45

        Behavior on color {
            ColorAnimation { duration: Tokens.motion(Tokens.durationFast) }
        }
    }
}
