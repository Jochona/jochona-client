import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import ComputerModel 1.0

import ComputerManager 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import SdlGamepadKeyNavigation 1.0

import "style"

// Jochona: M1 modern home screen (ADR-0001). Feature-flagged replacement for
// PcView (StreamingPreferences.modernHomeScreen). Controller-first host list:
// one full-width row per Host, status expressed by color plus text (never color
// alone), all PcView actions preserved (apps, pairing, wake, test, rename,
// delete, details). Dialogs are duplicated from PcView deliberately; they are
// deleted with it when the flag is retired.
CenteredGridView {
    property ComputerModel computerModel : createModel()

    id: homeGrid
    focus: true
    activeFocusOnTab: true
    topMargin: 24
    bottomMargin: 8
    // Single centered column of wide rows
    cellWidth: homeGrid.availableWidth > 0 ? Math.min(homeGrid.availableWidth, Tokens.listMaxWidth) : Tokens.listMaxWidth
    cellHeight: Tokens.rowHeight
    objectName: qsTr("Home")

    Component.onCompleted: {
        // Match PcView: no highlighted row until the user interacts.
        currentIndex = -1
    }

    // Note: Any initialization done here that is critical for streaming must
    // also be done in CliStartStreamSegue.qml, since this code does not run
    // for command-line initiated streams.
    StackView.onActivated: {
        ComputerManager.computerAddCompleted.connect(addComplete)

        if (currentIndex === -1 && SdlGamepadKeyNavigation.getConnectedGamepads() > 0) {
            currentIndex = 0
        }
    }

    StackView.onDeactivating: {
        ComputerManager.computerAddCompleted.disconnect(addComplete)
    }

    function pairingComplete(error)
    {
        pairDialog.close()

        if (error !== undefined) {
            errorDialog.text = error
            errorDialog.helpText = ""
            errorDialog.open()
        }
    }

    function addComplete(success, detectedPortBlocking)
    {
        if (!success) {
            errorDialog.text = qsTr("Unable to connect to the specified PC.")

            if (detectedPortBlocking) {
                errorDialog.text += "\n\n" + qsTr("This PC's Internet connection is blocking Jochona. Streaming over the Internet may not work while connected to this network.")
            }
            else {
                errorDialog.helpText = qsTr("Click the Help button for possible solutions.")
            }

            errorDialog.open()
        }
    }

    function createModel()
    {
        var model = Qt.createQmlObject('import ComputerModel 1.0; ComputerModel {}', parent, '')
        model.initialize(ComputerManager)
        model.pairingCompleted.connect(pairingComplete)
        model.connectionTestCompleted.connect(testConnectionDialog.connectionTestComplete)
        return model
    }

    // One-line availability description; state is never conveyed by color alone.
    function statusText(online, paired, statusUnknown, wakeable, waking)
    {
        if (waking)
            return qsTr("Waking…")
        if (statusUnknown)
            return qsTr("Checking…")
        if (!online)
            return wakeable ? qsTr("Offline — wakeable") : qsTr("Offline")
        if (!paired)
            return qsTr("Ready to pair")
        return qsTr("Online — ready")
    }

    function statusColor(online, paired, statusUnknown)
    {
        if (statusUnknown)
            return Tokens.statusUnknown
        if (!online)
            return Tokens.statusOffline
        if (!paired)
            return Tokens.statusPairing
        return Tokens.statusOnline
    }

    model: computerModel

    delegate: NavigableItemDelegate {
        id: hostRow
        width: homeGrid.cellWidth
        height: homeGrid.cellHeight
        grid: homeGrid

        property alias pcContextMenu : pcContextMenuLoader.item

        // Visible waking state (M1 WOL): wakeComputer() is fire-and-forget in
        // the model, so we show an optimistic Waking state until the poller
        // flips the host online, with a timeout so a dead host recovers the row.
        readonly property bool waking: wakeTimer.running && !model.online
        Timer {
            id: wakeTimer
            interval: 45000
        }

        // Card background; the base ItemDelegate style is left untouched so the
        // rest of the app is visually unaffected by this screen's existence.
        Rectangle {
            anchors.fill: parent
            anchors.margins: 4
            radius: Tokens.radiusCard
            color: parent.highlighted ? Tokens.surfaceFocus : Tokens.surface
            border.width: parent.highlighted ? 2 : 1
            border.color: parent.highlighted ? Tokens.borderFocus : Tokens.border
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Tokens.gutter
            anchors.rightMargin: 16
            spacing: 18

            // Status well: spinner / lock / dot
            Item {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                Layout.alignment: Qt.AlignVCenter

                Rectangle {
                    anchors.centerIn: parent
                    width: 20
                    height: 20
                    radius: 10
                    visible: !model.statusUnknown && !hostRow.waking
                    color: homeGrid.statusColor(model.online, model.paired, model.statusUnknown)
                }

                Image {
                    anchors.centerIn: parent
                    visible: !model.statusUnknown && model.online && !model.paired
                    source: "qrc:/res/baseline-lock-24px.svg"
                    sourceSize { width: 20; height: 20 }
                }

                BusyIndicator {
                    anchors.centerIn: parent
                    width: 30
                    height: 30
                    visible: model.statusUnknown || hostRow.waking
                    running: visible
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 2

                Label {
                    text: model.name
                    font.pointSize: Tokens.sizeSection
                    font.family: Tokens.fontDisplay
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Label {
                    text: homeGrid.statusText(model.online, model.paired, model.statusUnknown,
                                              model.wakeable, hostRow.waking)
                    font.pointSize: Tokens.sizeBody
                    color: Tokens.textSecondary
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            // Offline rows offer the contextual action inline; online rows show
            // a chevron hinting at the app list.
            Label {
                Layout.alignment: Qt.AlignVCenter
                visible: !model.online && model.wakeable && !hostRow.waking
                text: qsTr("Wake")
                font.pointSize: Tokens.sizeAction
                font.bold: true
                color: parent.parent.highlighted ? Tokens.accentFocus : Tokens.accent
            }

            Label {
                Layout.alignment: Qt.AlignVCenter
                visible: model.online && model.paired
                text: "›"
                font.pointSize: 28
                color: Tokens.accent
            }
        }

        Loader {
            id: pcContextMenuLoader
            asynchronous: true
            sourceComponent: NavigableMenu {
                id: pcContextMenu
                initiator: pcContextMenuLoader.parent
                MenuItem {
                    text: qsTr("PC Status: %1").arg(model.online ? qsTr("Online") : qsTr("Offline"))
                    font.bold: true
                    enabled: false
                }
                NavigableMenuItem {
                    text: qsTr("View All Apps")
                    onTriggered: {
                        var component = Qt.createComponent("AppView.qml")
                        var appView = component.createObject(stackView, {"computerIndex": index, "objectName": model.name, "showHiddenGames": true})
                        stackView.push(appView)
                    }
                    visible: model.online && model.paired
                }
                NavigableMenuItem {
                    // ADR-0004: Direct Wake only — the magic packet cannot cross
                    // an overlay like Tailscale, so the constraint is stated here.
                    text: qsTr("Wake PC") + (model.online ? "" : "\n" + qsTr("(Wake-on-LAN; only reaches hosts on the same local network)"))
                    onTriggered: {
                        computerModel.wakeComputer(index)
                        wakeTimer.restart()
                    }
                    visible: !model.online && model.wakeable
                }
                NavigableMenuItem {
                    text: qsTr("Test Network")
                    onTriggered: {
                        computerModel.testConnectionForComputer(index)
                        testConnectionDialog.open()
                    }
                }

                NavigableMenuItem {
                    text: qsTr("Rename PC")
                    onTriggered: {
                        renamePcDialog.pcIndex = index
                        renamePcDialog.originalName = model.name
                        renamePcDialog.open()
                    }
                }
                NavigableMenuItem {
                    text: qsTr("Delete PC")
                    onTriggered: {
                        deletePcDialog.pcIndex = index
                        deletePcDialog.pcName = model.name
                        deletePcDialog.open()
                    }
                }
                NavigableMenuItem {
                    text: qsTr("View Details")
                    onTriggered: {
                        showPcDetailsDialog.pcDetails = model.details
                        showPcDetailsDialog.open()
                    }
                }
            }
        }

        onClicked: {
            if (model.online) {
                if (!model.serverSupported) {
                    errorDialog.text = qsTr("The version of GeForce Experience on %1 is not supported by this build of Jochona. You must update Jochona to stream from %1.").arg(model.name)
                    errorDialog.helpText = ""
                    errorDialog.open()
                }
                else if (model.paired) {
                    var component = Qt.createComponent("AppView.qml")
                    var appView = component.createObject(stackView, {"computerIndex": index, "objectName": model.name})
                    stackView.push(appView)
                }
                else {
                    var pin = computerModel.generatePinString()
                    computerModel.pairComputer(index, pin)
                    pairDialog.pin = pin
                    pairDialog.open()
                }
            } else if (model.wakeable && !hostRow.waking) {
                // Gamepad-first: Enter on an offline wakeable host is the
                // primary action (wake), not the menu. Menu stays on hold.
                computerModel.wakeComputer(index)
                wakeTimer.restart()
            } else {
                // Using open() here because it may be activated by keyboard
                pcContextMenu.open()
            }
        }

        onPressAndHold: {
            if (pcContextMenu.popup) {
                pcContextMenu.popup()
            }
            else {
                pcContextMenu.open()
            }
        }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.RightButton;
            onClicked: {
                parent.pressAndHold()
            }
        }

        Keys.onMenuPressed: {
            pcContextMenu.open()
        }

        Keys.onDeletePressed: {
            deletePcDialog.pcIndex = index
            deletePcDialog.pcName = model.name
            deletePcDialog.open()
        }
    }

    // Empty state
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12
        visible: homeGrid.count === 0

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 10

            BusyIndicator {
                id: searchSpinner
                visible: StreamingPreferences.enableMdns
                running: visible
                Layout.preferredWidth: 36
                Layout.preferredHeight: 36
            }

            Label {
                text: StreamingPreferences.enableMdns ? qsTr("Looking for hosts on your network…")
                                                      : qsTr("No hosts yet")
                font.pointSize: 22
                font.family: "Space Grotesk"
                font.bold: true
                Layout.alignment: Qt.AlignVCenter
            }
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 600
            horizontalAlignment: Text.AlignHCenter
            text: StreamingPreferences.enableMdns ?
                      qsTr("Add a PC with the + button above. Hosts running Sunshine, Apollo, or Vibepollo appear here automatically.") :
                      qsTr("Automatic discovery is off. Add a PC with the + button above.")
            font.pointSize: 12
            color: "#9fa8ba"
            wrapMode: Text.Wrap
        }
    }

    ScrollBar.vertical: ScrollBar {}

    ErrorMessageDialog {
        id: errorDialog

        // Using Setup-Guide here instead of Troubleshooting because it's likely that users
        // will arrive here by forgetting to enable GameStream or not forwarding ports.
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide"
    }

    NavigableMessageDialog {
        id: pairDialog
        closePolicy: Popup.CloseOnEscape

        // don't allow edits to the rest of the window while open
        property string pin : "0000"
        text:qsTr("Please enter %1 on your host PC. This dialog will close when pairing is completed.").arg(pin)+"\n\n"+
             qsTr("If your host PC is running Sunshine, navigate to the Sunshine web UI to enter the PIN.")
        standardButtons: Dialog.Cancel
        onRejected: {
            // FIXME: We should interrupt pairing here
        }
    }

    NavigableMessageDialog {
        id: deletePcDialog
        // don't allow edits to the rest of the window while open
        property int pcIndex : -1
        property string pcName : ""
        text: qsTr("Are you sure you want to remove '%1'?").arg(pcName)
        standardButtons: Dialog.Yes | Dialog.No

        onAccepted: {
            computerModel.deleteComputer(pcIndex)
        }
    }

    NavigableMessageDialog {
        id: testConnectionDialog
        closePolicy: Popup.CloseOnEscape
        standardButtons: Dialog.Ok

        onAboutToShow: {
            testConnectionDialog.text = qsTr("Jochona is testing your network connection to determine if any required ports are blocked.") + "\n\n" + qsTr("This may take a few seconds…")
            showSpinner = true
        }

        function connectionTestComplete(result, blockedPorts)
        {
            if (result === -1) {
                text = qsTr("The network test could not be performed because none of Moonlight's connection testing servers were reachable from this PC. Check your Internet connection or try again later.")
                imageSrc = "qrc:/res/baseline-warning-24px.svg"
            }
            else if (result === 0) {
                // Jochona: first clause is this app; "Moonlight Internet Hosting Tool" is
                // upstream's separately-named tool and its testing servers, kept as-is.
                text = qsTr("This network does not appear to be blocking Jochona. If you still have trouble connecting, check your PC's firewall settings.") + "\n\n" + qsTr("If you are trying to stream over the Internet, install the Moonlight Internet Hosting Tool on your gaming PC and run the included Internet Streaming Tester to check your gaming PC's Internet connection.")
                imageSrc = "qrc:/res/baseline-check_circle_outline-24px.svg"
            }
            else {
                text = qsTr("Your PC's current network connection seems to be blocking Jochona. Streaming over the Internet may not work while connected to this network.") + "\n\n" + qsTr("The following network ports were blocked:") + "\n"
                text += blockedPorts
                imageSrc = "qrc:/res/baseline-error_outline-24px.svg"
            }

            // Stop showing the spinner and show the image instead
            showSpinner = false
        }
    }

    NavigableDialog {
        id: renamePcDialog
        property string label: qsTr("Enter the new name for this PC:")
        property string originalName
        property int pcIndex : -1;

        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: {
            // Force keyboard focus on the textbox so keyboard navigation works
            editText.forceActiveFocus()
        }

        onClosed: {
            editText.clear()
        }

        onAccepted: {
            if (editText.text) {
                computerModel.renameComputer(pcIndex, editText.text)
            }
        }

        ColumnLayout {
            Label {
                text: renamePcDialog.label
                font.bold: true
            }

            TextField {
                id: editText
                placeholderText: renamePcDialog.originalName
                Layout.fillWidth: true
                focus: true

                Keys.onReturnPressed: {
                    renamePcDialog.accept()
                }

                Keys.onEnterPressed: {
                    renamePcDialog.accept()
                }
            }
        }
    }

    NavigableMessageDialog {
        id: showPcDetailsDialog
        property string pcDetails : "";
        text: showPcDetailsDialog.pcDetails
        imageSrc: "qrc:/res/baseline-help_outline-24px.svg"
        standardButtons: Dialog.Ok
    }
}
