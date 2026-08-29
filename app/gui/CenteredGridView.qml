import QtQuick 2.15
import QtQuick.Controls 2.2

import "style"

GridView {
    property int minMargin: Tokens.gutterTight
    property real availableWidth: Math.max(0, parent.width - 2 * minMargin)
    property int itemsPerRow: Math.max(1, Math.floor(
                                          availableWidth
                                          / Math.max(1, cellWidth)))
    property real horizontalMargin:
        itemsPerRow < count && availableWidth >= cellWidth
        ? (availableWidth - itemsPerRow * cellWidth) / 2 : minMargin

    function updateMargins() {
        leftMargin = horizontalMargin
        rightMargin = horizontalMargin
    }

    onHorizontalMarginChanged: updateMargins()
    Component.onCompleted: updateMargins()

    boundsBehavior: Flickable.StopAtBounds
}
