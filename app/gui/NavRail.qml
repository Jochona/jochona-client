// Night Route destination bar. It is visible only on the root Home surface:
// named stops replace icon-only chrome, and the same line is the visual and
// controller navigation model.
import QtQuick
import QtQuick.Controls 2.2

import "style"

pragma ComponentBehavior: Bound

Item {
    id: route

    signal resumeRequested()
    signal rigsRequested()
    signal libraryRequested()
    signal controllersRequested()
    signal settingsRequested()
    signal enterContentRequested()
    signal backRequested()

    property string currentKey: "resume"
    readonly property int destinationCount: destinationRepeater.count

    height: Tokens.routeBarHeight
    focusPolicy: Qt.TabFocus

    function focusRoute(key) {
        var targetIndex = 0
        for (var i = 0; i < destinationRepeater.count; i++) {
            var candidate = destinationRepeater.itemAt(i)
            if (candidate && candidate.modelData.key === key) {
                targetIndex = i
                break
            }
        }
        var target = destinationRepeater.itemAt(targetIndex)
        if (target)
            target.forceActiveFocus()
    }

    function focusRail() {
        focusRoute(currentKey)
    }

    Rectangle {
        anchors.fill: parent
        radius: Tokens.radiusPanel
        color: Tokens.surface
        border.width: Tokens.routeStroke
        border.color: Tokens.border
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: route.width / (route.destinationCount * 2)
        anchors.rightMargin: anchors.leftMargin
        y: Tokens.dp(20)
        height: Tokens.routeStroke
        color: Tokens.border
    }

    Row {
        anchors.fill: parent

        Repeater {
            id: destinationRepeater
            model: [
                { key: "resume",      label: qsTr("Resume") },
                { key: "rigs",        label: qsTr("Rigs") },
                { key: "library",     label: qsTr("Library") },
                { key: "controllers", label: qsTr("Controllers") },
                { key: "settings",    label: qsTr("Settings") }
            ]

            delegate: Item {
                id: stop

                required property var modelData
                required property int index

                width: route.width / route.destinationCount
                height: route.height
                focusPolicy: Qt.StrongFocus
                activeFocusOnTab: true
                readonly property bool selected: route.currentKey === modelData.key

                Accessible.role: Accessible.Button
                Accessible.name: modelData.label
                Accessible.description: selected
                                        ? qsTr("Current destination")
                                        : qsTr("Open %1").arg(modelData.label)
                Accessible.onPressAction: activate()

                Rectangle {
                    id: cue
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: Tokens.dp(14)
                    width: stop.activeFocus ? Tokens.dp(14)
                                            : stop.selected ? Tokens.dp(10)
                                                            : Tokens.dp(7)
                    height: width
                    radius: width / 2
                    color: stop.activeFocus ? Tokens.moon
                                            : stop.selected ? Tokens.link
                                                            : Tokens.border
                    border.width: stop.activeFocus ? Tokens.routeStroke : 0
                    border.color: Tokens.textPrimary

                    Behavior on width {
                        NumberAnimation {
                            duration: Tokens.motion(Tokens.durationFast)
                            easing.type: Easing.OutCubic
                        }
                    }
                    Behavior on color {
                        ColorAnimation { duration: Tokens.motion(Tokens.durationFast) }
                    }
                }

                Label {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: cue.bottom
                    anchors.topMargin: Tokens.dp(8)
                    text: stop.modelData.label
                    font.family: Tokens.familyBody
                    font.pixelSize: Tokens.tMicro
                    font.weight: stop.activeFocus || stop.selected
                                 ? Font.DemiBold : Font.Medium
                    color: stop.activeFocus ? Tokens.textPrimary
                                            : stop.selected ? Tokens.link
                                                            : Tokens.textSecondary
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: Tokens.dp(16)
                    anchors.rightMargin: Tokens.dp(16)
                    anchors.bottom: parent.bottom
                    height: Math.max(Tokens.routeStroke * 2, Tokens.dp(2))
                    radius: height / 2
                    color: Tokens.moon
                    opacity: stop.activeFocus ? 1.0 : 0.0
                    Behavior on opacity {
                        NumberAnimation { duration: Tokens.motion(Tokens.durationFast) }
                    }
                }

                HoverHandler {
                    onHoveredChanged: {
                        if (hovered)
                            Tokens.inputMode = "pointer"
                    }
                }

                TapHandler {
                    onTapped: {
                        Tokens.inputMode = "pointer"
                        stop.forceActiveFocus()
                        stop.activate()
                    }
                }

                function activate() {
                    route.currentKey = stop.modelData.key
                    if (stop.modelData.key === "resume")
                        route.resumeRequested()
                    else if (stop.modelData.key === "rigs")
                        route.rigsRequested()
                    else if (stop.modelData.key === "library")
                        route.libraryRequested()
                    else if (stop.modelData.key === "controllers")
                        route.controllersRequested()
                    else
                        route.settingsRequested()
                }

                Keys.onLeftPressed: {
                    var previous = destinationRepeater.itemAt(index - 1)
                    if (previous)
                        previous.forceActiveFocus()
                }
                Keys.onRightPressed: {
                    var next = destinationRepeater.itemAt(index + 1)
                    if (next)
                        next.forceActiveFocus()
                }
                Keys.onUpPressed: route.enterContentRequested()
                Keys.onReturnPressed: activate()
                Keys.onEnterPressed: activate()
                Keys.onSpacePressed: activate()
                Keys.onEscapePressed: route.backRequested()
                Keys.onBackPressed: route.backRequested()
            }
        }
    }
}
