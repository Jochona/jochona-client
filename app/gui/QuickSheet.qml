// Night Route context panel. Short actions preserve the screen beneath them;
// long content scrolls inside a bounded panel, focus is explicit and restored,
// and handheld uses a bottom panel while wider scenes use a right waypoint.
import QtQuick
import QtQuick.Controls 2.15

import "style"

pragma ComponentBehavior: Bound

Popup {
    id: panel

    property string title: ""
    property Item initialFocusItem: null
    property Item returnFocusItem: null
    property real preferredWidth: Tokens.panelWidth
    default property alias panelContent: contentColumn.data

    signal accepted()
    signal rejected()

    property bool _accepted: false
    property Item _capturedFocus: null

    readonly property bool sidePanel: !Tokens.handheld
                                      && parent !== null
                                      && parent.width >= Tokens.dp(1180)
    readonly property real titleBlockHeight: titleRow.visible
                                                   ? titleRow.implicitHeight
                                                     + Tokens.gutterTight : 0
    readonly property real maxBodyHeight: parent !== null
                                          ? Math.max(Tokens.dp(140),
                                                     parent.height - Tokens.safeInset * 2
                                                     - topPadding - bottomPadding
                                                     - titleBlockHeight)
                                          : Tokens.dp(480)

    function accept() {
        _accepted = true
        close()
    }

    function reject() {
        _accepted = false
        close()
    }

    function focusInitial() {
        if (initialFocusItem !== null && initialFocusItem.visible
                && initialFocusItem.enabled) {
            initialFocusItem.forceActiveFocus()
            return
        }

        var candidates = []
        function collect(item) {
            var kids = item.children
            for (var i = 0; i < kids.length; i++) {
                var child = kids[i]
                if (child.visible === undefined || !child.visible || !child.enabled)
                    continue
                var focusable = (child.focusPolicy !== undefined
                                 && child.focusPolicy !== Qt.NoFocus)
                                || child.activeFocusOnTab === true
                if (focusable)
                    candidates.push(child)
                collect(child)
            }
        }
        collect(contentColumn)

        var target = null
        for (var j = 0; j < candidates.length; j++) {
            if (candidates[j].destructive !== true) {
                target = candidates[j]
                break
            }
        }
        if (target === null && candidates.length > 0)
            target = candidates[0]
        if (target !== null)
            target.forceActiveFocus()
    }

    modal: true
    dim: true
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    topPadding: Tokens.gutter
    bottomPadding: Tokens.gutter
    leftPadding: Tokens.gutter
    rightPadding: Tokens.gutter

    width: parent === null ? preferredWidth
                           : sidePanel
                             ? Math.min(preferredWidth,
                                        parent.width - Tokens.safeInset * 2)
                             : parent.width - Tokens.safeInset * 2
    height: Math.min(titleBlockHeight + bodyScroll.height
                     + topPadding + bottomPadding,
                     parent !== null
                     ? parent.height - Tokens.safeInset * 2
                     : Tokens.dp(640))
    x: parent === null ? 0
                       : sidePanel
                         ? parent.width - width - Tokens.safeInset
                         : Tokens.safeInset
    y: parent === null ? 0
                       : sidePanel
                         ? Tokens.safeInset
                         : parent.height - height - Tokens.safeInset

    transformOrigin: sidePanel ? Item.Right : Item.Bottom

    Overlay.modal: Rectangle {
        color: Tokens.scrim
    }

    background: Rectangle {
        color: Tokens.surface
        radius: Tokens.radiusPanel
        border.width: Tokens.routeStroke
        border.color: Tokens.border
    }

    enter: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: Tokens.motion(Tokens.durationBase)
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "scale"
                from: 0.985
                to: 1.0
                duration: Tokens.motion(Tokens.durationBase)
                easing.type: Easing.OutCubic
            }
        }
    }

    exit: Transition {
        ParallelAnimation {
            NumberAnimation {
                property: "opacity"
                to: 0.0
                duration: Tokens.motion(Tokens.durationFast)
            }
            NumberAnimation {
                property: "scale"
                to: 0.985
                duration: Tokens.motion(Tokens.durationFast)
            }
        }
    }

    onAboutToShow: {
        _accepted = false
        var win = parent !== null ? parent.Window.window : null
        _capturedFocus = returnFocusItem !== null
                         ? returnFocusItem
                         : win !== null ? win.activeFocusItem : null
    }

    onOpened: Qt.callLater(focusInitial)

    onClosed: {
        if (_accepted)
            accepted()
        else
            rejected()

        var restore = _capturedFocus
        Qt.callLater(function() {
            if (restore !== null && restore.parent !== null)
                restore.forceActiveFocus()
        })
    }

    contentItem: Column {
        spacing: Tokens.gutterTight

        Row {
            id: titleRow
            width: parent.width
            spacing: Tokens.dp(12)
            visible: panel.title.length > 0

            Rectangle {
                width: Tokens.dp(7)
                height: width
                radius: width / 2
                color: Tokens.moon
                anchors.verticalCenter: parent.verticalCenter
            }

            Label {
                width: parent.width - Tokens.dp(19)
                text: panel.title
                font.family: Tokens.familyDisplay
                font.pixelSize: Tokens.tTitle
                font.weight: Font.Medium
                color: Tokens.textPrimary
                elide: Text.ElideRight
                Accessible.role: Accessible.Heading
            }
        }

        ScrollView {
            id: bodyScroll
            width: parent.width
            height: Math.min(contentColumn.implicitHeight, panel.maxBodyHeight)
            clip: true
            contentWidth: availableWidth
            contentHeight: contentColumn.implicitHeight
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            Column {
                id: contentColumn
                width: bodyScroll.availableWidth
                spacing: Tokens.gutterTight
            }
        }
    }
}
