// Legacy Dialog compatibility rendered inside the Night Route system. New
// controller-first flows use QuickSheet/TextEntryView; settings forms retain
// this API until their value editors are migrated.
import QtQuick 2.15
import QtQuick.Controls 2.5

import "style"

Dialog {
    id: dialog

    property Item returnFocusItem: null
    property Item capturedFocusItem: null

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    padding: Tokens.gutter
    palette.window: Tokens.surface
    palette.windowText: Tokens.textPrimary
    palette.base: Tokens.surface
    palette.text: Tokens.textPrimary
    palette.button: Tokens.surface
    palette.buttonText: Tokens.textPrimary
    palette.highlight: Tokens.accentFocus
    palette.highlightedText: Tokens.focusInk

    Overlay.modal: Rectangle { color: Tokens.scrim }

    background: Rectangle {
        radius: Tokens.radiusPanel
        color: Tokens.surface
        border.width: Tokens.routeStroke
        border.color: Tokens.border
    }

    onAboutToShow: {
        var win = dialog.parent !== null ? dialog.parent.Window.window : null
        capturedFocusItem = returnFocusItem !== null
                            ? returnFocusItem
                            : win !== null ? win.activeFocusItem : null
    }

    onClosed: {
        var restore = capturedFocusItem
        Qt.callLater(function() {
            if (restore !== null && restore.parent !== null)
                restore.forceActiveFocus()
            else if (stackView)
                stackView.forceActiveFocus()
        })
    }
}
