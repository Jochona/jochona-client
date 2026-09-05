// Stream launch and reconnect composition. SessionStatusOverlay owns every
// visible state; the native SDL window still owns live video.
import QtQuick 2.15
import QtQuick.Controls 2.2

import SdlGamepadKeyNavigation 1.0
import Session 1.0
import SystemProperties 1.0
import EffectiveSettings 1.0
import LibraryManager 1.0

import "session"
import "style"

Item {
    id: streamSegue

    property Session session
    property Session reconnectSource: null
    property string appName: ""
    property string hostName: qsTr("Rig")
    property bool isResume: false
    property bool quitAfter: false
    property string launchWarning: ""
    property bool displayReconnectPending: false
    property bool sessionSettingsOpen: false
    // Guards recordOutcome() so each Session object (one physical connection
    // attempt) writes at most one Local History outcome record, even though
    // failure can be observed from more than one signal path.
    property bool _outcomeRecorded: false

    onSessionChanged: streamSegue._outcomeRecorded = false

    // Writes a single redacted, structured Local History outcome record for
    // the current Session object. stage MUST be a short protocol phase name
    // or fixed literal -- never host-supplied or free-form text.
    function recordOutcome(success, stage, errorCode)
    {
        if (streamSegue._outcomeRecorded || !streamSegue.session
                || streamSegue.session.hostUuid.length === 0)
            return
        streamSegue._outcomeRecorded = true
        LibraryManager.recordSessionOutcome(streamSegue.session.hostUuid,
                                            streamSegue.session.appId,
                                            success, stage, errorCode)
    }

    function openSessionSettings() {
        if (!session)
            return
        sessionSettingsOpen = true
        SdlGamepadKeyNavigation.setStreamOverlayMode(true)
        SdlGamepadKeyNavigation.setUiNavMode(true)
        SdlGamepadKeyNavigation.enable()
        window.visible = true
        window.show()
        window.raise()
        window.requestActivate()
        sessionSettings.open()
    }

    function closeSessionSettings() {
        sessionSettings.visible = false
        sessionSettingsOpen = false
        SdlGamepadKeyNavigation.disable()
        SdlGamepadKeyNavigation.setStreamOverlayMode(false)
        SdlGamepadKeyNavigation.setUiNavMode(false)
        window.visible = false
        if (session)
            session.closeSessionSettings()
    }

    function applySessionSettings(patch, scope) {
        sessionSettings.visible = false
        sessionSettingsOpen = false
        SdlGamepadKeyNavigation.disable()
        SdlGamepadKeyNavigation.setStreamOverlayMode(false)
        SdlGamepadKeyNavigation.setUiNavMode(false)
        window.visible = false
        if (session)
            session.applySessionSettings(patch, scope)
    }

    objectName: isResume ? qsTr("Resume stream") : qsTr("Start stream")

    function beginCurrentSession()
    {
        launchWarning = ""
        SdlGamepadKeyNavigation.disable()

        if (!session || !session.initialize(window)) {
            statusOverlay.failureTitle = qsTr("Jochona could not prepare the stream")
            statusOverlay.failureDetail = qsTr(
                        "Check the rig and local decoder settings, then try again.")
            statusOverlay.errorCode = 0
            statusOverlay.state = "failed"
            window.visible = true
            SdlGamepadKeyNavigation.enable()
            streamSegue.recordOutcome(false, "initialize", 0)
            return
        }

        if (session.launchWarnings.length > 0) {
            var warnings = []
            for (var i = 0; i < session.launchWarnings.length; i++) {
                warnings.push(session.launchWarnings[i])
                console.warn(session.launchWarnings[i])
            }
            launchWarning = warnings.join("\n")
        }

        startSessionTimer.start()
    }

    function reconnect()
    {
        var source = reconnectSource !== null ? reconnectSource : session
        if (source === null) {
            statusOverlay.failureTitle = qsTr("Reconnect is no longer available")
            statusOverlay.failureDetail = qsTr("Return to the rig and start the game again.")
            statusOverlay.state = "failed"
            return
        }

        var replacement = source.createReconnectSession()
        reconnectSource = null
        session = replacement
        window.visible = true
        startupTimer.restart()
    }

    function connectionStarted()
    {
        window.visible = false
        launchWarning = ""
    }

    function sessionFinished(result)
    {
        if (sessionSettingsOpen) {
            sessionSettings.visible = false
            sessionSettingsOpen = false
            SdlGamepadKeyNavigation.disable()
            SdlGamepadKeyNavigation.setStreamOverlayMode(false)
            SdlGamepadKeyNavigation.setUiNavMode(false)
        }
        SdlGamepadKeyNavigation.enable()
        window.visible = true
        if (displayReconnectPending) {
            reconnectSource = session
            displayReconnectPending = false
            streamSegue.reconnect()
            return
        }

        if (statusOverlay.state === "failed"
                || statusOverlay.state === "reconnecting") {
            reconnectSource = session
            streamSegue.recordOutcome(
                        false,
                        statusOverlay.stageText.length > 0
                        ? statusOverlay.stageText : "connection_terminated",
                        statusOverlay.errorCode)
            return
        }

        streamSegue.recordOutcome(true, "", 0)

        if (!quitAfter) {
            stackView.pop()
        } else if (statusOverlay.state !== "failed") {
            Qt.quit()
        }
    }

    function quitStarting()
    {
        var component = Qt.createComponent("QuitSegue.qml")
        stackView.replace(stackView.currentItem,
                          component.createObject(stackView, {"appName": appName}),
                          StackView.Immediate)
        window.visible = true
    }

    function showLaunchError(text)
    {
        console.error(text)
        reconnectSource = session
        statusOverlay.failureTitle = qsTr("The stream could not start")
        statusOverlay.failureDetail = text
        statusOverlay.errorCode = 0
        statusOverlay.state = "failed"
        window.visible = true
        streamSegue.recordOutcome(false, "launch_error", 0)
    }

    StackView.onActivated: {
        SystemProperties.waitForAsyncLoad()
        startupTimer.start()
    }

    StackView.onDeactivating: SdlGamepadKeyNavigation.enable()

    Timer {
        id: startupTimer
        interval: 100
        onTriggered: streamSegue.beginCurrentSession()
    }

    Timer {
        id: startSessionTimer
        interval: 0
        onTriggered: {
            gc()
            session.start()
        }
    }

    Connections {
        target: streamSegue.session

        function onConnectionStarted() {
            streamSegue.connectionStarted()
        }
        function onStageFailed(stage, code, ports) {
            streamSegue.reconnectSource = streamSegue.session
        }
        function onConnectionTerminated(code, ports) {
            if (code !== 0)
                streamSegue.reconnectSource = streamSegue.session
        }
        function onDisplayLaunchError(text) {
            streamSegue.showLaunchError(text)
        }
        function onQuitStarting() {
            streamSegue.quitStarting()
        }
        function onSessionFinished(result) {
            streamSegue.sessionFinished(result)
        }
        function onDisplayReconnectRequested() {
            streamSegue.displayReconnectPending = true
            streamSegue.reconnectSource = streamSegue.session
        }
        function onReadyForDeletion() {
            if (statusOverlay.state !== "failed"
                    && statusOverlay.state !== "reconnecting") {
                streamSegue.session = null
                gc()
            }
        }
        function onSessionSettingsRequested() {
            streamSegue.openSessionSettings()
        }
    }

    Connections {
        target: SystemProperties
        function onDisplayTopologyChanged(previousContextId,
                                          currentContextId) {
            if (!streamSegue.session
                    || statusOverlay.state !== "connected")
                return
            var displayName = qsTr("another display")
            var contexts = SystemProperties.displayContexts
            for (var i = 0; i < contexts.length; ++i) {
                if (contexts[i].id === currentContextId) {
                    displayName = contexts[i].name
                    break
                }
            }
            streamSegue.session.notifyDisplayContextChanged(displayName)
        }
    }

    SessionStatusOverlay {
        id: statusOverlay
        session: streamSegue.session
        hostLabel: streamSegue.hostName
        destinationLabel: streamSegue.appName

        onReconnectRequested: streamSegue.reconnect()
        onQuitRequested: {
            reconnectSource = null
            session = null
            if (quitAfter)
                Qt.quit()
            else
                stackView.pop()
        }
        onDetailsRequested: {
            connectionDetails.retryAvailable = false
            connectionDetails.open()
            connectionDetails.connectionTestComplete(
                        statusOverlay.portTestResult,
                        statusOverlay.failingPorts)
        }
    }


    SessionSettingsOverlay {
        id: sessionSettings
        session: streamSegue.session
        onCloseRequested: streamSegue.closeSessionSettings()
        onApplyRequested: function(restartPatch, saveScope) {
            streamSegue.applySessionSettings(restartPatch, saveScope)
        }
    }
    ConnectionTestDialog {
        id: connectionDetails
        returnFocusItem: statusOverlay
        pairKey: streamSegue.session
                 ? EffectiveSettings.clientDeviceId + "|"
                   + streamSegue.session.hostUuid
                 : ""
    }

    Label {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Tokens.gutter
        visible: launchWarning.length > 0
        text: launchWarning
        font.family: Tokens.familyBody
        font.pixelSize: Tokens.tMicro
        color: Tokens.statusPairing
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
    }

    Label {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Tokens.gutter
        visible: launchWarning.length === 0
        text: SdlGamepadKeyNavigation.getConnectedGamepads() > 0
              ? qsTr("Disconnect shortcut: Start + Select + L1 + R1")
              : qsTr("Disconnect shortcut: Ctrl + Alt + Shift + Q")
        font.family: Tokens.familyBody
        font.pixelSize: Tokens.tMicro
        color: Tokens.textSecondary
    }
}
