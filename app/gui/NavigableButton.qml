import QtQuick 2.15
import QtQuick.Controls 2.2

import "style"

// Night Route action primitive. It carries one literal action, exposes native
// accessibility semantics, and uses the same route trace as every focusable
// destination. Pills are reserved for compact choice chips.
Button {
    id: control

    property bool primary: false
    property bool compact: false
    property bool destructive: false
    property string description: ""
    property string accessibleName: text

    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true
    hoverEnabled: true

    implicitHeight: Math.max(compact ? Tokens.dp(44) : Tokens.actionHeight,
                             contentItem.implicitHeight + topPadding + bottomPadding)
    implicitWidth: Math.max(compact ? Tokens.dp(104) : Tokens.dp(148),
                            contentItem.implicitWidth + leftPadding + rightPadding)
    leftPadding: compact ? Tokens.dp(18) : Tokens.dp(24)
    rightPadding: leftPadding
    topPadding: Tokens.dp(10)
    bottomPadding: Tokens.dp(10)

    Keys.onReturnPressed: clicked()
    Keys.onEnterPressed: clicked()

    Keys.onRightPressed: {
        var next = nextItemInFocusChain(true)
        if (next)
            next.forceActiveFocus(Qt.TabFocus)
    }
    Keys.onLeftPressed: {
        var previous = nextItemInFocusChain(false)
        if (previous)
            previous.forceActiveFocus(Qt.TabFocus)
    }

    Accessible.role: Accessible.Button
    Accessible.name: accessibleName.length > 0 ? accessibleName : text
    Accessible.description: description
    Accessible.onPressAction: clicked()

    contentItem: Item {
        implicitWidth: Math.max(actionLabel.implicitWidth,
                                descriptionLabel.visible
                                ? descriptionLabel.implicitWidth : 0)
        implicitHeight: actionLabel.implicitHeight
                        + (descriptionLabel.visible
                           ? contentColumn.spacing
                             + descriptionLabel.implicitHeight : 0)

        Column {
            id: contentColumn
            anchors.fill: parent
            spacing: descriptionLabel.visible ? Tokens.dp(3) : 0

            Label {
                id: actionLabel
                width: parent.width
                text: control.text
                font.pixelSize: control.compact ? Tokens.tChip : Tokens.tMeta
                font.family: Tokens.familyBody
                font.weight: Font.DemiBold
                color: control.primary && (control.activeFocus || control.down)
                       ? Tokens.focusInk : control.destructive
                         ? Tokens.statusOffline : Tokens.textPrimary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            Label {
                id: descriptionLabel
                width: parent.width
                visible: control.description.length > 0 && !control.compact
                text: control.description
                font.pixelSize: Tokens.tMicro
                font.family: Tokens.familyBody
                color: Tokens.textSecondary
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }
    }

    background: Rectangle {
        radius: control.compact ? height / 2 : Tokens.radiusControl
        color: control.primary
               ? (control.activeFocus || control.down
                  ? Tokens.accentFocus : Tokens.surfaceFocus)
               : (control.down || control.highlighted || control.checked
                  ? Tokens.surfaceFocus
                  : control.hovered ? Qt.lighter(Tokens.surface, 1.08)
                                    : Tokens.surface)
        border.width: control.activeFocus ? 0 : Tokens.routeStroke
        border.color: control.primary || control.highlighted || control.checked
                      ? Tokens.link : Tokens.border
        opacity: control.enabled ? 1.0 : 0.42

        Behavior on color {
            ColorAnimation { duration: Tokens.motion(Tokens.durationFast) }
        }

        MoonGlow {
            active: control.activeFocus
            radius: parent.radius
            allowLift: false
        }
    }
}
