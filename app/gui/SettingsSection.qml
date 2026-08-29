// Night Route settings group: one semantic section, no rich-text title or
// Material frame. Content keeps its existing controls and preference bindings.
import QtQuick 2.15
import QtQuick.Controls 2.2

import "style"

GroupBox {
    id: section

    padding: Tokens.gutter
    topPadding: labelItem.implicitHeight + Tokens.gutter
    font.family: Tokens.familyBody
    font.pointSize: Tokens.sizeBody

    label: Label {
        id: labelItem
        x: section.leftPadding
        width: section.availableWidth
        text: section.title
        font.family: Tokens.familyDisplay
        font.pixelSize: Tokens.tShelf
        font.weight: Font.Medium
        color: Tokens.textPrimary
        elide: Text.ElideRight
        Accessible.role: Accessible.Heading
    }

    background: Rectangle {
        y: labelItem.implicitHeight + Tokens.gutterTight
        height: section.height - y
        radius: Tokens.radiusPanel
        color: Tokens.surface
        border.width: Tokens.routeStroke
        border.color: Tokens.border
    }
}
