import QtQuick 2.15
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import ComputerModel 1.0

import ComputerManager 1.0
import StreamingPreferences 1.0

import "style"

// Jochona: first-run guided setup (M2). Pushed above HomeView by main.qml
// only when no hosts are known yet; returning users never see it. Four
// steps — welcome, find your PC, add by address, done — every one skippable
// and reversible: B/Escape steps back, and backing out of the first step
// (or the toolbar arrow at any point) lands on Home. Pairing itself is
// PairView's ceremony, pushed on top; when it succeeds we advance to the
// done step so the flow resumes finished.
FocusScope {
    id: welcomeView

    // 0 welcome · 1 find · 2 address · 3 done
    property int step: 0
    property bool addingHost: false
    property string addressPending: ""
    property string addressError: ""

    property ComputerModel hostsModel: createModel()

    objectName: qsTr("Welcome")

    function createModel() {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', welcomeView, '')
        model.initialize(ComputerManager)
        model.pairingCompleted.connect(handlePairingCompleted)
        return model
    }

    Component.onCompleted: {
        ComputerManager.computerAddCompleted.connect(handleAddCompleted)
    }

    Component.onDestruction: {
        ComputerManager.computerAddCompleted.disconnect(handleAddCompleted)
    }

    StackView.onActivated: {
        // Returning from PairView with the chosen host now trusted (or it
        // got paired out-of-band) finishes the flow.
        if (step === 1 && hostGrid.currentItem !== null && hostGrid.currentItem.paired)
            step = 3
        refocus()
    }
    onStepChanged: refocus()

    function refocus() {
        if (step === 0)
            findButton.forceActiveFocus()
        else if (step === 1) {
            if (hostGrid.count > 0) {
                if (hostGrid.currentIndex === -1)
                    hostGrid.currentIndex = 0
                hostGrid.forceActiveFocus()
            }
            else {
                addressButton.forceActiveFocus()
            }
        }
        else if (step === 2)
            addressPad.forceActiveFocus()
        else
            doneButton.forceActiveFocus()
    }

    function handlePairingCompleted(error) {
        if (error === undefined)
            step = 3
    }

    function handleAddCompleted(success, detectedPortBlocking) {
        if (!addingHost)
            return
        addingHost = false
        if (success) {
            addressError = ""
            addressPad.clear()
            step = 1
        }
        else {
            addressError = detectedPortBlocking
                    ? qsTr("Couldn't reach %1. This network is blocking Jochona — streaming over the Internet may not work from here.").arg(addressPending)
                    : qsTr("Couldn't reach %1. Check the address and that the host app is running, then try again.").arg(addressPending)
        }
    }

    function submitAddress() {
        var addr = addressPad.text.trim()
        if (addr === "" || addingHost)
            return
        addressPending = addr
        addressError = ""
        addingHost = true
        ComputerManager.addNewHostManually(addr)
    }

    // B / Escape walks one step back; from the first step it bubbles up to
    // main.qml's StackView handler, which pops back to Home (skip).
    Keys.onEscapePressed: function(event) {
        if (step === 1 || step === 2) {
            addingHost = false
            step--
            event.accepted = true
        }
        else {
            event.accepted = false
        }
    }

    function statusWord(online, paired, statusUnknown) {
        if (statusUnknown)
            return qsTr("Checking…")
        if (!online)
            return qsTr("Offline")
        if (!paired)
            return qsTr("Ready to pair")
        return qsTr("Paired")
    }

    function statusColor(online, paired, statusUnknown) {
        if (statusUnknown)
            return Tokens.statusUnknown
        if (!online)
            return Tokens.statusOffline
        if (!paired)
            return Tokens.statusPairing
        return Tokens.statusOnline
    }

    Rectangle {
        anchors.fill: parent
        color: Qt.darker(Tokens.surface, 1.6)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tokens.gutter
        anchors.leftMargin: Math.max(Tokens.gutter, (parent.width - Tokens.listMaxWidth) / 2)
        anchors.rightMargin: anchors.leftMargin
        spacing: 8

        Item {
            id: stepArea
            Layout.fillWidth: true
            Layout.fillHeight: true

            readonly property bool roomy: height > 560

            // --- Step 0: welcome ---
            Item {
                anchors.fill: parent
                opacity: welcomeView.step === 0 ? 1 : 0
                visible: opacity > 0

                Behavior on opacity {
                    NumberAnimation { duration: Tokens.motion(Tokens.durationBase); easing.type: Easing.OutQuart }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(640, parent.width)
                    spacing: 14

                    Image {
                        source: "qrc:/res/jochona-512.png"
                        sourceSize.width: 96
                        sourceSize.height: 96
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 96
                        Layout.preferredHeight: 96
                    }

                    Label {
                        text: qsTr("Welcome to Jochona")
                        font.pointSize: Tokens.sizeHero
                        font.family: Tokens.familyDisplay
                        font.bold: true
                        color: Tokens.textPrimary
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Label {
                        text: qsTr("Stream games from your own PC to this screen — no account, just your hardware on your network.")
                        font.pointSize: Tokens.sizeBody
                        font.family: Tokens.familyBody
                        color: Tokens.textSecondary
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Label {
                        text: qsTr("You'll need a PC running Sunshine, Apollo, or Vibepollo.")
                        font.pointSize: Tokens.sizeBody
                        font.family: Tokens.familyBody
                        color: Tokens.textSecondary
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 12
                        spacing: 12

                        NavigableButton {
                            id: findButton
                            primary: true
                            text: qsTr("Find my gaming PC")
                            onClicked: welcomeView.step = 1
                        }

                        NavigableButton {
                            text: qsTr("Skip setup")
                            onClicked: stackView.pop()
                        }
                    }
                }
            }

            // --- Step 1: find your PC ---
            Item {
                anchors.fill: parent
                opacity: welcomeView.step === 1 ? 1 : 0
                visible: opacity > 0

                Behavior on opacity {
                    NumberAnimation { duration: Tokens.motion(Tokens.durationBase); easing.type: Easing.OutQuart }
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 6

                    Label {
                        text: qsTr("Choose your gaming PC")
                        font.pointSize: Tokens.sizeHero
                        font.family: Tokens.familyDisplay
                        font.bold: true
                        color: Tokens.textPrimary
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        topPadding: 6
                    }

                    Label {
                        text: StreamingPreferences.enableMdns
                              ? qsTr("PCs on your network appear here automatically.")
                              : qsTr("Automatic discovery is off — add your PC by address.")
                        font.pointSize: Tokens.sizeBody
                        font.family: Tokens.familyBody
                        color: Tokens.textSecondary
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    CenteredGridView {
                        id: hostGrid
                        // One centered column; sized to the row so a single
                        // discovered host sits centered, not parked left.
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: cellWidth
                        Layout.fillHeight: true
                        Layout.topMargin: 10
                        clip: true
                        minMargin: 0
                        cellWidth: Math.min(760, stepArea.width)
                        cellHeight: Tokens.rowHeight

                        model: welcomeView.hostsModel

                        // The moment the first host appears, make it the live
                        // selection so A/Enter acts on it immediately and the
                        // D-pad never lands on an empty grid.
                        onCountChanged: {
                            if (count > 0 && currentIndex === -1) {
                                currentIndex = 0
                                if (welcomeView.step === 1 && !addressPad.activeFocus)
                                    forceActiveFocus()
                            }
                        }

                        delegate: ItemDelegate {
                            id: hostRow
                            readonly property bool paired: model.paired
                            width: hostGrid.cellWidth
                            height: hostGrid.cellHeight
                            highlighted: hostGrid.activeFocus && hostGrid.currentItem === this

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 4
                                radius: Tokens.radiusCard
                                color: hostRow.highlighted ? Tokens.surfaceFocus : Tokens.surface
                                border.width: hostRow.highlighted ? 2 : 1
                                border.color: hostRow.highlighted ? Tokens.borderFocus : Tokens.border
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Tokens.gutter
                                anchors.rightMargin: 16
                                spacing: 18

                                Rectangle {
                                    width: 12
                                    height: 12
                                    radius: 6
                                    color: welcomeView.statusColor(model.online, model.paired, model.statusUnknown)
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: 2

                                    Label {
                                        text: model.name
                                        font.pointSize: Tokens.sizeSection
                                        font.family: Tokens.familyDisplay
                                        font.bold: true
                                        color: Tokens.textPrimary
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    Label {
                                        text: welcomeView.statusWord(model.online, model.paired, model.statusUnknown)
                                        font.pointSize: Tokens.sizeBody
                                        font.family: Tokens.familyBody
                                        color: Tokens.textSecondary
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }

                                Label {
                                    text: model.paired ? qsTr("Done") : qsTr("Pair")
                                    font.pointSize: Tokens.sizeBody
                                    font.family: Tokens.familyBody
                                    font.bold: true
                                    color: hostRow.highlighted ? Tokens.accentFocus : Tokens.accent
                                    Layout.alignment: Qt.AlignVCenter
                                }
                            }

                            function choose() {
                                if (model.paired) {
                                    welcomeView.step = 3
                                }
                                else {
                                    stackView.push("qrc:/gui/PairView.qml", {
                                        "computerModel": welcomeView.hostsModel,
                                        "computerIndex": index
                                    })
                                }
                            }

                            onClicked: choose()
                            Keys.onReturnPressed: choose()
                            Keys.onEnterPressed: choose()

                            Keys.onUpPressed: hostGrid.moveCurrentIndexUp()
                            Keys.onDownPressed: {
                                var atBottom = hostGrid.currentIndex === hostGrid.count - 1
                                if (atBottom)
                                    addressButton.forceActiveFocus()
                                else
                                    hostGrid.moveCurrentIndexDown()
                            }
                        }

                        // Empty state: discovery is genuinely running; say so.
                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 10
                            visible: hostGrid.count === 0

                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                spacing: 10

                                BusyIndicator {
                                    visible: StreamingPreferences.enableMdns
                                    running: visible
                                    Layout.preferredWidth: 32
                                    Layout.preferredHeight: 32
                                }

                                Label {
                                    text: StreamingPreferences.enableMdns
                                          ? qsTr("Looking for PCs on your network…")
                                          : qsTr("No PCs yet")
                                    font.pointSize: Tokens.sizeSection
                                    font.family: Tokens.familyDisplay
                                    font.bold: true
                                    color: Tokens.textPrimary
                                }
                            }

                            Label {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.maximumWidth: 520
                                text: qsTr("Hosts show up within a few seconds. If yours doesn't, add it by address below.")
                                font.pointSize: Tokens.sizeBody
                                font.family: Tokens.familyBody
                                color: Tokens.textSecondary
                                wrapMode: Text.Wrap
                                horizontalAlignment: Text.AlignHCenter
                            }
                        }
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 6
                        spacing: 12

                        NavigableButton {
                            id: addressButton
                            text: qsTr("Enter an address instead")
                            onClicked: welcomeView.step = 2
                            Keys.onUpPressed: {
                                if (hostGrid.count > 0) {
                                    if (hostGrid.currentIndex === -1)
                                        hostGrid.currentIndex = 0
                                    hostGrid.forceActiveFocus()
                                }
                            }
                        }

                        NavigableButton {
                            text: qsTr("Back")
                            onClicked: welcomeView.step = 0
                            Keys.onUpPressed: {
                                if (hostGrid.count > 0) {
                                    if (hostGrid.currentIndex === -1)
                                        hostGrid.currentIndex = 0
                                    hostGrid.forceActiveFocus()
                                }
                            }
                        }
                    }
                }
            }

            // --- Step 2: add by address ---
            Item {
                anchors.fill: parent
                opacity: welcomeView.step === 2 ? 1 : 0
                visible: opacity > 0

                Behavior on opacity {
                    NumberAnimation { duration: Tokens.motion(Tokens.durationBase); easing.type: Easing.OutQuart }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(860, parent.width)
                    spacing: 12

                    Label {
                        text: qsTr("Add your PC by address")
                        font.pointSize: Tokens.sizeHero
                        font.family: Tokens.familyDisplay
                        font.bold: true
                        color: Tokens.textPrimary
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Label {
                        visible: stepArea.roomy
                        text: qsTr("Type the IP address or hostname of your gaming PC.")
                        font.pointSize: Tokens.sizeBody
                        font.family: Tokens.familyBody
                        color: Tokens.textSecondary
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    // Composed value, caret included: the keyboard below is
                    // the input method, this field is the value.
                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: Math.min(560, parent.width - 24)
                        Layout.preferredHeight: 58
                        radius: Tokens.radiusCard
                        color: Tokens.surface
                        border.width: 1
                        border.color: addressPad.activeFocus ? Tokens.borderFocus : Tokens.border

                        Row {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 18
                            spacing: 2

                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                text: addressPad.text
                                visible: addressPad.text !== ""
                                font.pointSize: Tokens.sizeTitle
                                font.family: Tokens.familyBody
                                font.weight: Font.Medium
                                color: Tokens.textPrimary
                            }

                            Rectangle {
                                id: caret
                                anchors.verticalCenter: parent.verticalCenter
                                width: 2
                                height: 26
                                color: Tokens.accentFocus

                                SequentialAnimation on opacity {
                                    running: welcomeView.step === 2 && Tokens.motion(1) > 0
                                    loops: Animation.Infinite
                                    NumberAnimation { from: 1; to: 0; duration: 500 }
                                    NumberAnimation { from: 0; to: 1; duration: 500 }
                                }
                            }

                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                text: qsTr("192.168.1.50")
                                visible: addressPad.text === ""
                                font.pointSize: Tokens.sizeTitle
                                font.family: Tokens.familyBody
                                font.weight: Font.Medium
                                color: Tokens.textSecondary
                                opacity: 0.55
                            }
                        }
                    }

                    // Progress and failure both speak: never a bare spinner,
                    // never color alone.
                    RowLayout {
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 8
                        visible: welcomeView.addingHost || welcomeView.addressError !== ""

                        BusyIndicator {
                            visible: welcomeView.addingHost
                            running: visible
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                        }

                        Rectangle {
                            visible: !welcomeView.addingHost && welcomeView.addressError !== ""
                            width: 10
                            height: 10
                            radius: 5
                            color: Tokens.statusOffline
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Label {
                            text: welcomeView.addingHost
                                  ? qsTr("Contacting %1…").arg(welcomeView.addressPending)
                                  : welcomeView.addressError
                            font.pointSize: Tokens.sizeBody
                            font.family: Tokens.familyBody
                            color: Tokens.textSecondary
                            wrapMode: Text.Wrap
                            Layout.maximumWidth: 560
                        }
                    }

                    OnScreenKeyboard {
                        id: addressPad
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: Math.min(848, parent.width)
                        Layout.preferredHeight: implicitHeight
                        doneLabel: qsTr("Add")
                        onAccepted: welcomeView.submitAddress()
                        onNavigateDown: addressBackButton.forceActiveFocus()
                    }

                    NavigableButton {
                        id: addressBackButton
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Back")
                        Keys.onUpPressed: addressPad.forceActiveFocus()
                        onClicked: {
                            welcomeView.addingHost = false
                            welcomeView.step = 1
                        }
                    }
                }
            }

            // --- Step 3: done ---
            Item {
                anchors.fill: parent
                opacity: welcomeView.step === 3 ? 1 : 0
                visible: opacity > 0

                Behavior on opacity {
                    NumberAnimation { duration: Tokens.motion(Tokens.durationBase); easing.type: Easing.OutQuart }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(640, parent.width)
                    spacing: 14

                    Image {
                        source: "qrc:/res/baseline-check_circle_outline-24px.svg"
                        sourceSize.width: 64
                        sourceSize.height: 64
                        Layout.alignment: Qt.AlignHCenter
                        Layout.preferredWidth: 64
                        Layout.preferredHeight: 64
                    }

                    Label {
                        text: qsTr("You're set up")
                        font.pointSize: Tokens.sizeHero
                        font.family: Tokens.familyDisplay
                        font.bold: true
                        color: Tokens.textPrimary
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Label {
                        text: qsTr("Your host is paired. It lives on Home now — pick it any time to see your games.")
                        font.pointSize: Tokens.sizeBody
                        font.family: Tokens.familyBody
                        color: Tokens.textSecondary
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                    }

                    NavigableButton {
                        id: doneButton
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 12
                        primary: true
                        text: qsTr("Start playing")
                        onClicked: stackView.pop()
                    }
                }
            }
        }

        // Controller legend, constant across steps.
        Row {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 2
            spacing: 22

            ControllerHint { button: "A"; label: qsTr("Select") }
            ControllerHint { button: "B"; label: welcomeView.step === 0 ? qsTr("Skip") : qsTr("Back") }
        }
    }
}
