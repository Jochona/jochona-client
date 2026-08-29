// Night Route Home. Resume owns first focus; Rigs and Library are explicit
// destinations selected by the root route bar. No passive hero or shelf wall:
// the stage is an action and each list is a visible connection segment.
import QtQuick
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import ComputerModel 1.0
import ComputerManager 1.0
import SdlGamepadKeyNavigation 1.0
import LibraryManager 1.0
import EffectiveSettings 1.0

import "style"
import "session"

pragma ComponentBehavior: Bound

Item {
    id: home
    objectName: qsTr("Home")

    property ComputerModel computerModel: createModel()
    property string destination: "resume" // resume | rigs | library
    property int actionsHostIndex: -1
    property var recentEntries: []

    focus: true
    activeFocusOnTab: true

    function createModel()
    {
        var model = Qt.createQmlObject(
                    'import ComputerModel 1.0; ComputerModel {}', parent, '')
        model.initialize(ComputerManager)
        model.connectionTestCompleted.connect(testConnectionDialog.connectionTestComplete)
        return model
    }

    Connections {
        target: home.computerModel
        function onWakeReady(computerIndex, appId) {
            home.openHost(computerIndex,
                          home.computerModel.uuidForIndex(computerIndex),
                          home.computerModel.nameForIndex(computerIndex),
                          appId)
        }
    }

    readonly property var resumeSource: {
        if (recentEntries.length === 0)
            return null
        var entry = recentEntries[0]
        var hostIndex = home.computerModel.indexOfUuid(entry.uuid)
        return {
            heroPlayable: hostIndex >= 0,
            heroTitle: entry.name,
            heroSubtitle: qsTr("Recent"),
            heroArt: "",
            heroMeta: hostIndex >= 0
                      ? home.computerModel.nameForIndex(hostIndex) : "",
            heroHost: hostIndex >= 0
                      ? home.computerModel.nameForIndex(hostIndex) : "",
            heroDestination: entry.name,
            heroAction: qsTr("Resume"),
            heroActivate: function() {
                if (hostIndex < 0)
                    return
                if (home.computerModel.isOnlinePaired(hostIndex)) {
                    home.openHost(hostIndex, entry.uuid,
                                  home.computerModel.nameForIndex(hostIndex),
                                  entry.appid)
                } else {
                    home.computerModel.wakeComputer(hostIndex, true,
                                                    entry.appid)
                    home.showDestination("rigs")
                }
            }
        }
    }

    readonly property var heroSource: {
        if (destination === "library")
            return libraryShelf.focusedItem
        if (destination === "rigs")
            return rigsShelf.focusedItem
        return resumeSource !== null ? resumeSource : rigsShelf.focusedItem
    }

    function showDestination(nextDestination) {
        destination = ["resume", "rigs", "library"].indexOf(nextDestination) >= 0
                      ? nextDestination : "resume"
        Qt.callLater(function() {
            if (destination === "resume") {
                resumeStage.forceActiveFocus()
            } else if (destination === "rigs") {
                if (!rigsShelf.takeFocus())
                    addRigAction.forceActiveFocus()
            } else if (!libraryShelf.takeFocus()) {
                resumeStage.forceActiveFocus()
            }
        })
    }

    function grabFirstFocus() {
        showDestination(destination)
        return true
    }

    StackView.onActivated: {
        ComputerManager.computerAddCompleted.connect(home.addComplete)
        RecentApps.entriesChanged.connect(home.invalidateContinue)
        home.invalidateContinue()
        home.showDestination(home.destination)
    }

    StackView.onDeactivating: {
        ComputerManager.computerAddCompleted.disconnect(home.addComplete)
        RecentApps.entriesChanged.disconnect(home.invalidateContinue)
    }

    function invalidateContinue() {
        recentEntries = RecentApps.visibleEntries(home.computerModel, 10)
    }

    function addComplete(success, detectedPortBlocking)
    {
        if (!success) {
            errorDialog.text = qsTr("Jochona could not reach that rig.")
            if (detectedPortBlocking) {
                errorDialog.text += "\n\n" + qsTr(
                            "This network blocks incoming streaming connections. "
                            + "Try the same local network or review the router rules.")
            } else {
                errorDialog.helpText = qsTr(
                            "Check the address and confirm the host service is running.")
            }
            errorDialog.open()
        }
    }

    function statusText(online, paired, statusUnknown, wakeable, wakeState)
    {
        if (wakeState === "sending")
            return qsTr("Sending wake packets…")
        if (wakeState === "sent")
            return qsTr("Sent — waiting for the rig")
        if (wakeState === "failed")
            return qsTr("Failed — check Wake settings")
        if (wakeState === "ready")
            return qsTr("Ready")
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

    Keys.onDownPressed: function(event) {
        if (!resumeStage.activeFocus)
            return
        if (destination === "rigs") {
            if (!rigsShelf.takeFocus())
                addRigAction.forceActiveFocus()
            event.accepted = true
        } else if (destination === "library" && libraryShelf.count > 0) {
            libraryShelf.takeFocus()
            event.accepted = true
        }
    }

    Keys.onUpPressed: function(event) {
        if (!resumeStage.activeFocus) {
            resumeStage.forceActiveFocus()
            event.accepted = true
        }
    }

    Keys.onRightPressed: function(event) {
        if (destination === "rigs" && rigsShelf.count > 0
                && rigsShelf.currentIndex === rigsShelf.count - 1) {
            addRigAction.forceActiveFocus()
            event.accepted = true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Tokens.gutter

        Hero {
            id: resumeStage
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: Tokens.dp(Tokens.handheld ? 260 : 330)
            title: home.heroSource && home.heroSource.heroTitle !== undefined
                   ? home.heroSource.heroTitle : ""
            subtitle: home.heroSource && home.heroSource.heroSubtitle !== undefined
                      ? home.heroSource.heroSubtitle : ""
            art: home.heroSource && home.heroSource.heroArt !== undefined
                 ? home.heroSource.heroArt : ""
            metaLine: home.heroSource && home.heroSource.heroMeta !== undefined
                      ? home.heroSource.heroMeta : ""
            hostLabel: home.heroSource && home.heroSource.heroHost !== undefined
                       ? home.heroSource.heroHost : ""
            destinationLabel: home.heroSource
                              && home.heroSource.heroDestination !== undefined
                              ? home.heroSource.heroDestination : ""
            actionLabel: home.heroSource && home.heroSource.heroAction !== undefined
                         ? home.heroSource.heroAction : qsTr("Open")
            playable: home.heroSource !== null
            emptyHint: home.heroSource !== null ? ""
                       : home.destination === "library"
                         ? qsTr("Games appear here after Jochona reads a paired rig.")
                         : qsTr("Add a rig running Sunshine, Vibepollo, or Apollo.")

            onActivate: {
                if (home.heroSource && home.heroSource.heroActivate)
                    home.heroSource.heroActivate()
                else
                    home.openAddRigFlow()
            }
        }

        RowLayout {
            visible: home.destination === "rigs"
            Layout.fillWidth: true
            Layout.preferredHeight: visible
                                    ? Math.max(rigsShelf.height,
                                               addRigAction.implicitHeight) : 0
            spacing: Tokens.gutter

            Shelf {
                id: rigsShelf
                Layout.fillWidth: true
                Layout.preferredHeight: height
                label: qsTr("Rigs")
                emptyText: qsTr("No rigs yet.")
                itemHeight: Tokens.dp(150)
                model: home.computerModel

                delegate: HostCard {
                    id: hostStop
                    objectName: name

                    required property string name
                    required property bool online
                    required property bool paired
                    required property bool statusUnknown
                    required property bool wakeable
                    required property string connectionPath
                    required property string wakeState

                    hostName: name
                    availability: home.statusText(online, paired, statusUnknown,
                                                  wakeable, wakeState)
                    connectionLabel: online && paired && connectionPath !== "Unknown"
                                     ? connectionPath : ""
                    actionText: wakeState === "sending" ? qsTr("Sending…")
                                : wakeState === "sent" ? qsTr("Sent")
                                : wakeState === "failed" ? qsTr("Failed — retry")
                                : wakeState === "ready" ? qsTr("Ready")
                                : statusUnknown ? qsTr("Checking…")
                                : !online ? wakeable ? qsTr("Wake") : qsTr("Actions")
                                : !paired ? qsTr("Pair") : qsTr("Browse apps")
                    statusColor: home.statusColor(online, paired, statusUnknown)
                    unknown: statusUnknown
                    waking: wakeState === "sending" || wakeState === "sent"

                    onCardActivate: {
                        if (wakeState === "sending" || wakeState === "sent")
                            return
                        if (!online && wakeable) {
                            home.computerModel.wakeComputer(index, false, -1)
                        } else if (!online) {
                            home.hostActionsSheet_forIndex(index)
                        } else if (!paired) {
                            home.openPairing(index)
                        } else {
                            home.openHost(index,
                                          home.computerModel.uuidForIndex(index),
                                          name)
                        }
                    }
                    onPressHold: home.hostActionsSheet_forIndex(index)
                }
            }

            NavigableButton {
                id: addRigAction
                Layout.alignment: Qt.AlignBottom
                text: qsTr("Add a rig")
                description: ""
                primary: rigsShelf.count === 0
                onClicked: home.openAddRigFlow()
                Keys.onLeftPressed: {
                    if (rigsShelf.count > 0)
                        rigsShelf.takeFocus(rigsShelf.count - 1)
                }
                Keys.onUpPressed: resumeStage.forceActiveFocus()
            }
        }

        Shelf {
            id: libraryShelf
            visible: home.destination === "library" && count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? height : 0
            label: qsTr("Library")
            emptyText: qsTr("Pair a rig to build the unified Library.")
            itemHeight: Tokens.dp(158)
            model: LibraryManager.entries

            delegate: AppTile {
                required property var modelData
                objectName: modelData.title
                titleText: modelData.title
                artUrl: modelData.artwork || ""
                hostText: modelData.available
                          ? qsTr("%1 ready").arg(modelData.readyHostCount)
                          : qsTr("Offline")
                hidden: modelData.hidden

                onCardActivate: home.openLibraryEntry(modelData.id)
            }
        }
    }

    function openLibraryEntry(entryId)
    {
        var chosen = LibraryManager.bestHostCandidate(entryId)
        if (chosen.hostUuid === undefined)
            return
        var hostIndex = home.computerModel.indexOfUuid(chosen.hostUuid)
        if (hostIndex < 0)
            return
        if (chosen.available) {
            home.openHost(hostIndex, chosen.hostUuid, chosen.hostName,
                          chosen.appId)
        } else if (chosen.wakeable) {
            home.computerModel.wakeComputer(hostIndex, true, chosen.appId)
            home.destination = "rigs"
            home.showDestination("rigs")
        } else {
            home.openHost(hostIndex, chosen.hostUuid, chosen.hostName,
                          chosen.appId)
        }
    }

    // --- Navigation ---
    function openHost(hostIndex, hostUuid, hostName, autoLaunchAppId)
    {
        var component = Qt.createComponent("AppView.qml")
        if (component.status !== Component.Ready) {
            console.log("Jochona: AppView load failed:", component.errorString())
            return
        }
        var view = component.createObject(stackView, {
                                              "computerIndex": hostIndex,
                                              "objectName": hostName,
                                              "showHiddenGames": true,
                                              "hostUuid": hostUuid,
                                              "hostDisplayName": hostName,
                                              "pendingAutoLaunchAppId": autoLaunchAppId !== undefined ? autoLaunchAppId : -1
                                          })
        stackView.push(view)
    }

    // Verification hook (main.qml uiShotNavigate): open the first rig's
    // host detail exactly like activating its card would.
    function shotOpenFirstHost()
    {
        if (home.computerModel.rowCount() === 0) {
            return
        }
        openHost(0, home.computerModel.uuidForIndex(0), home.computerModel.nameForIndex(0), -1)
    }

    function shotOpenFirstHostActions()
    {
        if (home.computerModel.rowCount() > 0)
            hostActionsSheet_forIndex(0)
    }

    function shotOpenFirstPairing()
    {
        if (home.computerModel.rowCount() > 0)
            openPairing(0)
    }

    function hostActionsSheet_forIndex(idx)
    {
        home.actionsHostIndex = idx
        hostActionsSheet.title = home.computerModel.nameForIndex(idx)
        hostActionsSheet.open()
    }

    function openAddRigFlow()
    {
        stackView.push("qrc:/gui/WelcomeView.qml", {"step": 1})
    }

    function openPairing(idx)
    {
        stackView.push("qrc:/gui/PairView.qml", {
                           "computerModel": home.computerModel,
                           "computerIndex": idx
                       })
    }

    function openRename(idx)
    {
        var component = Qt.createComponent("TextEntryView.qml")
        if (component.status !== Component.Ready) {
            console.warn("Jochona: text entry load failed:", component.errorString())
            return
        }
        var view = component.createObject(stackView, {
                                              "title": qsTr("Rename rig"),
                                              "prompt": qsTr("Choose the name shown in Jochona."),
                                              "initialText": home.computerModel.nameForIndex(idx),
                                              "submitLabel": qsTr("Rename")
                                          })
        view.accepted.connect(function(value) {
            home.computerModel.renameComputer(idx, value)
            stackView.pop()
        })
        view.cancelled.connect(function() { stackView.pop() })
        stackView.push(view)
    }

    function removeSheet_forIndex(idx)
    {
        removeSheet.targetIndex = idx
        removeSheet.open()
    }

    // --- Host actions (Quick Sheets) ---
    QuickSheet {
        id: hostActionsSheet
        title: ""

        Column {
            spacing: Tokens.gutterTight
            width: parent.width

            Repeater {
                model: ["apps", "wake", "test", "manage"]

                delegate: NavigableButton {
                    required property var modelData

                    text: modelData === "apps" ? qsTr("View apps")
                          : modelData === "wake" ? qsTr("Wake")
                          : modelData === "test" ? qsTr("Test connection")
                                                 : qsTr("Manage rig")
                    description: modelData === "wake"
                                 ? qsTr("Available on the same local network")
                                 : modelData === "test"
                                   ? qsTr("Check the paths required for streaming")
                                   : ""

                    onClicked: {
                        hostActionsSheet.close()
                        var idx = home.actionsHostIndex
                        if (idx < 0)
                            return
                        if (modelData === "apps") {
                            home.openHost(idx, home.computerModel.uuidForIndex(idx),
                                          home.computerModel.nameForIndex(idx))
                        } else if (modelData === "wake") {
                            home.computerModel.wakeComputer(idx)
                        } else if (modelData === "test") {
                            testConnectionDialog.returnFocusItem =
                                    rigsShelf.focusedItem !== null
                                    ? rigsShelf.focusedItem : resumeStage
                            testConnectionDialog.pairKey =
                                EffectiveSettings.clientDeviceId + "|"
                                + home.computerModel.uuidForIndex(idx)
                            home.computerModel.testConnectionForComputer(idx)
                            testConnectionDialog.open()
                        } else {
                            Qt.callLater(function() { manageHostPanel.open() })
                        }
                    }
                }
            }
        }
    }

    QuickSheet {
        id: manageHostPanel
        title: home.actionsHostIndex >= 0
               ? home.computerModel.nameForIndex(home.actionsHostIndex) : ""
        initialFocusItem: renameRigButton

        NavigableButton {
            id: renameRigButton
            text: qsTr("Rename rig")
            primary: true
            onClicked: {
                manageHostPanel.close()
                home.openRename(home.actionsHostIndex)
            }
        }

        NavigableButton {
            text: qsTr("Remove rig")
            destructive: true
            onClicked: {
                manageHostPanel.close()
                home.removeSheet_forIndex(home.actionsHostIndex)
            }
        }

        NavigableButton {
            text: qsTr("Cancel")
            onClicked: manageHostPanel.close()
        }
    }



    QuickSheet {
        id: removeSheet
        title: qsTr("Remove this rig?")
        initialFocusItem: keepButton

        property int targetIndex: -1
        property string targetUuid: ""

        Label {
            width: parent.width
            text: qsTr("Jochona will forget %1. You can add it again at any time.")
                  .arg(home.computerModel.nameForIndex(removeSheet.targetIndex))
            font.pixelSize: Tokens.tMeta
            color: Tokens.textSecondary
            wrapMode: Text.WordWrap
        }

        Row {
            spacing: Tokens.gutter

            NavigableButton {
                text: qsTr("Remove rig")
                destructive: true
                onClicked: removeSheet.accept()
            }
            NavigableButton {
                id: keepButton
                text: qsTr("Keep rig")
                primary: true
                onClicked: removeSheet.reject()
            }
        }

        onOpened: targetUuid = home.computerModel.uuidForIndex(targetIndex)
        onAccepted: {
            RecentApps.forgetHost(targetUuid)
            home.computerModel.deleteComputer(targetIndex)
        }
    }


    ErrorMessageDialog {
        id: errorDialog
    }

    ConnectionTestDialog {
        id: testConnectionDialog
        onRetryRequested: {
            if (home.actionsHostIndex >= 0)
                home.computerModel.testConnectionForComputer(
                            home.actionsHostIndex)
        }
    }
}
