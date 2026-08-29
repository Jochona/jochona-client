import QtQuick 2.15
import QtQuick.Controls 2.2

import "style"

// Jochona: reusable controller-first on-screen keyboard (M2 pairing era).
//
// A pure input method: it renders the key grid and edits `text`; the host
// surface renders the composed value however it likes (PIN cells, an address
// field). The whole keyboard is ONE focus stop — D-pad / arrow keys move an
// internal cursor, Return/Enter (gamepad A) presses the highlighted key, and
// the cursor ring is always drawn so the current key is never ambiguous.
//
// Physical keyboards bypass the grid: printable keys append, Backspace
// deletes, and Return commits — but only while the last input was typed;
// arrowing back onto the grid re-arms Return as "press highlighted key".
// Mouse users simply click keys. Escape is deliberately NOT consumed so the
// surface's back behavior keeps working.
//
// `digitsOnly` collapses the layout to a phone pad for PIN entry.
FocusScope {
    id: keyboard

    property string text: ""
    property bool digitsOnly: false
    property int maxLength: 0                 // 0 = unlimited
    property string doneLabel: qsTr("Done")

    // Fired by the Done key, or by Return right after physical typing.
    signal accepted()
    // Fired when the cursor tries to leave the grid vertically; the surface
    // may move focus to controls above/below (deterministic D-pad topology).
    signal navigateUp()
    signal navigateDown()
    // Internal: lets the pressed key's delegate flash for gamepad presses too.
    signal keyActivated(int row, int col)

    // --- Internal state ---
    property bool symbolsPage: false
    property bool shifted: false
    property int curRow: 0
    property int curCol: 0
    // True while the most recent input was typed on a physical keyboard.
    property bool directInput: false

    readonly property int keySpacing: Tokens.dp(8)
    readonly property int keyHeight: Tokens.dp(digitsOnly ? 56 : 50)
    readonly property int unitsPerRow: digitsOnly ? 3 : 10
    readonly property real keyUnit: {
        var available = (width - (unitsPerRow - 1) * keySpacing) / unitsPerRow
        return Math.min(available, Tokens.dp(digitsOnly ? 96 : 76))
    }
    readonly property real gridWidth: keyUnit * unitsPerRow
                                     + keySpacing * (unitsPerRow - 1)

    implicitHeight: keyColumn.implicitHeight
    implicitWidth: digitsOnly
                   ? (3 * Tokens.dp(96) + 2 * keySpacing)
                   : (10 * Tokens.dp(76) + 9 * keySpacing)

    // Key descriptors: { t: "char"|"shift"|"page"|"space"|"del"|"done", ch, span }
    readonly property var rows: {
        function charRow(s) {
            var r = []
            for (var i = 0; i < s.length; i++)
                r.push({ t: "char", ch: s[i], span: 1 })
            return r
        }
        if (digitsOnly) {
            return [
                charRow("123"),
                charRow("456"),
                charRow("789"),
                [ { t: "del", span: 1 }, { t: "char", ch: "0", span: 1 }, { t: "done", span: 1 } ]
            ]
        }
        var pages = symbolsPage
                ? [ "1234567890", ".,:;!?'\"()", "-_/\\@#$%&*", "+=<>[]{}~^" ]
                : [ "1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm.:_" ]
        return [
            charRow(pages[0]),
            charRow(pages[1]),
            charRow(pages[2]),
            charRow(pages[3]),
            [ { t: "shift", span: 1.5 }, { t: "page", span: 1.5 }, { t: "space", span: 3 },
              { t: "del",   span: 1.5 }, { t: "done", span: 2.5 } ]
        ]
    }

    onRowsChanged: {
        curRow = Math.min(curRow, rows.length - 1)
        curCol = Math.min(curCol, rows[curRow].length - 1)
    }

    function keyLabel(key) {
        switch (key.t) {
        case "char":  return shifted && !symbolsPage ? key.ch.toUpperCase() : key.ch
        case "shift": return qsTr("Shift")
        case "page":  return symbolsPage ? qsTr("abc") : qsTr("?123")
        case "space": return qsTr("Space")
        case "del":   return qsTr("Delete")
        case "done":  return doneLabel
        }
        return ""
    }

    function insert(ch) {
        if (maxLength > 0 && text.length >= maxLength)
            return
        text += ch
    }

    function backspace() {
        if (text.length > 0)
            text = text.slice(0, -1)
    }

    function clear() {
        text = ""
    }

    function pressKey(key) {
        switch (key.t) {
        case "char":
            insert(shifted && !symbolsPage ? key.ch.toUpperCase() : key.ch)
            shifted = false
            break
        case "shift":
            shifted = !shifted
            break
        case "page":
            symbolsPage = !symbolsPage
            shifted = false
            break
        case "space":
            insert(" ")
            break
        case "del":
            backspace()
            break
        case "done":
            accepted()
            break
        }
    }

    function pressCurrent() {
        keyActivated(curRow, curCol)
        pressKey(rows[curRow][curCol])
    }

    // Center of a key in row units, for nearest-column mapping across rows
    // of different key counts and spans.
    function centerUnits(row, col) {
        var x = 0
        for (var i = 0; i < col; i++)
            x += rows[row][i].span
        return x + rows[row][col].span / 2
    }

    function nearestCol(row, units) {
        var best = 0
        var bestDist = -1
        for (var i = 0; i < rows[row].length; i++) {
            var d = Math.abs(centerUnits(row, i) - units)
            if (bestDist < 0 || d < bestDist) {
                bestDist = d
                best = i
            }
        }
        return best
    }

    function isAllowedChar(ch) {
        if (digitsOnly)
            return ch >= "0" && ch <= "9"
        return ch.charCodeAt(0) >= 32
    }

    Keys.onPressed: function(event) {
        switch (event.key) {
        case Qt.Key_Left:
            directInput = false
            if (curCol > 0)
                curCol--
            event.accepted = true
            return
        case Qt.Key_Right:
            directInput = false
            if (curCol < rows[curRow].length - 1)
                curCol++
            event.accepted = true
            return
        case Qt.Key_Up:
            directInput = false
            if (curRow > 0) {
                var cu = centerUnits(curRow, curCol)
                curRow--
                curCol = nearestCol(curRow, cu)
            }
            else {
                navigateUp()
            }
            event.accepted = true
            return
        case Qt.Key_Down:
            directInput = false
            if (curRow < rows.length - 1) {
                var cd = centerUnits(curRow, curCol)
                curRow++
                curCol = nearestCol(curRow, cd)
            }
            else {
                navigateDown()
            }
            event.accepted = true
            return
        case Qt.Key_Return:
        case Qt.Key_Enter:
            if (directInput)
                accepted()
            else
                pressCurrent()
            event.accepted = true
            return
        case Qt.Key_Backspace:
            backspace()
            directInput = true
            event.accepted = true
            return
        }

        if (event.text.length === 1 && isAllowedChar(event.text)) {
            insert(event.text)
            directInput = true
            event.accepted = true
        }
    }

    Column {
        id: keyColumn
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: keyboard.keySpacing

        Repeater {
            model: keyboard.rows.length

            Row {
                id: keyRow
                readonly property int rowIndex: index
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: keyboard.keySpacing

                Repeater {
                    model: keyboard.rows[keyRow.rowIndex]

                    Rectangle {
                        id: keyRect

                        readonly property var key: modelData
                        readonly property bool isCurrent: keyboard.curRow === keyRow.rowIndex &&
                                                          keyboard.curCol === index
                        readonly property bool armed: key.t === "shift" && keyboard.shifted

                        width: keyboard.keyUnit * key.span
                               + keyboard.keySpacing * (key.span - 1)
                        height: keyboard.keyHeight
                        radius: Tokens.radiusControl

                        color: isCurrent && keyboard.activeFocus ? Tokens.surfaceFocus
                             : keyArea.containsMouse             ? Tokens.surfaceFocus
                             : armed                             ? Tokens.surfaceFocus
                                                                 : Tokens.surface
                        border.width: isCurrent && keyboard.activeFocus
                                      ? Tokens.focusStroke : Tokens.routeStroke
                        border.color: isCurrent && keyboard.activeFocus
                                      ? Tokens.borderFocus
                                      : armed ? Tokens.link : Tokens.border

                        Behavior on color {
                            ColorAnimation { duration: Tokens.motion(Tokens.durationFast) }
                        }

                        Accessible.role: Accessible.Button
                        Accessible.name: keyboard.keyLabel(key)
                        Accessible.onPressAction: {
                            keyboard.curRow = keyRow.rowIndex
                            keyboard.curCol = index
                            keyboard.directInput = false
                            keyboard.pressCurrent()
                        }

                        Label {
                            anchors.centerIn: parent
                            text: keyboard.keyLabel(keyRect.key)
                            font.family: Tokens.familyBody
                            font.pixelSize: keyRect.key.t === "char"
                                            ? (keyboard.digitsOnly
                                               ? Tokens.tTitle : Tokens.tShelf)
                                            : Tokens.tMicro
                            font.weight: keyRect.key.t === "char"
                                         ? Font.Medium : Font.DemiBold
                            font.capitalization: keyRect.key.t === "char"
                                                 ? Font.MixedCase : Font.AllUppercase
                            color: keyRect.key.t === "done" ? Tokens.link
                                                            : Tokens.textPrimary
                        }

                        // Press feedback shared by mouse and gamepad activation.
                        Rectangle {
                            id: pressFlash
                            anchors.fill: parent
                            radius: parent.radius
                            color: Tokens.accentFocus
                            opacity: 0
                        }

                        NumberAnimation {
                            id: flashAnim
                            target: pressFlash
                            property: "opacity"
                            from: 0.35
                            to: 0
                            duration: Tokens.motion(Tokens.durationFast)
                        }

                        Connections {
                            target: keyboard
                            function onKeyActivated(row, col) {
                                if (row === keyRow.rowIndex && col === index)
                                    flashAnim.restart()
                            }
                        }

                        MouseArea {
                            id: keyArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: Tokens.inputMode = "pointer"
                            onClicked: {
                                keyboard.forceActiveFocus()
                                keyboard.curRow = keyRow.rowIndex
                                keyboard.curCol = index
                                keyboard.directInput = false
                                keyboard.pressCurrent()
                            }
                        }
                    }
                }
            }
        }
    }

}
