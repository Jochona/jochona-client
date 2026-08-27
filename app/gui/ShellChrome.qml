// Jochona shell chrome (M-redesign). Replaces the painted header bar for the
// modern shell: content runs full-bleed to the window edges while the screen
// title becomes a large typographic label and actions float as a ghost-icon
// cluster top-right (Netflix-10-foot / Steam-Deck pattern; M3 large top app
// bar). Legacy mode (modernHomeScreen=false) keeps the upstream ToolBar and
// never instantiates this overlay's controls.
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15

import AutoUpdateChecker 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import "style"

Item {
    id: chrome
    anchors.fill: parent
    z: 10

    // Wired by main.qml
    property var stack: null
    signal backRequested()
    signal addPcRequested()
    signal settingsRequested()

    readonly property bool chromeVisible: StreamingPreferences.modernHomeScreen
    readonly property int titleZone: 96

    // Ambient brand glow, top-left — carries the identity the accent bar used to
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        width: 520
        height: 320
        visible: chrome.chromeVisible
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop {
                position: 0.0
                color: Qt.hsla(Tokens.accent.hue, Tokens.accent.saturation * 0.9, 0.55, 0.12)
            }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    // Scrim so content scrolling beneath the floating chrome stays readable
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: chrome.titleZone
        visible: chrome.chromeVisible
        gradient: Gradient {
            orientation: Gradient.Vertical
            GradientStop { position: 0.0; color: Qt.alpha(Tokens.surface, 0.96) }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }

    Label {
        id: titleLabel
        visible: chrome.chromeVisible && text.length > 0
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 26
        anchors.leftMargin: (chrome.stack && chrome.stack.depth > 1) ? 96 : 32
        Behavior on anchors.leftMargin {
            NumberAnimation { duration: Tokens.motion(180); easing.type: Easing.OutCubic }
        }
        text: chrome.stack && chrome.stack.currentItem
              ? chrome.stack.currentItem.objectName : ""
        font.family: Tokens.familyDisplay
        font.pointSize: Tokens.sizeHero
        font.weight: Font.DemiBold
        color: Tokens.textPrimary
    }

    component GhostIconButton: Item {
        id: ghost
        implicitWidth: 44
        implicitHeight: 44
        signal clicked()
        property alias iconSource: icon.source
        property alias tooltipText: tip.text
        property bool hovered: mouse.containsMouse
        activeFocusOnTab: true
        scale: activeFocus ? 1.12 : 1.0
        Behavior on scale {
            NumberAnimation { duration: Tokens.motion(120); easing.type: Easing.OutCubic }
        }
        Keys.onReturnPressed: clicked()
        Keys.onSpacePressed: clicked()
        Keys.onEnterPressed: clicked()

        Rectangle {
            anchors.fill: parent
            radius: 22
            color: ghost.activeFocus ? Qt.alpha(Tokens.accent, 0.22)
                                     : (ghost.hovered ? Qt.alpha(Tokens.accent, 0.10) : "transparent")
            border.width: ghost.activeFocus ? 2 : 0
            border.color: Tokens.accentFocus
            Behavior on color { ColorAnimation { duration: Tokens.motion(120) } }
        }
        Image {
            id: icon
            anchors.centerIn: parent
            width: 22
            height: 22
            sourceSize.width: 44
            sourceSize.height: 44
            fillMode: Image.PreserveAspectFit
            // 0.4 ghost weight; full brightness when focused (Netflix-TV ghosting)
            opacity: ghost.activeFocus ? 1.0 : 0.4
            Behavior on opacity { NumberAnimation { duration: Tokens.motion(120) } }
        }
        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: ghost.clicked()
            onEntered: ghost.forceActiveFocus()
        }
        ToolTip {
            id: tip
            visible: ghost.hovered && text.length > 0
            delay: 800
            timeout: 3000
        }
    }

    // Actions float top-right; D-pad Up from content lands here
    Row {
        id: actionCluster
        visible: chrome.chromeVisible
        spacing: 6
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 22
        anchors.rightMargin: 24

        GhostIconButton {
            id: updateButton
            iconSource: "qrc:/res/update.svg"
            visible: false
            property string browserUrl: ""
            function updateAvailable(version, url) {
                tooltipText = qsTr("Update available for Jochona: Version %1").arg(version)
                browserUrl = url
                visible = true
            }
            onClicked: {
                if (SystemProperties.hasBrowser)
                    Qt.openUrlExternally(browserUrl)
            }
            Keys.onDownPressed: chrome.giveFocusToContent()
            Component.onCompleted: {
                AutoUpdateChecker.onUpdateAvailable.connect(updateAvailable)
                // Jochona: upstream Moonlight manifest intentionally not started;
                // see main.qml history (re-enable against Jochona GitHub Releases).
            }
        }

        GhostIconButton {
            id: addPcButton
            iconSource: "qrc:/res/ic_add_to_queue_white_48px.svg"
            visible: chrome.stack && chrome.stack.currentItem instanceof PcView
            tooltipText: qsTr("Add PC manually")
            onClicked: chrome.addPcRequested()
            Keys.onDownPressed: chrome.giveFocusToContent()
        }

        GhostIconButton {
            id: helpButton
            visible: SystemProperties.hasBrowser
            iconSource: "qrc:/res/question_mark.svg"
            tooltipText: qsTr("Help")
            onClicked: Qt.openUrlExternally("https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide")
            Keys.onDownPressed: chrome.giveFocusToContent()
        }

        GhostIconButton {
            id: settingsButton
            iconSource: "qrc:/res/settings.svg"
            tooltipText: qsTr("Settings")
            onClicked: chrome.settingsRequested()
            Keys.onDownPressed: chrome.giveFocusToContent()
        }
    }

    // Floating back affordance — appears one level deeper than the root
    GhostIconButton {
        id: backButton
        visible: chrome.chromeVisible && chrome.stack && chrome.stack.depth > 1
        iconSource: "qrc:/res/arrow_left.svg"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.topMargin: 22
        anchors.leftMargin: 28
        opacity: visible ? 1.0 : 0.0
        scale: visible ? 1.0 : 0.6
        Behavior on opacity { NumberAnimation { duration: Tokens.motion(160) } }
        Behavior on scale {
            NumberAnimation { duration: Tokens.motion(160); easing.type: Easing.OutCubic }
        }
        onClicked: chrome.backRequested()
        Keys.onDownPressed: chrome.giveFocusToContent()
    }

    // Version marker replaces the old in-bar label
    Label {
        visible: chrome.chromeVisible &&
                 chrome.stack && chrome.stack.currentItem instanceof SettingsView
        text: qsTr("Version %1").arg(SystemProperties.versionString)
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 14
        font.pointSize: Tokens.sizeMicro
        color: Tokens.textSecondary
    }

    function giveFocusToContent() {
        if (chrome.stack && chrome.stack.currentItem)
            chrome.stack.currentItem.forceActiveFocus(Qt.TabFocus)
    }

    // When the view under the cluster changes, make sure a now-invisible
    // focused ghost doesn't eat the controller focus ring
    onVisibleChanged: {
        if (!visible && actionCluster.children) {
            for (var i = 0; i < actionCluster.children.length; i++) {
                if (actionCluster.children[i] && actionCluster.children[i].activeFocus)
                    giveFocusToContent()
            }
        }
    }
}
