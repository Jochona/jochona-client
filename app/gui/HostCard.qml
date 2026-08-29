// A rig as a Night Route stop: live status, literal name, and one activation.
// Wake, diagnostics, rename, and remove remain progressive secondary actions.
import QtQuick
import QtQuick.Controls 2.2

import "style"

pragma ComponentBehavior: Bound

Item {
    id: stop

    property string hostName: ""
    property string availability: ""
    property string connectionLabel: ""
    property string actionText: qsTr("Open")
    property color statusColor: Tokens.statusUnknown
    property bool unknown: false
    property bool waking: false
    signal cardActivate()
    signal pressHold()

    readonly property bool heroPlayable: true
    readonly property string heroTitle: hostName
    readonly property string heroSubtitle: availability
    readonly property string heroArt: ""
    readonly property string heroMeta: connectionLabel
    readonly property string heroHost: hostName
    readonly property string heroDestination: qsTr("Apps")
    readonly property string heroAction: actionText
    function heroActivate() { cardActivate() }

    required property int index
    width: Tokens.dp(Tokens.handheld ? 248 : 300)
    height: Tokens.dp(150)
    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true

    Accessible.role: Accessible.Button
    Accessible.name: hostName
    Accessible.description: availability
    Accessible.onPressAction: cardActivate()

    onActiveFocusChanged: {
        if (activeFocus && ListView.view && index >= 0)
            ListView.view.currentIndex = index
    }

    function step(delta, event) {
        var view = ListView.view
        if (view !== null && index + delta >= 0 && index + delta < view.count) {
            view.currentIndex = index + delta
            var item = view.itemAtIndex(index + delta)
            if (item)
                item.forceActiveFocus()
            event.accepted = true
        }
    }

    Keys.onLeftPressed: function(event) { step(-1, event) }
    Keys.onRightPressed: function(event) { step(1, event) }
    Keys.onReturnPressed: cardActivate()
    Keys.onEnterPressed: cardActivate()
    Keys.onSpacePressed: cardActivate()

    scale: glow.lift ? 1.025 : 1.0
    Behavior on scale {
        NumberAnimation {
            duration: Tokens.motion(Tokens.durationFast)
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: Tokens.radiusCard
        color: stop.activeFocus || pointer.containsMouse
               ? Tokens.surfaceFocus : Tokens.surface
        border.width: stop.activeFocus ? 0 : Tokens.routeStroke
        border.color: Tokens.border
        Behavior on color {
            ColorAnimation { duration: Tokens.motion(Tokens.durationFast) }
        }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: Tokens.dp(4)
        width: stop.activeFocus ? Tokens.dp(14) : Tokens.dp(10)
        height: width
        radius: width / 2
        color: stop.unknown || stop.waking ? Tokens.statusUnknown : stop.statusColor
        border.width: Tokens.routeStroke
        border.color: Tokens.surface
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: Tokens.gutter
        spacing: Tokens.dp(7)

        Row {
            spacing: Tokens.dp(9)

            BusyIndicator {
                width: Tokens.dp(22)
                height: width
                visible: stop.unknown || stop.waking
                running: visible
            }

            Label {
                width: parent.parent.width - (stop.unknown || stop.waking
                                              ? Tokens.dp(31) : 0)
                text: stop.hostName
                font.family: Tokens.familyDisplay
                font.pixelSize: Tokens.tCard
                font.weight: Font.Medium
                color: Tokens.textPrimary
                elide: Text.ElideRight
            }
        }

        Label {
            width: parent.width
            text: stop.availability
            font.family: Tokens.familyBody
            font.pixelSize: Tokens.tChip
            color: Tokens.textSecondary
            elide: Text.ElideRight
        }

        Label {
            width: parent.width
            visible: stop.connectionLabel.length > 0
            text: stop.connectionLabel
            font.family: Tokens.familyBody
            font.pixelSize: Tokens.tMicro
            color: Tokens.link
            elide: Text.ElideRight
        }
    }

    MoonGlow {
        id: glow
        active: stop.activeFocus
        radius: Tokens.radiusCard
    }

    MouseArea {
        id: pointer
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        onEntered: Tokens.inputMode = "pointer"
        onClicked: {
            Tokens.inputMode = "pointer"
            stop.forceActiveFocus()
            if (mouse.button === Qt.RightButton)
                stop.pressHold()
            else
                stop.cardActivate()
        }
        onPressAndHold: stop.pressHold()
    }
}
