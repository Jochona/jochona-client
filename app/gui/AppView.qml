// Night Route host and library surface. Host state and stream quality form the
// route header; applications are wide destinations, not a legacy poster grid.
import QtQuick 2.15
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import AppModel 1.0
import ComputerManager 1.0
import StreamingPreferences 1.0
import SdlGamepadKeyNavigation 1.0
import Negotiator 1.0
import LibraryManager 1.0
import EffectiveSettings 1.0

import "style"

pragma ComponentBehavior: Bound

GridView {
    id: appGrid

    property int computerIndex
    property AppModel appModel: createModel()
    property bool activated: false
    property bool showHiddenGames: false
    property bool showGames: false

    property string hostUuid: ""
    property string hostDisplayName: ""
    property int pendingAutoLaunchAppId: -1
    property var hostProbe: null
    property int hostRevision: 0
    property var hostInfo: {
        hostRevision
        return hostProbe === null ? ({})
                                  : hostProbe.hostInfoForIndex(computerIndex)
    }

    property var qualityEffective: ({})
    property var qualityOverride: ({})
    property var autoChip: null

    property int actionAppIndex: -1
    property int actionAppId: 0
    property string actionAppName: ""
    property bool actionAppRunning: false
    property bool actionAppDirectLaunch: false
    property int pendingFloorAppIndex: -1
    property int pendingFloorAppId: -1
    property string pendingFloorAppName: ""
    property bool pendingFloorQuitExisting: false

    function settingsContext(appId) {
        return {
            hostUuid: hostUuid,
            appId: appId,
            libraryEntryId: LibraryManager.libraryEntryFor(
                                hostUuid, appId)
        }
    }

    function floorConflictText(conflicts) {
        var messages = []
        var keys = Object.keys(conflicts)
        for (var i = 0; i < keys.length; ++i) {
            var conflict = conflicts[keys[i]]
            messages.push(qsTr("%1 requested %2; safety resolved %3.")
                          .arg(keys[i])
                          .arg(conflict.floor)
                          .arg(conflict.resolved))
        }
        return messages.join("\n")
    }
    property bool actionAppHidden: false

    readonly property bool loadingApps: hostProbe === null
                                                || hostInfo.statusUnknown === true
                                                || hostInfo.online === undefined
    readonly property int columnCount: Math.max(
                                           1,
                                           Math.floor((width - leftMargin - rightMargin)
                                                      / Tokens.dp(Tokens.handheld
                                                                  ? 268 : 320)))

    focus: true
    activeFocusOnTab: true
    model: appModel
    currentIndex: -1
    keyNavigationWraps: false
    boundsBehavior: Flickable.StopAtBounds
    leftMargin: Tokens.gutter / 2
    rightMargin: Tokens.gutter / 2
    topMargin: Tokens.gutter
    bottomMargin: Tokens.gutter
    cellWidth: Math.floor((width - leftMargin - rightMargin) / columnCount)
    cellHeight: Tokens.dp(178)

    function createModel()
    {
        var model = Qt.createQmlObject(
                    'import AppModel 1.0; AppModel {}', parent, '')
        model.initialize(ComputerManager, computerIndex, showHiddenGames)
        return model
    }

    function computerLost()
    {
        if (hostProbe === null || hostProbe.indexOfUuid(hostUuid) < 0)
            stackView.pop()
    }

    function refreshQuality()
    {
        qualityEffective = Negotiator.effectiveQualityFor(hostUuid)
        qualityOverride = Negotiator.qualityOverride(hostUuid)
    }

    function qualityActive(kind)
    {
        var override = qualityOverride
        if (kind === "auto")
            return hostUuid.length > 0 && Object.keys(override).length === 0
        if (Object.keys(override).length === 0)
            return false
        if (kind === "4k120")
            return override.width === 3840 && override.height === 2160
                    && override.fps === 120
        if (kind === "1080p60")
            return override.width === 1920 && override.height === 1080
                    && override.fps === 60
        return !(override.width === 3840 && override.height === 2160
                 && override.fps === 120)
                && !(override.width === 1920 && override.height === 1080
                     && override.fps === 60)
    }

    function qualitySummaryText()
    {
        if (hostUuid.length === 0 || qualityEffective.width === undefined)
            return ""
        var q = qualityEffective
        var resolution = qsTr("%1×%2 at %3 fps")
                         .arg(q.width).arg(q.height).arg(q.fps)
        var codec = String(q.codec).toLowerCase() === "auto"
                    ? qsTr("Automatic codec")
                    : String(q.codec).toUpperCase()
        var bitrate = qsTr("%1 Mbps")
                      .arg(Math.round(q.bitrateKbps / 100) / 10)
        return resolution + " · " + codec + " · " + bitrate
    }

    function qualityExplanationText()
    {
        var adjusted = qualityEffective.reasons !== undefined
                       && Object.keys(qualityEffective.reasons).length > 0
        var custom = Object.keys(qualityOverride).length > 0
        if (adjusted && custom)
            return qsTr("Jochona adjusted your custom profile to fit the "
                        + "current display, decoder, host, or network path.")
        if (adjusted)
            return qsTr("Jochona chose the closest safe profile for the "
                        + "current display, decoder, host, and network path.")
        if (custom)
            return qsTr("This rig is using your custom quality profile.")
        return qsTr("This rig follows your global streaming preferences.")
    }

    function recordLaunch(appId, appName)
    {
        if (hostUuid.length > 0) {
            RecentApps.record(hostUuid, hostDisplayName, appId, appName)
            LibraryManager.recordLaunch(hostUuid, appId)
        }
    }

    function launchApp(index, appId, appName, quitExistingApp,
                       bypassFloorPrompt)
    {
        if (!bypassFloorPrompt) {
            var resolved = EffectiveSettings.resolve(
                               settingsContext(appId))
            var conflicts = resolved.floorConflicts || ({})
            if (Object.keys(conflicts).length > 0) {
                pendingFloorAppIndex = index
                pendingFloorAppId = appId
                pendingFloorAppName = appName
                pendingFloorQuitExisting = quitExistingApp
                qualityFloorDialog.conflictText =
                    floorConflictText(conflicts)
                qualityFloorDialog.open()
                return
            }
        }
        var runningId = appModel.getRunningAppId()
        if (runningId !== 0 && runningId !== appId) {
            if (quitExistingApp) {
                quitAppSheet.appName = appModel.getRunningAppName()
                quitAppSheet.segueToStream = true
                quitAppSheet.nextAppName = appName
                quitAppSheet.nextAppIndex = index
                quitAppSheet.open()
            }
            return
        }

        var component = Qt.createComponent("StreamSegue.qml")
        recordLaunch(appId, appName)
        var segue = component.createObject(stackView, {
                                               "appName": appName,
                                               "hostName": appGrid.hostInfo.name
                                                           || appGrid.hostDisplayName,
                                               "session": appModel.createSessionForApp(index),
                                               "isResume": runningId === appId
                                           })
        stackView.push(segue)
    }

    function autoLaunchPending()
    {
        if (pendingAutoLaunchAppId < 0)
            return
        var appId = pendingAutoLaunchAppId
        pendingAutoLaunchAppId = -1
        var index = appModel.indexForAppId(appId)
        if (index < 0)
            return
        var runningId = appModel.getRunningAppId()
        if (runningId !== 0 && runningId !== appId)
            return
        launchApp(index, appId, appModel.nameForAppId(appId), false)
    }

    function focusFirstApp(event)
    {
        if (count > 0) {
            currentIndex = 0
            forceActiveFocus()
            if (event !== undefined)
                event.accepted = true
        }
    }

    function openAppActions(index, appId, appName, running, directLaunch, hidden)
    {
        actionAppIndex = index
        actionAppId = appId
        actionAppName = appName
        actionAppRunning = running
        actionAppDirectLaunch = directLaunch
        actionAppHidden = hidden
        appActionsPanel.title = appName
        appActionsPanel.open()
    }

    Component.onCompleted: {
        currentIndex = count > 0 ? 0 : -1
        refreshQuality()
        Negotiator.deviceProfileChanged.connect(refreshQuality)
        Negotiator.qualityOverridesChanged.connect(refreshQuality)

        var probe = Qt.createQmlObject(
                    'import ComputerModel 1.0; ComputerModel {}',
                    appGrid, 'hostDetailProbe')
        probe.initialize(ComputerManager)
        probe.dataChanged.connect(function() { hostRevision++ })
        probe.modelReset.connect(function() {
            hostRevision++
            computerLost()
        })
        hostProbe = probe

        if (pendingAutoLaunchAppId >= 0)
            Qt.callLater(autoLaunchPending)
    }

    StackView.onActivated: {
        appModel.computerLost.connect(computerLost)
        activated = true
        if (currentIndex < 0 && count > 0)
            currentIndex = 0
        forceActiveFocus()

        if (!showGames && !showHiddenGames) {
            var directLaunchAppIndex = model.getDirectLaunchAppIndex()
            if (directLaunchAppIndex >= 0) {
                currentIndex = directLaunchAppIndex
                currentItem.cardActivate()
                showGames = true
            }
        }
    }

    StackView.onDeactivating: {
        appModel.computerLost.disconnect(computerLost)
        activated = false
    }

    onCountChanged: {
        if (count === 0)
            currentIndex = -1
        else if (currentIndex < 0 || currentIndex >= count)
            currentIndex = 0
    }

    Keys.onUpPressed: function(event) {
        if (autoChip !== null && currentIndex < columnCount) {
            autoChip.forceActiveFocus()
            event.accepted = true
        }
    }
    Keys.onReturnPressed: {
        if (currentItem !== null)
            currentItem.cardActivate()
    }
    Keys.onEnterPressed: {
        if (currentItem !== null)
            currentItem.cardActivate()
    }
    Keys.onMenuPressed: {
        if (currentItem !== null)
            currentItem.pressHold()
    }

    header: Item {
        width: appGrid.width
        height: hostHeader.implicitHeight + Tokens.gutter

        Column {
            id: hostHeader
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: Tokens.gutter / 2
            anchors.rightMargin: Tokens.gutter / 2
            spacing: Tokens.gutterTight

            Row {
                spacing: Tokens.dp(12)

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Tokens.dp(11)
                    height: width
                    radius: width / 2
                    color: appGrid.hostInfo.statusUnknown
                           ? Tokens.statusUnknown
                           : appGrid.hostInfo.online
                             ? Tokens.statusOnline : Tokens.statusOffline
                }

                Label {
                    text: appGrid.hostInfo.name || appGrid.hostDisplayName
                    font.family: Tokens.familyDisplay
                    font.pixelSize: Tokens.tTitle
                    font.weight: Font.Medium
                    color: Tokens.textPrimary
                    Accessible.role: Accessible.Heading
                }
            }

            Label {
                width: parent.width
                text: (appGrid.hostInfo.summary || "")
                      + (appGrid.hostInfo.path ? " · " + appGrid.hostInfo.path : "")
                font.family: Tokens.familyBody
                font.pixelSize: Tokens.tMeta
                color: Tokens.textSecondary
                elide: Text.ElideRight
            }

            Label {
                text: qsTr("Stream quality")
                font.family: Tokens.familyDisplay
                font.pixelSize: Tokens.tShelf
                font.weight: Font.Medium
                color: Tokens.textPrimary
            }

            Flow {
                id: chipsFlow
                width: parent.width
                spacing: Tokens.gutterTight

                Repeater {
                    id: chipRepeater
                    model: ["auto", "4k120", "1080p60", "custom"]

                    delegate: NavigableButton {
                        id: qualityChip
                        required property var modelData
                        required property int index

                        compact: true
                        text: modelData === "auto" ? qsTr("Auto")
                              : modelData === "4k120" ? qsTr("4K 120")
                              : modelData === "1080p60" ? qsTr("1080p 60")
                                                       : qsTr("Custom…")
                        highlighted: appGrid.qualityActive(modelData)

                        Component.onCompleted: {
                            if (modelData === "auto")
                                appGrid.autoChip = qualityChip
                        }

                        onClicked: {
                            if (modelData === "custom") {
                                customQualityPanel.openFor(
                                    "host_client_pair", -1)
                            } else if (modelData === "auto") {
                                Negotiator.clearQualityOverride(appGrid.hostUuid)
                                appGrid.refreshQuality()
                            } else if (modelData === "4k120") {
                                Negotiator.setQualityOverride(
                                            appGrid.hostUuid,
                                            {width: 3840, height: 2160, fps: 120})
                                appGrid.refreshQuality()
                            } else {
                                Negotiator.setQualityOverride(
                                            appGrid.hostUuid,
                                            {width: 1920, height: 1080, fps: 60})
                                appGrid.refreshQuality()
                            }
                        }

                        Keys.onLeftPressed: {
                            var previous = chipRepeater.itemAt(index - 1)
                            if (previous)
                                previous.forceActiveFocus()
                        }
                        Keys.onRightPressed: {
                            var next = chipRepeater.itemAt(index + 1)
                            if (next)
                                next.forceActiveFocus()
                        }
                        Keys.onDownPressed: function(event) {
                            appGrid.focusFirstApp(event)
                        }
                    }
                }
            }

            RowLayout {
                width: parent.width
                spacing: Tokens.gutterTight

                Label {
                    Layout.fillWidth: true
                    text: appGrid.qualitySummaryText()
                    font.family: Tokens.familyBody
                    font.pixelSize: Tokens.tMeta
                    color: Tokens.link
                    elide: Text.ElideRight
                }

                NavigableButton {
                    compact: true
                    text: qsTr("Why this profile?")
                    onClicked: qualityWhyPanel.open()
                    Keys.onDownPressed: function(event) {
                        appGrid.focusFirstApp(event)
                    }
                }
            }

            Flow {
                width: parent.width
                spacing: Tokens.gutterTight

                NavigableButton {
                    visible: appGrid.hostInfo.wakeable === true
                             && appGrid.hostInfo.online === false
                    enabled: appGrid.hostInfo.wakeState !== "sending"
                             && appGrid.hostInfo.wakeState !== "sent"
                    text: appGrid.hostInfo.wakeState === "sending"
                          ? qsTr("Sending via %1…")
                                .arg(appGrid.hostInfo.wakeProvider)
                          : appGrid.hostInfo.wakeState === "sent"
                            ? qsTr("Sent via %1")
                                  .arg(appGrid.hostInfo.wakeProvider)
                          : appGrid.hostInfo.wakeState === "failed"
                            ? qsTr("Retry %1")
                                  .arg(appGrid.hostInfo.wakeProvider)
                            : qsTr("Wake via %1")
                                  .arg(appGrid.hostInfo.wakeProvider)
                    description: appGrid.hostInfo.wakeState === "failed"
                                 ? appGrid.hostInfo.wakeError
                                 : appGrid.hostInfo.wakeProvider === "Beacon"
                                   ? qsTr("Beacon accepts one request and owns "
                                          + "the LAN Wake burst")
                                   : qsTr("Direct Wake sends from this Client")
                    onClicked: appGrid.hostProbe.wakeComputer(
                                   appGrid.computerIndex, false, -1)
                }

                NavigableButton {
                    visible: appGrid.hostInfo.online === false
                             && appGrid.hostInfo.wakeState === "failed"
                             && appGrid.hostInfo.wakeProvider === "Beacon"
                             && appGrid.hostInfo.canDirectWake === true
                    text: qsTr("Try Direct Wake")
                    description: qsTr("Explicit fallback; the Beacon route remains selected")
                    onClicked: appGrid.hostProbe.wakeComputer(
                                   appGrid.computerIndex, false, -1, true)
                }

                NavigableButton {
                    text: qsTr("Wake settings")
                    onClicked: wakeSettingsSheet.open()
                }
            }
        }
    }

    delegate: AppTile {
        required property string name
        required property bool running
        required property url boxart
        required property bool hidden
        required property int appid
        required property bool directLaunch

        width: appGrid.cellWidth - Tokens.gutter
        height: Tokens.dp(158)
        titleText: name
        artUrl: boxart
        hostText: appGrid.hostInfo.name || appGrid.hostDisplayName
        selected: appGrid.activeFocus && appGrid.currentIndex === index
        navigationOwner: appGrid

        onCardActivate: appGrid.launchApp(index, appid, name, true)
        onPressHold: appGrid.openAppActions(index, appid, name, running,
                                            directLaunch, hidden)
    }

    Item {
        anchors.centerIn: parent
        width: Math.min(appGrid.width * 0.72, Tokens.dp(760))
        height: emptyColumn.implicitHeight
        visible: appGrid.count === 0

        Column {
            id: emptyColumn
            width: parent.width
            spacing: Tokens.gutterTight

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: appGrid.loadingApps
                running: visible
                width: Tokens.dp(42)
                height: width
            }

            Label {
                width: parent.width
                text: appGrid.loadingApps
                      ? qsTr("Reading this rig’s library…")
                      : appGrid.hostInfo.online === false
                        ? qsTr("This rig is offline. Wake it to read its library.")
                        : qsTr("No visible applications are available on this rig.")
                font.family: Tokens.familyBody
                font.pixelSize: Tokens.tMeta
                color: Tokens.textSecondary
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
        }
    }

    QuickSheet {
        id: qualityWhyPanel
        title: qsTr("Why this profile?")
        initialFocusItem: whyCloseButton

        Label {
            width: parent.width
            text: appGrid.qualitySummaryText()
            font.family: Tokens.familyDisplay
            font.pixelSize: Tokens.tShelf
            color: Tokens.textPrimary
            wrapMode: Text.WordWrap
        }

        Label {
            width: parent.width
            text: appGrid.qualityExplanationText()
            font.family: Tokens.familyBody
            font.pixelSize: Tokens.tMeta
            color: Tokens.textSecondary
            wrapMode: Text.WordWrap
        }

        NavigableButton {
            id: whyCloseButton
            text: qsTr("Done")
            primary: true
            onClicked: qualityWhyPanel.close()
        }
    }

    QuickSheet {
        id: wakeSettingsSheet
        title: qsTr("Wake provider")
        initialFocusItem: directWakeChoice

        property var route: ({})
        property int hostsRevision: 0

        function refreshRoute() {
            route = beaconManager.wakeRouteForHost(appGrid.hostUuid)
            appGrid.hostRevision++
        }

        onAboutToShow: {
            refreshRoute()
            var paired = beaconManager.pairedBeacons
            for (var i = 0; i < paired.length; ++i) {
                if (paired[i].identityState === "trusted")
                    beaconManager.refreshHosts(paired[i].id)
            }
        }

        Column {
            width: parent.width
            spacing: Tokens.gutter

            Label {
                width: parent.width
                text: qsTr("Choose one Wake Provider for this Host. "
                           + "A failed Beacon request never silently sends "
                           + "from this Client.")
                color: Tokens.textSecondary
                wrapMode: Text.WordWrap
            }

            NavigableButton {
                id: directWakeChoice
                width: parent.width
                text: wakeSettingsSheet.route.provider === "direct"
                      ? qsTr("Direct Wake — selected")
                      : qsTr("Use Direct Wake")
                description: qsTr("This Client sends Wake packets on its own network interfaces")
                onClicked: {
                    if (beaconManager.setDirectWake(appGrid.hostUuid))
                        wakeSettingsSheet.refreshRoute()
                }
            }

            Label {
                width: parent.width
                visible: beaconManager.pairedBeacons.length === 0
                text: qsTr("No Beacon is paired. Pair one in Settings → Beacon Wake.")
                color: Tokens.textSecondary
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: beaconManager.pairedBeacons
                delegate: Column {
                    id: beaconChoice
                    property var beacon: modelData
                    width: wakeSettingsSheet.width - 2 * Tokens.gutter
                    spacing: Tokens.gutterTight

                    RowLayout {
                        width: parent.width
                        Label {
                            Layout.fillWidth: true
                            text: beacon.name
                            font.weight: Font.Medium
                            color: beacon.identityState === "trusted"
                                   ? Tokens.textPrimary : Tokens.statusPairing
                            elide: Text.ElideRight
                        }
                        NavigableButton {
                            compact: true
                            text: qsTr("Refresh")
                            enabled: beacon.identityState === "trusted"
                            onClicked:
                                beaconManager.refreshHosts(beacon.id)
                        }
                    }

                    Label {
                        width: parent.width
                        visible: beacon.identityState !== "trusted"
                        text: qsTr("Identity changed — re-pair this Beacon")
                        color: Tokens.statusPairing
                        wrapMode: Text.WordWrap
                    }

                    Repeater {
                        model: {
                            wakeSettingsSheet.hostsRevision
                            return beaconManager.hostsForBeacon(
                                beaconChoice.beacon.id)
                        }
                        delegate: NavigableButton {
                            property var beaconHost: modelData
                            property string beaconHostId:
                                String(beaconHost.host_id !== undefined
                                       ? beaconHost.host_id
                                       : beaconHost.id !== undefined
                                         ? beaconHost.id : "")
                            width: beaconChoice.width
                            text: {
                                var name = beaconHost.name !== undefined
                                           ? beaconHost.name : beaconHostId
                                var selected =
                                    wakeSettingsSheet.route.provider === "beacon"
                                    && wakeSettingsSheet.route.beaconId
                                        === beaconChoice.beacon.id
                                    && wakeSettingsSheet.route.beaconHostId
                                        === beaconHostId
                                return selected
                                    ? qsTr("%1 — selected").arg(name)
                                    : qsTr("Wake %1 through this Beacon").arg(name)
                            }
                            description: beaconHost.ready === true
                                         ? qsTr("Host is ready now")
                                         : qsTr("Beacon will send the LAN Wake burst")
                            enabled: beaconChoice.beacon.identityState === "trusted"
                                     && beaconHostId.length > 0
                            onClicked: {
                                if (beaconManager.setBeaconWake(
                                        appGrid.hostUuid,
                                        beaconChoice.beacon.id,
                                        beaconHostId)) {
                                    wakeSettingsSheet.refreshRoute()
                                }
                            }
                        }
                    }
                }
            }

            NavigableButton {
                text: qsTr("Done")
                primary: true
                onClicked: wakeSettingsSheet.close()
            }
        }

        Connections {
            target: beaconManager
            function onHostsChanged(beaconId) {
                wakeSettingsSheet.hostsRevision++
            }
            function onHostRefreshFailed(beaconId, error) {
                wakeProviderError.text = error
            }
        }

        Label {
            id: wakeProviderError
            width: parent.width
            visible: text.length > 0
            color: Tokens.statusPairing
            wrapMode: Text.WordWrap
        }
    }

    QuickSheet {
        id: customQualityPanel
        title: targetAppId >= 0
               ? qsTr("Game quality patch") : qsTr("Pair quality patch")

        property string targetScope: "host_client_pair"
        property int targetAppId: -1
        property string targetLibraryEntryId: ""
        property int selWidth: 3840
        property int selHeight: 2160
        property int selFps: 120
        property int selBitrateKbps: 80000
        property string selCodec: "auto"
        property bool selVirtualDisplay: false
        property bool pinPatch: false
        property bool enforceFloor: false

        function contextKey() {
            if (targetScope === "library_entry")
                return targetLibraryEntryId
            if (targetScope === "host_application")
                return appGrid.hostUuid + "|" + targetAppId
            return EffectiveSettings.clientDeviceId + "|" + appGrid.hostUuid
        }
        function codecName(values) {
            if (values.codec !== undefined)
                return values.codec
            if (values.videocfg === StreamingPreferences.VCC_FORCE_H264)
                return "h264"
            if (values.videocfg === StreamingPreferences.VCC_FORCE_HEVC)
                return "hevc"
            if (values.videocfg === StreamingPreferences.VCC_FORCE_AV1)
                return "av1"
            return "auto"
        }
        function loadSelection() {
            var bundle = EffectiveSettings.patch(targetScope, contextKey())
            var values = bundle.values || ({})
            selWidth = values.width !== undefined ? values.width
                                                   : qualityEffective.width || 1920
            selHeight = values.height !== undefined ? values.height
                                                     : qualityEffective.height || 1080
            selFps = values.fps !== undefined ? values.fps
                                               : qualityEffective.fps || 60
            selBitrateKbps = values.bitrateKbps !== undefined
                             ? values.bitrateKbps
                             : values.bitrate !== undefined ? values.bitrate
                             : qualityEffective.bitrateKbps || 20000
            selCodec = codecName(values)
            selVirtualDisplay = values.virtualdisplay !== undefined
                                ? values.virtualdisplay === true
                                : qualityEffective.virtualdisplay === true
            var pins = bundle.pins || ({})
            pinPatch = pins.width === true || pins.fps === true
                       || pins.bitrateKbps === true
            var floors = bundle.floors || ({})
            enforceFloor = floors.fps !== undefined
                           || floors.bitrateKbps !== undefined
        }
        function openFor(scope, appId) {
            targetAppId = appId
            targetLibraryEntryId = appId >= 0
                ? LibraryManager.libraryEntryFor(appGrid.hostUuid, appId) : ""
            targetScope = scope
            open()
        }
        onAboutToShow: loadSelection()

        Column {
            width: parent.width
            spacing: Tokens.gutter

            Label {
                text: qsTr("Save to")
                font.family: Tokens.familyDisplay
                font.pixelSize: Tokens.tCard
                color: Tokens.textPrimary
            }
            AutoResizingComboBox {
                id: qualityScope
                width: parent.width
                textRole: "name"
                model: {
                    var choices = [{
                        name: qsTr("This Device ↔ Rig"),
                        scope: "host_client_pair"
                    }]
                    if (customQualityPanel.targetLibraryEntryId.length > 0)
                        choices.push({
                            name: qsTr("Library Entry"),
                            scope: "library_entry"
                        })
                    if (customQualityPanel.targetAppId >= 0)
                        choices.push({
                            name: qsTr("Host Application"),
                            scope: "host_application"
                        })
                    return choices
                }
                function syncScope() {
                    for (var i = 0; i < model.length; ++i) {
                        if (model[i].scope
                                === customQualityPanel.targetScope) {
                            currentIndex = i
                            return
                        }
                    }
                    currentIndex = 0
                }
                Component.onCompleted: syncScope()
                onModelChanged: syncScope()
                onActivated: {
                    customQualityPanel.targetScope =
                        model[currentIndex].scope
                    customQualityPanel.loadSelection()
                }
            }

            Label {
                text: qsTr("Resolution")
                font.family: Tokens.familyDisplay
                font.pixelSize: Tokens.tCard
                color: Tokens.textPrimary
            }
            Flow {
                width: parent.width
                spacing: Tokens.gutterTight
                Repeater {
                    model: [["1080p", 1920, 1080],
                            ["1440p", 2560, 1440],
                            ["4K", 3840, 2160]]
                    delegate: NavigableButton {
                        required property var modelData
                        compact: true
                        text: modelData[0]
                        highlighted: customQualityPanel.selWidth === modelData[1]
                        onClicked: {
                            customQualityPanel.selWidth = modelData[1]
                            customQualityPanel.selHeight = modelData[2]
                        }
                    }
                }
            }

            Label {
                text: qsTr("Frame rate")
                font.family: Tokens.familyDisplay
                font.pixelSize: Tokens.tCard
                color: Tokens.textPrimary
            }
            Flow {
                width: parent.width
                spacing: Tokens.gutterTight
                Repeater {
                    model: [30, 60, 120]
                    delegate: NavigableButton {
                        required property var modelData
                        compact: true
                        text: qsTr("%1 fps").arg(modelData)
                        highlighted: customQualityPanel.selFps === modelData
                        onClicked: customQualityPanel.selFps = modelData
                    }
                }
            }

            Label {
                text: qsTr("Bitrate")
                font.family: Tokens.familyDisplay
                font.pixelSize: Tokens.tCard
                color: Tokens.textPrimary
            }
            Flow {
                width: parent.width
                spacing: Tokens.gutterTight
                Repeater {
                    model: [20, 50, 80, 120]
                    delegate: NavigableButton {
                        required property var modelData
                        compact: true
                        text: qsTr("%1 Mbps").arg(modelData)
                        highlighted: customQualityPanel.selBitrateKbps
                                     === modelData * 1000
                        onClicked: customQualityPanel.selBitrateKbps
                                   = modelData * 1000
                    }
                }
            }

            Label {
                text: qsTr("Codec")
                font.family: Tokens.familyDisplay
                font.pixelSize: Tokens.tCard
                color: Tokens.textPrimary
            }
            Flow {
                width: parent.width
                spacing: Tokens.gutterTight
                Repeater {
                    model: ["auto", "h264", "hevc", "av1"]
                    delegate: NavigableButton {
                        required property var modelData
                        compact: true
                        text: modelData === "auto"
                              ? qsTr("Automatic") : modelData.toUpperCase()
                        highlighted: customQualityPanel.selCodec === modelData
                        onClicked: customQualityPanel.selCodec = modelData
                    }
                }
            }

            CheckBox {
                width: parent.width
                text: qsTr("Use Host virtual display")
                enabled: {
                    var caps = hostAdapters.capabilitiesFor(appGrid.hostUuid)
                    var names = caps.capabilities || []
                    return names.indexOf("virtualDisplayDriverReady") >= 0
                }
                checked: customQualityPanel.selVirtualDisplay
                onToggled: customQualityPanel.selVirtualDisplay = checked
            }

            CheckBox {
                width: parent.width
                text: qsTr("Pin this patch")
                checked: customQualityPanel.pinPatch
                onToggled: customQualityPanel.pinPatch = checked
            }
            CheckBox {
                width: parent.width
                text: qsTr("Use frame rate and bitrate as quality floors")
                checked: customQualityPanel.enforceFloor
                onToggled: customQualityPanel.enforceFloor = checked
            }
            Label {
                width: parent.width
                text: qsTr("Capability safety can still lower pinned values. "
                           + "Jochona asks before launch when safety crosses "
                           + "a saved quality floor.")
                color: Tokens.textSecondary
                wrapMode: Text.WordWrap
            }

            Row {
                spacing: Tokens.gutterTight

                NavigableButton {
                    text: qsTr("Save patch")
                    primary: true
                    onClicked: customQualityPanel.accept()
                }
                NavigableButton {
                    text: qsTr("Cancel")
                    onClicked: customQualityPanel.reject()
                }
            }
        }

        onAccepted: {
            var bundle = EffectiveSettings.patch(targetScope, contextKey())
            var values = bundle.values || ({})
            values.width = selWidth
            values.height = selHeight
            values.fps = selFps
            values.bitrateKbps = selBitrateKbps
            values.codec = selCodec
            values.virtualdisplay = selVirtualDisplay

            var pins = bundle.pins || ({})
            var keys = ["width", "height", "fps", "bitrateKbps", "codec",
                        "virtualdisplay"]
            for (var i = 0; i < keys.length; ++i) {
                if (pinPatch)
                    pins[keys[i]] = true
                else
                    delete pins[keys[i]]
            }
            var floors = bundle.floors || ({})
            if (enforceFloor) {
                floors.fps = selFps
                floors.bitrateKbps = selBitrateKbps
            } else {
                delete floors.fps
                delete floors.bitrateKbps
            }
            EffectiveSettings.setPatch(targetScope, contextKey(),
                                       values, pins, floors)
            appGrid.refreshQuality()
        }
    }

    NavigableMessageDialog {
        id: qualityFloorDialog
        property string conflictText: ""
        title: qsTr("Quality floor cannot be met")
        text: qsTr("Capability safety must lower this launch below a saved "
                   + "quality floor:\n\n%1\n\nLaunch with the safe values?")
              .arg(conflictText)
        standardButtons: Dialog.Yes | Dialog.No
        yesText: qsTr("Launch safely")
        noText: qsTr("Cancel")
        onAccepted: {
            appGrid.launchApp(appGrid.pendingFloorAppIndex,
                              appGrid.pendingFloorAppId,
                              appGrid.pendingFloorAppName,
                              appGrid.pendingFloorQuitExisting,
                              true)
        }
    }

    QuickSheet {
        id: appActionsPanel
        title: appGrid.actionAppName
        initialFocusItem: launchActionButton

        NavigableButton {
            id: launchActionButton
            text: appGrid.actionAppRunning ? qsTr("Resume") : qsTr("Play")
            primary: true
            onClicked: {
                appActionsPanel.close()
                appGrid.launchApp(appGrid.actionAppIndex, appGrid.actionAppId,
                                  appGrid.actionAppName, true)
            }
        }

        NavigableButton {
            visible: appGrid.actionAppRunning
            text: qsTr("Stop on rig")
            destructive: true
            onClicked: {
                appActionsPanel.close()
                quitAppSheet.appName = appGrid.actionAppName
                quitAppSheet.segueToStream = false
                quitAppSheet.open()
            }
        }

        NavigableButton {
            checkable: true
            checked: appGrid.actionAppDirectLaunch
            text: checked ? qsTr("Direct launch: on") : qsTr("Direct launch: off")
            description: qsTr("Open this app immediately after selecting the rig")
            onClicked: {
                appModel.setAppDirectLaunch(appGrid.actionAppIndex, checked)
                appActionsPanel.close()
            }
        }

        NavigableButton {
            checkable: true
            checked: appGrid.actionAppHidden
            text: checked ? qsTr("Hidden") : qsTr("Hide from library")
            enabled: checked || (!appGrid.actionAppRunning
                                 && !appGrid.actionAppDirectLaunch)
            onClicked: {
                appModel.setAppHidden(appGrid.actionAppIndex, checked)
                appActionsPanel.close()
            }
        }

        NavigableButton {
            text: qsTr("Game quality settings")
            description: qsTr("Save a sparse patch to the Library Entry or "
                              + "this Host Application")
            onClicked: {
                appActionsPanel.close()
                customQualityPanel.openFor(
                    "host_application", appGrid.actionAppId)
            }
        }

        NavigableButton {
            text: qsTr("Cancel")
            onClicked: appActionsPanel.close()
        }
    }

    QuickSheet {
        id: quitAppSheet
        title: qsTr("Stop %1?").arg(appName)
        initialFocusItem: keepRunningButton

        property string appName: ""
        property bool segueToStream: false
        property string nextAppName: ""
        property int nextAppIndex: 0

        Label {
            width: parent.width
            text: qsTr("Unsaved progress in the host application may be lost.")
            font.family: Tokens.familyBody
            font.pixelSize: Tokens.tMeta
            color: Tokens.textSecondary
            wrapMode: Text.WordWrap
        }

        NavigableButton {
            id: keepRunningButton
            text: qsTr("Keep running")
            primary: true
            onClicked: quitAppSheet.close()
        }

        NavigableButton {
            text: qsTr("Stop application")
            destructive: true
            onClicked: quitAppSheet.accept()
        }

        function quitApp() {
            var component = Qt.createComponent("QuitSegue.qml")
            var parameters = {
                "appName": appName,
                "quitRunningAppFn": function() { appModel.quitRunningApp() }
            }
            if (segueToStream) {
                parameters.nextAppName = nextAppName
                parameters.nextSession = appModel.createSessionForApp(nextAppIndex)
            } else {
                parameters.nextAppName = null
                parameters.nextSession = null
            }
            stackView.push(component.createObject(stackView, parameters))
        }

        onAccepted: quitApp()
    }

    ScrollBar.vertical: ScrollBar {}
}
