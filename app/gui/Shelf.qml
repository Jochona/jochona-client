// Horizontal route segment for one kind of destination. A deterministic
// model-ready focus request replaces polling; the line is both visual rhythm
// and the left/right controller topology.
import QtQuick
import QtQuick.Controls 2.2

import "style"

pragma ComponentBehavior: Bound

Item {
    id: routeStrip

    property string label: ""
    property string emptyText: ""
    property alias model: list.model
    property alias delegate: list.delegate
    property alias spacing: list.spacing
    property alias currentIndex: list.currentIndex
    property alias count: list.count
    property int itemHeight: Tokens.dp(164)
    readonly property var focusedItem: list.currentItem
    property bool focusPending: false
    property int pendingIndex: -1

    function takeFocus(index) {
        pendingIndex = index !== undefined && index >= 0 ? index
                                                           : list.currentIndex >= 0
                                                             ? list.currentIndex : 0
        if (count === 0) {
            focusPending = true
            return false
        }
        pendingIndex = Math.min(pendingIndex, count - 1)
        list.currentIndex = pendingIndex
        list.positionViewAtIndex(pendingIndex, ListView.Contain)
        if (list.currentItem !== null) {
            focusPending = false
            list.currentItem.forceActiveFocus()
            return true
        }
        focusPending = true
        return false
    }

    height: headerRow.height + Tokens.gutterTight + list.height

    Row {
        id: headerRow
        anchors.left: parent.left
        anchors.right: parent.right
        height: visible ? Math.max(labelText.implicitHeight, Tokens.dp(18)) : 0
        visible: routeStrip.label.length > 0
        spacing: Tokens.dp(12)

        Rectangle {
            width: Tokens.dp(7)
            height: width
            radius: width / 2
            color: Tokens.link
            anchors.verticalCenter: parent.verticalCenter
        }

        Label {
            id: labelText
            text: routeStrip.label
            font.family: Tokens.familyDisplay
            font.pixelSize: Tokens.tShelf
            font.weight: Font.Medium
            color: Tokens.textPrimary
        }
    }

    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: headerRow.bottom
        anchors.topMargin: Tokens.gutterTight
        height: routeStrip.itemHeight

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Tokens.dp(6)
            anchors.rightMargin: Tokens.dp(6)
            anchors.verticalCenter: parent.verticalCenter
            height: Tokens.routeStroke
            color: Tokens.border
            visible: list.count > 0
        }

        ListView {
            id: list
            anchors.fill: parent
            orientation: ListView.Horizontal
            activeFocusOnTab: false
            highlightFollowsCurrentItem: false
            spacing: Tokens.gutter
            cacheBuffer: width
            leftMargin: Tokens.dp(4)
            rightMargin: Tokens.dp(4)
            topMargin: Tokens.dp(4)
            bottomMargin: Tokens.dp(4)
            clip: true

            Keys.onMenuPressed: {
                if (currentItem !== null)
                    currentItem.pressHold()
            }

            onCountChanged: {
                if (count === 0)
                    currentIndex = -1
                else if (currentIndex < 0 || currentIndex >= count)
                    currentIndex = 0
                if (focusPending && count > 0)
                    Qt.callLater(function() { routeStrip.takeFocus(pendingIndex) })
            }

            onCurrentItemChanged: {
                if (focusPending && currentItem !== null) {
                    focusPending = false
                    currentItem.forceActiveFocus()
                }
            }
        }

        Label {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            visible: list.count === 0 && routeStrip.emptyText.length > 0
            text: routeStrip.emptyText
            font.family: Tokens.familyBody
            font.pixelSize: Tokens.tMeta
            color: Tokens.textSecondary
            wrapMode: Text.WordWrap
        }
    }
}
