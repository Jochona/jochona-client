// Night Route focus trace. One precise outline and one route cue replace the
// stacked glow frames: unmistakable at ten feet, cheap on handheld GPUs, and
// stable in reduced-motion and high-contrast modes.
import QtQuick

import "style"

pragma ComponentBehavior: Bound

Item {
    id: trace

    property bool active: false
    property int radius: Tokens.radiusControl
    property bool allowLift: true
    readonly property bool lift: active && allowLift && Tokens.motion(1) > 0

    anchors.fill: parent
    z: active ? 10 : 0

    Rectangle {
        anchors.fill: parent
        anchors.margins: -Tokens.dp(2)
        radius: trace.radius + Tokens.dp(2)
        color: "transparent"
        border.width: trace.active ? Tokens.focusStroke : 0
        border.color: trace.active ? Tokens.borderFocus : "transparent"
        opacity: trace.active ? 1.0 : 0.0

        Behavior on opacity {
            NumberAnimation {
                duration: Tokens.motion(Tokens.durationFast)
                easing.type: Easing.OutCubic
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: Tokens.dp(16)
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Tokens.dp(4)
        width: trace.active ? Math.max(Tokens.dp(38),
                                       Math.min(parent.width * 0.32, Tokens.dp(132))) : 0
        height: Math.max(Tokens.routeStroke * 2, Tokens.dp(2))
        radius: height / 2
        color: Tokens.moon
        opacity: trace.active ? 1.0 : 0.0

        Behavior on width {
            NumberAnimation {
                duration: Tokens.motion(Tokens.durationRoute)
                easing.type: Easing.OutCubic
            }
        }
        Behavior on opacity {
            NumberAnimation { duration: Tokens.motion(Tokens.durationFast) }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.leftMargin: Tokens.dp(12)
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Tokens.dp(2)
        width: Tokens.dp(6)
        height: width
        radius: width / 2
        color: Tokens.moon
        visible: trace.active
    }
}
