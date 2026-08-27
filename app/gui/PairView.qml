import QtQuick 2.15
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import ComputerModel 1.0

import "style"

// THESIS: Pairing is a physical link completing between two machines that
//   face each other; this screen shows both ends at once and refuses the
//   category default — a modal with a code in it.
// OWN-WORLD: Jochona neon-on-navy. Tokens surfaces and borders, Space
//   Grotesk poster digits, Inter body, status always dot+word, never color
//   alone.
// FIRST-VIEWPORT: Two device panels — this device and the host — separated
//   by a gap; the PIN bridges the gap at a scale readable from a couch 3m
//   away.
// SIGNATURE-MOMENT: While the host decides, a pulse travels the dashed wire
//   toward it; on approval the wire snaps solid and both panels flip to
//   "linked" together, then the screen bows out.
// REFUSALS: No modal, no color-only state, no spinner-only waiting, no PIN
//   in body type, no new hexes outside Tokens.
FocusScope {
    id: pairView

    property ComputerModel computerModel
    property int computerIndex: -1

    // Host state mirrored out of the model row (kept live by the Bindings
    // below, so polling updates drive the ceremony).
    property string hostName: ""
    property bool hostOnline: false
    property bool hostPaired: false
    property bool hostStatusUnknown: true

    // Ceremony phases: "waiting" | "showPin" | "enterPin" | "success" | "failed"
    property string phase: "waiting"
    // Who authored the active code: "client" (we generated it) or "host"
    // (the user typed the code the host is showing).
    property string pinSource: "client"
    property string pin: ""
    property string failReason: ""

    objectName: qsTr("Pairing")

    StackView.onActivated: {
        computerModel.pairingCompleted.connect(handlePairingCompleted)
        evaluate()
        refocus()
    }

    StackView.onDeactivating: {
        computerModel.pairingCompleted.disconnect(handlePairingCompleted)
    }

    onHostOnlineChanged: {
        if (phase === "waiting")
            evaluate()
    }

    onHostPairedChanged: {
        if (hostPaired && phase !== "success" && phase !== "failed") {
            // Paired from elsewhere (or a completed attempt we missed).
            phase = "success"
            popTimer.start()
        }
    }

    onPhaseChanged: refocus()

    function evaluate() {
        if (hostPaired) {
            phase = "success"
            popTimer.start()
        }
        else if (hostOnline) {
            startPairing()
        }
        else {
            phase = "waiting"
        }
    }

    function startPairing() {
        pin = computerModel.generatePinString()
        pinSource = "client"
        computerModel.pairComputer(computerIndex, pin)
        phase = "showPin"
    }

    function submitHostPin(entered) {
        pin = entered
        pinSource = "host"
        computerModel.pairComputer(computerIndex, entered)
        phase = "showPin"
    }

    function handlePairingCompleted(error) {
        if (error !== undefined) {
            failReason = error
            phase = "failed"
        }
        else {
            phase = "success"
            popTimer.start()
        }
    }

    function refocus() {
        if (phase === "enterPin")
            pinPad.forceActiveFocus()
        else if (phase === "failed")
            retryButton.forceActiveFocus()
        else if (phase === "showPin" && pinSource === "client")
            hostPinButton.forceActiveFocus()
        else if (phase === "waiting" || phase === "showPin")
            cancelButton.forceActiveFocus()
        else
            pairView.forceActiveFocus()
    }

    // The user backs out of PIN entry to the ceremony, and out of the
    // ceremony to the previous screen. Pairing cannot be interrupted
    // backend-side; leaving simply stops listening, like upstream's Cancel.
    Keys.onEscapePressed: function(event) {
        if (phase === "enterPin") {
            phase = "showPin"
            event.accepted = true
        }
        else {
            event.accepted = false
        }
    }

    Timer {
        id: popTimer
        interval: 1300
        onTriggered: stackView.pop()
    }

    // Live mirror of the target model row. Bindings survive dataChanged and
    // keep the ceremony honest without touching C++.
    Repeater {
        model: pairView.computerModel

        Item {
            visible: false
            readonly property bool isTarget: index === pairView.computerIndex

            Binding { target: pairView; property: "hostName"; value: model.name; when: isTarget }
            Binding { target: pairView; property: "hostOnline"; value: model.online; when: isTarget }
            Binding { target: pairView; property: "hostPaired"; value: model.paired; when: isTarget }
            Binding { target: pairView; property: "hostStatusUnknown"; value: model.statusUnknown; when: isTarget }
        }
    }

    // --- Copy ---

    function stageCaption() {
        switch (phase) {
        case "waiting": return qsTr("Your pairing code appears here")
        case "showPin": return pinSource === "client"
                        ? qsTr("Enter this code on %1").arg(hostName)
                        : qsTr("Sent — the host's code is now the pairing code")
        case "success": return qsTr("Devices linked")
        case "failed":  return qsTr("Pairing didn't complete")
        }
        return ""
    }

    function deviceStatusWord() {
        switch (phase) {
        case "waiting":  return qsTr("Ready")
        case "showPin":  return qsTr("Code issued")
        case "enterPin": return qsTr("Typing host's code")
        case "success":  return qsTr("Trusted")
        case "failed":   return qsTr("Not linked yet")
        }
        return ""
    }

    function hostStatusWord() {
        switch (phase) {
        case "waiting":  return hostStatusUnknown ? qsTr("Checking…") : qsTr("Offline")
        case "showPin":  return qsTr("Approval pending")
        case "enterPin": return qsTr("Showing a code")
        case "success":  return qsTr("Paired")
        case "failed":   return qsTr("Declined")
        }
        return ""
    }

    function hostStatusColor() {
        switch (phase) {
        case "waiting":  return hostStatusUnknown ? Tokens.statusUnknown : Tokens.statusOffline
        case "success":  return Tokens.statusOnline
        case "failed":   return Tokens.statusOffline
        }
        return Tokens.statusPairing
    }

    function hostBody() {
        switch (phase) {
        case "waiting":
            return qsTr("Make sure %1 is turned on, connected to your network, and running Sunshine, Apollo, or Vibepollo.").arg(hostName)
        case "showPin":
            return pinSource === "client"
                   ? qsTr("Open the host app on %1 — in Sunshine that's the web UI's PIN page — and type the code.").arg(hostName)
                   : qsTr("Waiting for %1 to approve this device.").arg(hostName)
        case "enterPin":
            return ""
        case "success":
            return qsTr("%1 now trusts this device. You won't need a code again.").arg(hostName)
        case "failed":
            return failReason
        }
        return ""
    }

    // --- Shared building blocks ---

    component StatusRow: Row {
        property alias word: statusLabel.text
        property alias dotColor: statusDot.color
        spacing: 8

        Rectangle {
            id: statusDot
            width: 10
            height: 10
            radius: 5
            anchors.verticalCenter: parent.verticalCenter
        }

        Label {
            id: statusLabel
            anchors.verticalCenter: parent.verticalCenter
            font.pointSize: Tokens.sizeMicro
            font.family: Tokens.familyBody
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1
            color: Tokens.textPrimary
        }
    }

    component MirrorPanel: Rectangle {
        id: panel

        property string title: ""
        property string iconSource: ""
        property string statusWord: ""
        property color statusColor: Tokens.statusUnknown
        property string body: ""
        property bool showSpinner: false
        property bool linked: false

        radius: Tokens.radiusCard
        color: Tokens.surface
        border.width: linked ? 2 : 1
        border.color: linked ? Tokens.statusOnline : Tokens.border

        Behavior on border.color {
            ColorAnimation { duration: Tokens.motion(Tokens.durationBase) }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Tokens.gutter
            spacing: 10

            Image {
                source: panel.iconSource
                sourceSize.width: 44
                sourceSize.height: 44
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
            }

            Label {
                text: panel.title
                font.pointSize: Tokens.sizeSection
                font.family: Tokens.familyDisplay
                font.bold: true
                elide: Text.ElideRight
                color: Tokens.textPrimary
                Layout.fillWidth: true
            }

            StatusRow {
                word: panel.statusWord
                dotColor: panel.statusColor
            }

            Label {
                text: panel.body
                visible: text !== ""
                font.pointSize: Tokens.sizeBody
                font.family: Tokens.familyBody
                color: Tokens.textSecondary
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                Layout.fillHeight: true
                verticalAlignment: Text.AlignTop
            }

            Item { Layout.fillHeight: panel.body === "" }

            BusyIndicator {
                visible: panel.showSpinner
                running: visible
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
            }
        }
    }

    component PinCells: Row {
        id: cells

        // The digits to show; padded with placeholders up to `slots`.
        property string digits: ""
        property int slots: 4
        // Index of the cell awaiting input (-1 when not entering).
        property int activeIndex: -1
        property bool linked: false
        property int cellWidth: 84
        property int cellHeight: 112

        spacing: 14

        Repeater {
            model: cells.slots

            Rectangle {
                readonly property bool filled: index < cells.digits.length
                readonly property bool active: index === cells.activeIndex

                width: cells.cellWidth
                height: cells.cellHeight
                radius: Tokens.radiusCard
                color: Tokens.surface
                border.width: active || cells.linked ? 2 : 1
                border.color: cells.linked ? Tokens.statusOnline
                            : active       ? Tokens.accentFocus
                            : filled       ? Tokens.accent
                                           : Tokens.border

                Behavior on border.color {
                    ColorAnimation { duration: Tokens.motion(Tokens.durationFast) }
                }

                Label {
                    anchors.centerIn: parent
                    text: filled ? cells.digits[index] : "–"
                    font.pointSize: 52
                    font.family: Tokens.familyDisplay
                    font.bold: true
                    color: filled ? Tokens.textPrimary : Tokens.textSecondary
                }

                // Entry cursor: a quiet underline in the awaiting cell.
                Rectangle {
                    visible: active
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 16
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: parent.width - 44
                    height: 3
                    radius: 1.5
                    color: Tokens.accentFocus
                }
            }
        }
    }

    // --- Stage ---

    Rectangle {
        anchors.fill: parent
        color: Qt.darker(Tokens.surface, 1.6)
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Tokens.gutter
        anchors.leftMargin: Math.max(Tokens.gutter, (parent.width - Tokens.listMaxWidth) / 2)
        anchors.rightMargin: anchors.leftMargin
        spacing: 8

        Label {
            text: qsTr("Pair with %1").arg(pairView.hostName)
            font.pointSize: Tokens.sizeHero
            font.family: Tokens.familyDisplay
            font.bold: true
            color: Tokens.textPrimary
            elide: Text.ElideRight
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            topPadding: 6
        }

        Label {
            text: qsTr("A one-time code links this device to your host. It takes about a minute.")
            font.pointSize: Tokens.sizeBody
            font.family: Tokens.familyBody
            color: Tokens.textSecondary
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }

        Item {
            id: stage
            Layout.fillWidth: true
            Layout.fillHeight: true

            readonly property real panelWidth: Math.min(340, width * 0.27)
            readonly property bool roomy: height >= 470

            // Ceremony composition: panels flanking the bridged gap.
            Item {
                id: ceremony
                anchors.fill: parent
                opacity: pairView.phase === "enterPin" ? 0 : 1
                visible: opacity > 0

                Behavior on opacity {
                    NumberAnimation { duration: Tokens.motion(Tokens.durationBase); easing.type: Easing.OutQuart }
                }

                MirrorPanel {
                    id: devicePanel
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: stage.panelWidth
                    height: Math.min(320, parent.height - 24)
                    title: qsTr("This device")
                    iconSource: "qrc:/res/jochona-512.png"
                    statusWord: pairView.deviceStatusWord()
                    statusColor: pairView.phase === "success" ? Tokens.statusOnline : Tokens.statusPairing
                    body: qsTr("Jochona holds the code. Nothing to do on this side.")
                    linked: pairView.phase === "success"
                }

                MirrorPanel {
                    id: hostPanel
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: stage.panelWidth
                    height: devicePanel.height
                    title: pairView.hostName
                    iconSource: "qrc:/res/desktop_windows-48px.svg"
                    statusWord: pairView.hostStatusWord()
                    statusColor: pairView.hostStatusColor()
                    body: pairView.hostBody()
                    showSpinner: pairView.phase === "waiting" || pairView.phase === "showPin"
                    linked: pairView.phase === "success"
                }

                // The wire: a dashed link that snaps solid on approval.
                Item {
                    id: wire
                    anchors.left: devicePanel.right
                    anchors.right: hostPanel.left
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    height: 8

                    readonly property int dashWidth: 12
                    readonly property int dashGap: 9

                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: wire.dashGap
                        opacity: pairView.phase === "success" ? 0 : 1

                        Behavior on opacity {
                            NumberAnimation { duration: Tokens.motion(Tokens.durationBase) }
                        }

                        Repeater {
                            model: Math.max(0, Math.floor((wire.width + wire.dashGap) / (wire.dashWidth + wire.dashGap)))

                            Rectangle {
                                width: wire.dashWidth
                                height: 2
                                color: pairView.phase === "showPin" ? Tokens.accent : Tokens.border
                            }
                        }
                    }

                    Rectangle {
                        id: solidWire
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        height: 2
                        color: Tokens.statusOnline
                        transform: Scale {
                            origin.x: solidWire.width / 2
                            xScale: pairView.phase === "success" ? 1 : 0

                            Behavior on xScale {
                                NumberAnimation { duration: Tokens.motion(Tokens.durationBase); easing.type: Easing.OutQuart }
                            }
                        }
                    }

                    // The request traveling toward the host while it decides.
                    Rectangle {
                        id: pulse
                        width: 8
                        height: 8
                        radius: 4
                        anchors.verticalCenter: parent.verticalCenter
                        color: Tokens.accentFocus
                        visible: pairView.phase === "showPin" && Tokens.motion(1) > 0

                        SequentialAnimation {
                            running: pulse.visible
                            loops: Animation.Infinite

                            NumberAnimation {
                                target: pulse
                                property: "x"
                                from: 0
                                to: wire.width - pulse.width
                                duration: 1600
                                easing.type: Easing.InOutQuad
                            }

                            PropertyAction { target: pulse; property: "x"; value: 0 }
                        }
                    }
                }

                // The code, bridging the gap at poster scale.
                PinCells {
                    id: posterCells
                    anchors.centerIn: parent
                    digits: pairView.phase === "waiting" ? "" : pairView.pin
                    linked: pairView.phase === "success"
                    cellWidth: stage.roomy ? 84 : 72
                    cellHeight: stage.roomy ? 112 : 92
                }

                Label {
                    anchors.top: posterCells.bottom
                    anchors.topMargin: 14
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.max(220, Math.min(420, stage.width - 2 * stage.panelWidth - 48))
                    text: pairView.stageCaption()
                    font.pointSize: Tokens.sizeBody
                    font.family: Tokens.familyBody
                    color: Tokens.textSecondary
                    wrapMode: Text.Wrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            // Entry composition: compact ends, the pad takes the floor.
            Item {
                id: entry
                anchors.fill: parent
                opacity: pairView.phase === "enterPin" ? 1 : 0
                visible: opacity > 0

                Behavior on opacity {
                    NumberAnimation { duration: Tokens.motion(Tokens.durationBase); easing.type: Easing.OutQuart }
                }

                Column {
                    anchors.centerIn: parent
                    spacing: 16

                    PinCells {
                        id: entryCells
                        anchors.horizontalCenter: parent.horizontalCenter
                        digits: pinPad.text
                        activeIndex: pinPad.text.length >= 4 ? -1 : pinPad.text.length
                        cellWidth: stage.roomy ? 76 : 64
                        cellHeight: stage.roomy ? 96 : 80
                    }

                    Label {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Math.min(460, stage.width - 48)
                        text: qsTr("Type the 4-digit code shown on %1. It becomes the pairing code.").arg(pairView.hostName)
                        font.pointSize: Tokens.sizeBody
                        font.family: Tokens.familyBody
                        color: Tokens.textSecondary
                        wrapMode: Text.Wrap
                        horizontalAlignment: Text.AlignHCenter
                    }

                    OnScreenKeyboard {
                        id: pinPad
                        anchors.horizontalCenter: parent.horizontalCenter
                        digitsOnly: true
                        maxLength: 4
                        doneLabel: qsTr("Pair")
                        onTextChanged: {
                            if (text.length === 4)
                                submitTimer.restart()
                        }
                        onAccepted: {
                            if (text.length === 4) {
                                submitTimer.stop()
                                pairView.submitHostPin(text)
                            }
                        }
                        onNavigateDown: showMyPinButton.forceActiveFocus()
                    }

                    Timer {
                        id: submitTimer
                        interval: 350
                        onTriggered: {
                            if (pinPad.text.length === 4)
                                pairView.submitHostPin(pinPad.text)
                        }
                    }
                }
            }
        }

        // --- Actions ---

        RowLayout {
            Layout.fillWidth: true
            Layout.bottomMargin: 4
            spacing: 12

            Item { Layout.fillWidth: true }

            NavigableButton {
                id: hostPinButton
                visible: pairView.phase === "showPin" && pairView.pinSource === "client"
                text: qsTr("Host showing a code? Enter it")
                onClicked: {
                    pinPad.clear()
                    pairView.phase = "enterPin"
                }
            }

            NavigableButton {
                id: showMyPinButton
                visible: pairView.phase === "enterPin"
                text: qsTr("Show my code instead")
                Keys.onUpPressed: pinPad.forceActiveFocus()
                onClicked: pairView.phase = "showPin"
            }

            NavigableButton {
                id: retryButton
                visible: pairView.phase === "failed"
                primary: true
                text: qsTr("Try again")
                onClicked: pairView.evaluate()
            }

            NavigableButton {
                id: cancelButton
                visible: pairView.phase !== "success"
                text: pairView.phase === "failed" ? qsTr("Back") : qsTr("Cancel")
                Keys.onUpPressed: {
                    if (pairView.phase === "enterPin")
                        pinPad.forceActiveFocus()
                }
                onClicked: stackView.pop()
            }

            Item { Layout.fillWidth: true }
        }

        // Controller legend: which physical inputs do what, at a glance.
        Row {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 2
            spacing: 22

            ControllerHint { button: "A"; label: qsTr("Select") }
            ControllerHint { button: "B"; label: qsTr("Back") }
        }
    }
}
