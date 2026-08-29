// A game/application as a route destination. Wide tickets work without art,
// preserve literal metadata, and avoid a generic poster wall.
import QtQuick
import QtQuick.Controls 2.2

import "style"

pragma ComponentBehavior: Bound

Item {
    id: destination

    property string titleText: ""
    property string artUrl: ""
    property string hostText: ""
    property bool running: false
    property bool hidden: false
    property bool selected: false
    property Item navigationOwner: null
    readonly property bool focusVisual: activeFocus || selected

    signal cardActivate()
    signal pressHold()

    required property int index

    readonly property bool heroPlayable: true
    readonly property string heroTitle: titleText
    readonly property string heroSubtitle: running ? qsTr("Running") : qsTr("Ready to play")
    readonly property string heroArt: artUrl
    readonly property string heroMeta: hostText
    readonly property string heroHost: hostText
    readonly property string heroDestination: titleText
    readonly property string heroAction: running ? qsTr("Resume") : qsTr("Play")
    function heroActivate() { cardActivate() }

    width: Tokens.dp(Tokens.handheld ? 250 : 300)
    height: Tokens.dp(158)
    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true
    opacity: hidden ? 0.45 : 1.0

    Accessible.role: Accessible.Button
    Accessible.name: titleText
    Accessible.description: (running ? qsTr("Running") : qsTr("Ready to play"))
                            + (hostText.length > 0 ? ", " + hostText : "")
    Accessible.onPressAction: cardActivate()

    onActiveFocusChanged: {
        var view = ListView.view
        if (activeFocus && view !== null && index >= 0)
            view.currentIndex = index
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
        id: field
        anchors.fill: parent
        radius: Tokens.radiusCard
        readonly property int hash: {
            var h = 0
            for (var i = 0; i < destination.titleText.length; i++)
                h = (h * 31 + destination.titleText.charCodeAt(i)) & 0xffffff
            return h
        }
        color: destination.focusVisual || pointer.containsMouse
               ? Tokens.surfaceFocus : Tokens.surface
        border.width: destination.focusVisual ? 0 : Tokens.routeStroke
        border.color: Tokens.border
        clip: true

        Rectangle {
            id: artField
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * 0.38
            color: Qt.hsla(0.58 + (field.hash % 40) / 400,
                           0.18,
                           Tokens.textPrimary.b > 0.5 ? 0.18 : 0.82,
                           1.0)

            Image {
                anchors.fill: parent
                source: destination.artUrl
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: status === Image.Ready && destination.artUrl.length > 0
            }

            Label {
                anchors.centerIn: parent
                visible: destination.artUrl.length === 0
                text: {
                    var words = destination.titleText.split(/\s+/).filter(
                                function(word) { return word.length > 0 })
                    if (words.length === 0)
                        return "?"
                    if (words.length === 1)
                        return words[0].substring(0, 2).toUpperCase()
                    return String(words[0].charAt(0)
                                  + words[1].charAt(0)).toUpperCase()
                }
                font.family: Tokens.familyDisplay
                font.pixelSize: Tokens.textPx(38)
                font.weight: Font.Medium
                color: Tokens.moonDim
            }
        }

        Column {
            anchors.left: artField.right
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Tokens.gutterTight
            anchors.rightMargin: Tokens.gutterTight
            spacing: Tokens.dp(7)

            Label {
                width: parent.width
                text: destination.titleText
                font.family: Tokens.familyDisplay
                font.pixelSize: Tokens.tCard
                font.weight: Font.Medium
                color: Tokens.textPrimary
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Label {
                width: parent.width
                visible: destination.hostText.length > 0
                text: destination.hostText
                font.family: Tokens.familyBody
                font.pixelSize: Tokens.tMicro
                color: Tokens.textSecondary
                elide: Text.ElideRight
            }

            Row {
                spacing: Tokens.dp(8)

                Rectangle {
                    width: Tokens.dp(7)
                    height: width
                    radius: width / 2
                    color: destination.running ? Tokens.statusOnline : Tokens.link
                    anchors.verticalCenter: parent.verticalCenter
                }

                Label {
                    text: destination.running ? qsTr("Running") : qsTr("Ready")
                    font.family: Tokens.familyBody
                    font.pixelSize: Tokens.tMicro
                    font.weight: Font.Medium
                    color: destination.running ? Tokens.statusOnline : Tokens.link
                }
            }
        }
    }

    MoonGlow {
        id: glow
        active: destination.focusVisual
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
            if (destination.navigationOwner !== null) {
                destination.navigationOwner.currentIndex = destination.index
                destination.navigationOwner.forceActiveFocus()
            } else {
                destination.forceActiveFocus()
            }
            if (mouse.button === Qt.RightButton)
                destination.pressHold()
            else
                destination.cardActivate()
        }
        onPressAndHold: destination.pressHold()
    }
}
