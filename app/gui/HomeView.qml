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
    property int actionsLibraryEntryIndex: -1
    property var recentEntries: []
    readonly property var actionsLibraryEntry: home.actionsLibraryEntryIndex >= 0
            && home.actionsLibraryEntryIndex < LibraryManager.entries.length
            ? LibraryManager.entries[home.actionsLibraryEntryIndex] : null

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
        } else if (destination === "library") {
            if (!libraryShelf.takeFocus())
                librarySearchAction.forceActiveFocus()
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
        } else if (destination === "library" && libraryShelf.count > 0
                && libraryShelf.currentIndex === libraryShelf.count - 1) {
            librarySearchAction.forceActiveFocus()
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

        RowLayout {
            visible: home.destination === "library"
            Layout.fillWidth: true
            Layout.preferredHeight: visible
                                    ? Math.max(libraryShelf.height,
                                               libraryActions.implicitHeight) : 0
            spacing: Tokens.gutter

            Shelf {
                id: libraryShelf
                visible: count > 0
                Layout.fillWidth: true
                Layout.preferredHeight: height
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
                    onPressHold: home.libraryEntryActionsSheet_forIndex(index)
                }
            }

            ColumnLayout {
                id: libraryActions
                Layout.alignment: Qt.AlignBottom
                spacing: Tokens.gutterTight

                NavigableButton {
                    id: librarySearchAction
                    compact: true
                    text: LibraryManager.search.length > 0
                          ? qsTr("Search: “%1”").arg(LibraryManager.search)
                          : qsTr("Search library")
                    onClicked: home.openLibrarySearch()
                    Keys.onLeftPressed: {
                        if (libraryShelf.count > 0)
                            libraryShelf.takeFocus(libraryShelf.count - 1)
                    }
                    Keys.onUpPressed: resumeStage.forceActiveFocus()
                }

                NavigableButton {
                    checkable: true
                    checked: LibraryManager.showHidden
                    compact: true
                    text: checked ? qsTr("Showing hidden") : qsTr("Show hidden")
                    onClicked: LibraryManager.setShowHidden(checked)
                    Keys.onLeftPressed: {
                        if (libraryShelf.count > 0)
                            libraryShelf.takeFocus(libraryShelf.count - 1)
                    }
                    Keys.onUpPressed: resumeStage.forceActiveFocus()
                }
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

    function openLibrarySearch()
    {
        var component = Qt.createComponent("TextEntryView.qml")
        if (component.status !== Component.Ready) {
            console.warn("Jochona: text entry load failed:", component.errorString())
            return
        }
        var view = component.createObject(stackView, {
                                              "title": qsTr("Search library"),
                                              "prompt": qsTr("Filter titles across every paired rig."),
                                              "initialText": LibraryManager.search,
                                              "placeholderText": qsTr("Game title"),
                                              "allowEmpty": true,
                                              "submitLabel": qsTr("Search")
                                          })
        view.accepted.connect(function(value) {
            LibraryManager.setSearch(value)
            stackView.pop()
        })
        view.cancelled.connect(function() { stackView.pop() })
        stackView.push(view)
    }

    function libraryEntryActionsSheet_forIndex(idx)
    {
        home.actionsLibraryEntryIndex = idx
        if (home.actionsLibraryEntry === null)
            return
        libraryEntryActionsSheet.title = home.actionsLibraryEntry.title
        libraryEntryActionsSheet.open()
    }

    function openLibraryGroupingPanel()
    {
        if (home.actionsLibraryEntry === null)
            return
        libraryGroupingPanel.title = home.actionsLibraryEntry.title
        libraryGroupingPanel.open()
    }

    function openMergePicker()
    {
        libraryMergePickerSheet.open()
    }

    function confirmMergeLibraryEntry(targetEntryId, targetTitle)
    {
        libraryMergeConfirmSheet.targetEntryId = targetEntryId
        libraryMergeConfirmSheet.targetTitle = targetTitle
        libraryMergeConfirmSheet.open()
    }

    function confirmSplitHostApplication(hostAppId, label)
    {
        librarySplitConfirmSheet.hostAppId = hostAppId
        librarySplitConfirmSheet.label = label
        librarySplitConfirmSheet.open()
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

    // Push a full-screen text entry for one wake override field, matching
    // openRename's controller-driven text-entry pattern. Reopens the wake
    // overrides panel afterward so the other fields stay reachable.
    function openWakeOverrideField(idx, field, title, prompt, initialValue)
    {
        wakeOverridesPanel.close()
        var component = Qt.createComponent("TextEntryView.qml")
        if (component.status !== Component.Ready) {
            console.warn("Jochona: text entry load failed:", component.errorString())
            return
        }
        var view = component.createObject(stackView, {
                                              "title": title,
                                              "prompt": prompt,
                                              "initialText": initialValue,
                                              "submitLabel": qsTr("Save"),
                                              "allowEmpty": true
                                          })
        function reopenPanel() {
            home.actionsHostIndex = idx
            Qt.callLater(function() { wakeOverridesPanel.open() })
        }
        view.accepted.connect(function(value) {
            var info = home.computerModel.hostInfoForIndex(idx)
            var mac = field === "mac" ? value : (info.manualMac || "")
            var port = field === "port" ? (parseInt(value, 10) || 0)
                                        : (info.wakePort || 0)
            var broadcast = field === "broadcast" ? value : (info.wakeBroadcast || "")
            home.computerModel.setWakeOverrides(idx, mac, port, broadcast)
            stackView.pop()
            reopenPanel()
        })
        view.cancelled.connect(function() {
            stackView.pop()
            reopenPanel()
        })
        stackView.push(view)
    }

    function openWakeOverridesPanel_forIndex(idx)
    {
        home.actionsHostIndex = idx
        wakeOverridesPanel.open()
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
                                 ? (home.actionsHostIndex >= 0
                                    && home.computerModel.hostInfoForIndex(
                                           home.actionsHostIndex).wakeProvider === "Beacon"
                                    ? qsTr("Beacon accepts one request and owns "
                                           + "the LAN Wake burst")
                                    : qsTr("Available on the same local network"))
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
            text: qsTr("Wake overrides")
            description: qsTr("Manual MAC, port, broadcast, and re-probe")
            onClicked: {
                manageHostPanel.close()
                Qt.callLater(function() {
                    home.openWakeOverridesPanel_forIndex(home.actionsHostIndex)
                })
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
        id: wakeOverridesPanel
        title: qsTr("Wake overrides")
        initialFocusItem: editMacButton

        property var wakeInfo: ({})

        function refresh() {
            wakeInfo = home.actionsHostIndex >= 0
                       ? home.computerModel.hostInfoForIndex(home.actionsHostIndex)
                       : ({})
        }

        onAboutToShow: refresh()

        Label {
            width: parent.width
            text: qsTr("Manual overrides beat the auto-detected wake details — "
                       + "the escape hatch for a Tailscale-cached MAC address. "
                       + "Leave a field blank to go back to automatic.")
            font.pixelSize: Tokens.tMeta
            color: Tokens.textSecondary
            wrapMode: Text.WordWrap
        }

        NavigableButton {
            id: editMacButton
            width: parent.width
            text: qsTr("MAC address")
            description: wakeOverridesPanel.wakeInfo.manualMac
                         ? wakeOverridesPanel.wakeInfo.manualMac
                         : qsTr("Auto-detected")
            onClicked: home.openWakeOverrideField(
                           home.actionsHostIndex, "mac",
                           qsTr("MAC address override"),
                           qsTr("Enter the rig's MAC address, or leave blank to auto-detect."),
                           wakeOverridesPanel.wakeInfo.manualMac || "")
        }

        NavigableButton {
            width: parent.width
            text: qsTr("Wake-on-LAN port")
            description: wakeOverridesPanel.wakeInfo.wakePort
                         ? String(wakeOverridesPanel.wakeInfo.wakePort)
                         : qsTr("Automatic")
            onClicked: home.openWakeOverrideField(
                           home.actionsHostIndex, "port",
                           qsTr("Wake port override"),
                           qsTr("Enter the UDP port for wake packets, or leave blank for automatic."),
                           wakeOverridesPanel.wakeInfo.wakePort
                           ? String(wakeOverridesPanel.wakeInfo.wakePort) : "")
        }

        NavigableButton {
            width: parent.width
            text: qsTr("Broadcast address")
            description: wakeOverridesPanel.wakeInfo.wakeBroadcast
                         ? wakeOverridesPanel.wakeInfo.wakeBroadcast
                         : qsTr("All network interfaces")
            onClicked: home.openWakeOverrideField(
                           home.actionsHostIndex, "broadcast",
                           qsTr("Broadcast address override"),
                           qsTr("Enter a broadcast address, or leave blank to sweep every interface."),
                           wakeOverridesPanel.wakeInfo.wakeBroadcast || "")
        }

        NavigableButton {
            width: parent.width
            text: qsTr("Re-probe from LAN")
            description: qsTr("Skip the wait and check this rig right now")
            onClicked: {
                if (home.actionsHostIndex >= 0)
                    home.computerModel.reprobeComputer(home.actionsHostIndex)
                wakeOverridesPanel.close()
            }
        }

        NavigableButton {
            text: qsTr("Done")
            primary: true
            onClicked: wakeOverridesPanel.close()
        }
    }

    QuickSheet {
        id: libraryEntryActionsSheet
        title: ""
        initialFocusItem: libraryFavoriteAction

        NavigableButton {
            id: libraryFavoriteAction
            checkable: true
            checked: home.actionsLibraryEntry !== null
                     ? home.actionsLibraryEntry.favorite : false
            text: checked ? qsTr("Favorited") : qsTr("Add to favorites")
            onClicked: {
                if (home.actionsLibraryEntry !== null)
                    LibraryManager.setFavorite(home.actionsLibraryEntry.id, checked)
                libraryEntryActionsSheet.close()
            }
        }

        NavigableButton {
            checkable: true
            checked: home.actionsLibraryEntry !== null
                     ? home.actionsLibraryEntry.hidden : false
            text: checked ? qsTr("Hidden") : qsTr("Hide from library")
            onClicked: {
                if (home.actionsLibraryEntry !== null)
                    LibraryManager.setHidden(home.actionsLibraryEntry.id, checked)
                libraryEntryActionsSheet.close()
            }
        }

        NavigableButton {
            text: qsTr("Manage grouping")
            description: qsTr("Split a rig out, or merge with another entry")
            onClicked: {
                libraryEntryActionsSheet.close()
                Qt.callLater(function() { home.openLibraryGroupingPanel() })
            }
        }

        NavigableButton {
            text: qsTr("Cancel")
            onClicked: libraryEntryActionsSheet.close()
        }
    }

    QuickSheet {
        id: libraryGroupingPanel
        title: ""

        property var candidates: home.actionsLibraryEntry !== null
                                 ? LibraryManager.hostCandidates(
                                       home.actionsLibraryEntry.id) : []

        onAboutToShow: candidates = home.actionsLibraryEntry !== null
                                    ? LibraryManager.hostCandidates(
                                          home.actionsLibraryEntry.id) : []

        Label {
            width: parent.width
            text: qsTr("Jochona grouped these into one Library Entry. Split "
                       + "one back out, or merge this entry into another.")
            font.pixelSize: Tokens.tMeta
            color: Tokens.textSecondary
            wrapMode: Text.WordWrap
        }

        Repeater {
            model: libraryGroupingPanel.candidates

            delegate: NavigableButton {
                id: groupingCandidateButton
                required property var modelData
                width: parent.width
                enabled: libraryGroupingPanel.candidates.length > 1
                text: modelData.hostName + " — " + modelData.appName
                description: groupingCandidateButton.enabled
                             ? qsTr("Split out into its own Library Entry")
                             : qsTr("The only rig in this entry")
                onClicked: {
                    libraryGroupingPanel.close()
                    Qt.callLater(function() {
                        home.confirmSplitHostApplication(
                                    groupingCandidateButton.modelData.hostAppId,
                                    groupingCandidateButton.modelData.hostName
                                    + " — " + groupingCandidateButton.modelData.appName)
                    })
                }
            }
        }

        NavigableButton {
            width: parent.width
            text: qsTr("Merge with another entry…")
            onClicked: {
                libraryGroupingPanel.close()
                Qt.callLater(function() { home.openMergePicker() })
            }
        }

        NavigableButton {
            text: qsTr("Done")
            primary: true
            onClicked: libraryGroupingPanel.close()
        }
    }

    QuickSheet {
        id: libraryMergePickerSheet
        title: qsTr("Merge into…")

        Repeater {
            model: home.actionsLibraryEntry !== null
                   ? LibraryManager.entries.filter(function(entry) {
                         return entry.id !== home.actionsLibraryEntry.id
                     })
                   : []

            delegate: NavigableButton {
                required property var modelData
                width: parent.width
                text: modelData.title
                onClicked: {
                    libraryMergePickerSheet.close()
                    Qt.callLater(function() {
                        home.confirmMergeLibraryEntry(modelData.id, modelData.title)
                    })
                }
            }
        }

        NavigableButton {
            text: qsTr("Cancel")
            onClicked: libraryMergePickerSheet.close()
        }
    }

    QuickSheet {
        id: libraryMergeConfirmSheet
        title: qsTr("Merge entries?")
        initialFocusItem: keepSeparateButton

        property string targetEntryId: ""
        property string targetTitle: ""

        Label {
            width: parent.width
            text: home.actionsLibraryEntry !== null
                  ? qsTr("Merge “%1” into “%2”? Their apps will appear "
                        + "together across every rig; you can split any of "
                        + "them back out later.")
                        .arg(home.actionsLibraryEntry.title)
                        .arg(libraryMergeConfirmSheet.targetTitle)
                  : ""
            font.pixelSize: Tokens.tMeta
            color: Tokens.textSecondary
            wrapMode: Text.WordWrap
        }

        Row {
            spacing: Tokens.gutter

            NavigableButton {
                text: qsTr("Merge")
                primary: true
                onClicked: libraryMergeConfirmSheet.accept()
            }
            NavigableButton {
                id: keepSeparateButton
                text: qsTr("Keep separate")
                onClicked: libraryMergeConfirmSheet.reject()
            }
        }

        onAccepted: {
            if (home.actionsLibraryEntry !== null) {
                LibraryManager.mergeEntries(targetEntryId,
                                           [home.actionsLibraryEntry.id])
            }
        }
    }

    QuickSheet {
        id: librarySplitConfirmSheet
        title: qsTr("Split out?")
        initialFocusItem: keepGroupedButton

        property var hostAppId: 0
        property string label: ""

        Label {
            width: parent.width
            text: qsTr("Split “%1” into its own Library Entry? It stops "
                       + "sharing favorites, categories, and Library-level "
                       + "settings with the rest of this group.")
                  .arg(librarySplitConfirmSheet.label)
            font.pixelSize: Tokens.tMeta
            color: Tokens.textSecondary
            wrapMode: Text.WordWrap
        }

        Row {
            spacing: Tokens.gutter

            NavigableButton {
                text: qsTr("Split")
                primary: true
                onClicked: librarySplitConfirmSheet.accept()
            }
            NavigableButton {
                id: keepGroupedButton
                text: qsTr("Keep grouped")
                onClicked: librarySplitConfirmSheet.reject()
            }
        }

        onAccepted: LibraryManager.splitHostApplication(hostAppId)
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
