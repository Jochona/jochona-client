// Night Route Resume stage. It is the first control, not a passive banner:
// one literal action and one visible Device → Rig → Game path fill the frame
// even when no artwork exists.
import QtQuick
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import "style"

pragma ComponentBehavior: Bound

Item {
    id: stage

    property string title: ""
    property string subtitle: ""
    property string art: ""
    property string metaLine: ""
    property string hostLabel: ""
    property string destinationLabel: ""
    property string actionLabel: qsTr("Resume")
    property bool playable: false
    property string emptyHint: ""

    signal activate()

    focusPolicy: Qt.StrongFocus
    activeFocusOnTab: true
    clip: false

    Accessible.role: Accessible.Button
    Accessible.name: title.length > 0
                     ? actionLabel + " " + title
                     : emptyHint
    Accessible.description: subtitle
    Accessible.onPressAction: activate()

    Rectangle {
        anchors.fill: parent
        radius: Tokens.radiusPanel
        color: Tokens.surface
        border.width: Tokens.routeStroke
        border.color: Tokens.border
        clip: true

        Image {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * (Tokens.wideViewport ? 0.48 : 0.62)
            source: stage.art
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            visible: status === Image.Ready && stage.art.length > 0
            opacity: visible ? 0.22 : 0.0
            Behavior on opacity {
                NumberAnimation { duration: Tokens.motion(Tokens.durationRoute) }
            }
        }

        // A quiet connection field gives no-art states structure without
        // substituting decorative chrome for content.
        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(parent.width * 0.52, Tokens.dp(760))
            height: Tokens.routeStroke
            color: Tokens.border
            opacity: 0.7
        }
    }

    GridLayout {
        anchors.fill: parent
        anchors.margins: Tokens.gutter
        columns: Tokens.wideViewport && !Tokens.handheld ? 2 : 1
        columnSpacing: Tokens.gutter * 2
        rowSpacing: Tokens.gutter

        ColumnLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: stage.width
                                   * (Tokens.wideViewport ? 0.42 : 1.0)
            Layout.alignment: Qt.AlignVCenter
            spacing: Tokens.gutterTight

            Label {
                Layout.fillWidth: true
                text: stage.title.length > 0 ? stage.title : qsTr("Jochona")
                font.family: Tokens.familyDisplay
                font.pixelSize: stage.title.length > 0 ? Tokens.tHero : Tokens.tTitle
                font.weight: Font.Medium
                color: Tokens.textPrimary
                elide: Text.ElideRight
                Accessible.role: Accessible.Heading
            }

            Label {
                Layout.fillWidth: true
                visible: stage.subtitle.length > 0
                text: stage.subtitle
                font.family: Tokens.familyBody
                font.pixelSize: Tokens.tMeta
                font.weight: Font.Medium
                color: Tokens.textSecondary
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                visible: stage.metaLine.length > 0
                text: stage.metaLine
                font.family: Tokens.familyBody
                font.pixelSize: Tokens.tChip
                color: Tokens.link
                elide: Text.ElideRight
            }

            Row {
                visible: stage.title.length > 0 || stage.emptyHint.length > 0
                spacing: Tokens.dp(10)

                Rectangle {
                    width: Tokens.dp(9)
                    height: width
                    radius: width / 2
                    color: stage.activeFocus ? Tokens.moon : Tokens.link
                    anchors.verticalCenter: parent.verticalCenter
                }

                Label {
                    text: stage.title.length > 0 ? stage.actionLabel : qsTr("Add a rig")
                    font.family: Tokens.familyBody
                    font.pixelSize: Tokens.tMeta
                    font.weight: Font.DemiBold
                    color: stage.activeFocus ? Tokens.textPrimary : Tokens.textSecondary
                }
            }

            Label {
                Layout.fillWidth: true
                visible: stage.emptyHint.length > 0
                text: stage.emptyHint
                font.family: Tokens.familyBody
                font.pixelSize: Tokens.tMeta
                color: Tokens.textSecondary
                wrapMode: Text.WordWrap
            }
        }

        Item {
            id: routeProof
            Layout.fillWidth: true
            Layout.preferredWidth: stage.width
                                   * (Tokens.wideViewport ? 0.50 : 1.0)
            Layout.minimumWidth: Tokens.wideViewport
                                 ? Tokens.dp(360) : 0
            Layout.minimumHeight: Tokens.dp(Tokens.handheld ? 104 : 132)
            Layout.alignment: Qt.AlignVCenter

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: parent.width / 6
                anchors.rightMargin: parent.width / 6
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -Tokens.dp(18)
                height: Tokens.routeStroke
                color: stage.playable ? Tokens.link : Tokens.border
                Behavior on color {
                    ColorAnimation { duration: Tokens.motion(Tokens.durationRoute) }
                }
            }

            Row {
                anchors.fill: parent

                Repeater {
                    model: [
                        qsTr("This device"),
                        stage.hostLabel.length > 0 ? stage.hostLabel : qsTr("Choose a rig"),
                        stage.destinationLabel.length > 0
                            ? stage.destinationLabel
                            : stage.title.length > 0 ? stage.title : qsTr("Play")
                    ]

                    delegate: Item {
                        required property var modelData
                        required property int index
                        width: routeProof.width / 3
                        height: routeProof.height

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.verticalCenterOffset: -Tokens.dp(18)
                            width: index === 2 && stage.activeFocus
                                   ? Tokens.dp(15) : Tokens.dp(9)
                            height: width
                            radius: width / 2
                            color: index === 2 && stage.activeFocus
                                   ? Tokens.moon
                                   : stage.playable ? Tokens.link : Tokens.border
                            Behavior on width {
                                NumberAnimation {
                                    duration: Tokens.motion(Tokens.durationFast)
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }

                        Label {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.verticalCenter
                            anchors.topMargin: Tokens.dp(2)
                            text: modelData
                            font.family: Tokens.familyBody
                            font.pixelSize: Tokens.tMicro
                            font.weight: Font.Medium
                            color: Tokens.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }
    }

    MoonGlow {
        active: stage.activeFocus
        radius: Tokens.radiusPanel
        allowLift: false
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
            stage.forceActiveFocus()
            stage.activate()
        }
    }

    Keys.onReturnPressed: activate()
    Keys.onEnterPressed: activate()
    Keys.onSpacePressed: activate()
}
