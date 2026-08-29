/*
THESIS: Jochona is the private route back to play; it refuses both the console
rail/hero/shelf template and the dense telemetry dashboard.
OWN-WORLD: Midnight matte fields, enamel connection lines, nickel rulings, and
small milk-glass cues; one restrained cyan-violet signal, never stacked glow.
STORY: Resume leads. The interface proves Device → Rig → Game, A advances,
B retraces, and technical detail appears only when requested.
FIRST VIEWPORT: One Resume stage owns the frame; named route stops expose
Rigs, Library, Controllers, and Settings without icon-only persistent chrome.
FORM: Night Route, grounded direction 4, seed 1fa81352.
FINISH: unreviewed and undocumented is unfinished; this build ends with the
finish review, the verdict, and DESIGN.md
*/
// Adaptive shell: one stack and route topology, with distinct handheld,
// desktop, and ten-foot compositions.
import QtQuick
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3
import QtQuick.Window 2.2

import ComputerManager 1.0
import StreamingPreferences 1.0
import SystemProperties 1.0
import SdlGamepadKeyNavigation 1.0
import AutoUpdateChecker 1.0

import "style"

ApplicationWindow {
    id: window

    property bool pollingActive: false
    property bool updateDismissed: false

    // Set by SettingsView to force the back operation to pop all
    // pages except the initial view. This is required when doing
    // a retranslate() because AppView breaks for some reason.
    property bool clearOnBack: false

    width: 1280
    height: 720
    minimumWidth: 720
    minimumHeight: 540
    color: Tokens.night

    palette.window: Tokens.night
    palette.windowText: Tokens.textPrimary
    palette.base: Tokens.surface
    palette.alternateBase: Tokens.surfaceFocus
    palette.text: Tokens.textPrimary
    palette.button: Tokens.surface
    palette.buttonText: Tokens.textPrimary
    palette.highlight: Tokens.accentFocus
    palette.highlightedText: Tokens.focusInk
    palette.placeholderText: Tokens.textSecondary

    Binding {
        target: Tokens
        property: "viewportWidth"
        value: window.width
    }
    Binding {
        target: Tokens
        property: "viewportHeight"
        value: window.height
    }
    Binding {
        target: Tokens
        property: "uiScale"
        value: Tokens.scaleFor(window.width, window.height)
    }

    Connections {
        target: SdlGamepadKeyNavigation
        function onInputModeChanged() {
            Tokens.inputMode = SdlGamepadKeyNavigation.inputMode
        }
        function onControllerFamilyChanged() {
            Glyphs.family = SdlGamepadKeyNavigation.controllerFamily
        }

    }

    Rectangle {
        id: updateNotice
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Tokens.gutter
        width: Math.min(parent.width - Tokens.gutter * 2, Tokens.dp(520))
        height: updateRow.implicitHeight + Tokens.gutter
        radius: Tokens.radiusPanel
        color: Tokens.surfaceFocus
        border.color: Tokens.borderFocus
        border.width: Tokens.routeStroke
        visible: AutoUpdateChecker.status === "available"
                 && !window.updateDismissed
        z: 500

        RowLayout {
            id: updateRow
            anchors.fill: parent
            anchors.margins: Tokens.gutterTight
            spacing: Tokens.gutterTight
            Label {
                Layout.fillWidth: true
                text: qsTr("Jochona %1 is available.")
                      .arg(AutoUpdateChecker.availableVersion)
                color: Tokens.textPrimary
                font.family: Tokens.familyBody
                font.pixelSize: Tokens.tMeta
                wrapMode: Text.WordWrap
            }
            NavigableButton {
                compact: true
                text: qsTr("View")
                onClicked: Qt.openUrlExternally(
                               AutoUpdateChecker.availableUrl)
            }
            NavigableButton {
                compact: true
                text: qsTr("Later")
                onClicked: window.updateDismissed = true
            }
        }
    }

    // This function runs prior to creation of the initial StackView item
    function doEarlyInit() {
        // Jochona: SdlGamepadKeyNavigation.enable() is deferred to bootTimer below.
        // It initializes the SDL game-controller subsystem, which runs HID device
        // enumeration inside a nested CFRunLoop. Doing that here (before the
        // window's first frame) can stall the first CoreAnimation commit on
        // macOS and pinwheel the app at launch.
    }

    Component.onCompleted: {
        Tokens.inputMode = SdlGamepadKeyNavigation.inputMode
        Glyphs.family = SdlGamepadKeyNavigation.controllerFamily
        // Resume-route backing
        AutoUpdateChecker.start()
        RecentApps.setup(database)

        // Show the window according to the user's preferences
        if (SystemProperties.hasDesktopEnvironment) {
            if (StreamingPreferences.uiDisplayMode == StreamingPreferences.UI_MAXIMIZED) {
                window.showMaximized()
            }
            else if (StreamingPreferences.uiDisplayMode == StreamingPreferences.UI_FULLSCREEN) {
                window.showFullScreen()
            }
            else {
                window.show()
            }
        } else {
            window.showFullScreen()
        }

        // Display any modal dialogs for configuration warnings
        if (runConfigChecks) {
            if (SystemProperties.isWow64) {
                wow64Dialog.open()
            }

            SystemProperties.hasHardwareAccelerationChanged.connect(hasHardwareAccelerationChanged)
            SystemProperties.unmappedGamepadsChanged.connect(hasUnmappedGamepadsChanged)
        }

        // Gamepad navigation and the deferred subsystem probe run after the
        // first frame is presented (see comment in doEarlyInit). The probe
        // thread warms the SDL game-controller subsystem off-thread, so
        // enable() typically just takes the refcount shortcut by now.
        bootTimer.start()
    }

    function hasHardwareAccelerationChanged() {
        if (!SystemProperties.hasHardwareAcceleration && StreamingPreferences.videoDecoderSelection !== StreamingPreferences.VDS_FORCE_SOFTWARE) {
            if (SystemProperties.isRunningXWayland) {
                xWaylandDialog.open()
            }
            else {
                noHwDecoderDialog.open()
            }
        }
    }

    function hasUnmappedGamepadsChanged() {
        if (SystemProperties.unmappedGamepads) {
            unmappedGamepadDialog.unmappedGamepads = SystemProperties.unmappedGamepads
            unmappedGamepadDialog.open()
        }
    }

    Timer {
        id: bootTimer
        interval: 250
        repeat: true
        onTriggered: {
            // Wait for the off-thread gamepad probe (it owns the SDL GC
            // subsystem init); enable() is then a fast refcount no-op and the
            // UI thread never sits in HID enumeration.
            if (SystemProperties.gamepadProbeComplete) {
                stop()
                SdlGamepadKeyNavigation.enable()
                if (runConfigChecks)
                    SystemProperties.startAsyncLoad()
            }
        }
    }

    // This timer keeps us polling for 5 minutes of inactivity
    // to allow the user to work with Jochona on a second display
    // while dealing with configuration issues.
    Timer {
        id: inactivityTimer
        interval: 5 * 60000
        onTriggered: {
            if (!active && pollingActive) {
                ComputerManager.stopPollingAsync()
                pollingActive = false
            }
        }
    }

    onVisibleChanged: {
        // When we become invisible while streaming is going on,
        // stop polling immediately.
        if (!visible) {
            inactivityTimer.stop()

            if (pollingActive) {
                ComputerManager.stopPollingAsync()
                pollingActive = false
            }
        }
        else if (active) {
            // When we become visible and active again, start polling
            inactivityTimer.stop()

            // Restart polling if it was stopped
            if (!pollingActive) {
                ComputerManager.startPolling()
                pollingActive = true
            }
        }

        // Poll for gamepad input only when the window is in focus
        SdlGamepadKeyNavigation.notifyWindowFocus(visible && active)
    }

    onActiveChanged: {
        if (active) {
            // Stop the inactivity timer
            inactivityTimer.stop()

            // Restart polling if it was stopped
            if (!pollingActive) {
                ComputerManager.startPolling()
                pollingActive = true
            }
        }
        else {
            // Start the inactivity timer to stop polling
            // if focus does not return within a few minutes.
            inactivityTimer.restart()
        }

        // Poll for gamepad input only when the window is in focus
        SdlGamepadKeyNavigation.notifyWindowFocus(visible && active)
    }

    function goBack() {
        if (clearOnBack) {
            // Pop all items except the first one
            stackView.pop(null)
            clearOnBack = false
        }
        else {
            stackView.pop()
        }
    }

    // Verification hook (main.cpp: JOCHONA_UI_PUSH). Drives the same code
    // paths a click would, so any screen can be screenshotted headlessly.
    function uiShotNavigate(spec)
    {
        console.log("Jochona: uiShotNavigate", spec)
        shotNavTimer.spec = spec
        shotNavTimer.tries = 0
        shotNavTimer.restart()
    }

    Timer {
        id: shotNavTimer
        interval: 100
        repeat: true
        property string spec: ""
        property int tries: 0
        onTriggered: {
            var view = stackView.currentItem
            var complete = false

            if (spec === "hostdetail" && view && view.shotOpenFirstHost) {
                view.shotOpenFirstHost()
                complete = true
            } else if ((spec === "rigs" || spec === "library")
                       && view && view.showDestination) {
                routeBar.currentKey = spec
                view.showDestination(spec)
                complete = true
            } else if (spec === "sheet" && view
                       && view.shotOpenFirstHostActions) {
                view.shotOpenFirstHostActions()
                complete = true
            } else if (spec === "pairing" && view
                       && view.shotOpenFirstPairing) {
                view.shotOpenFirstPairing()
                complete = true
            } else if ((spec === "settings"
                        || spec === "appearance"
                        || spec === "diagnostics") && view) {
                if (spec === "appearance" && view.shotShowAppearance) {
                    complete = view.shotShowAppearance()
                } else if (spec === "diagnostics"
                           && view.shotShowDiagnostics) {
                    complete = view.shotShowDiagnostics()
                } else if (spec === "settings") {
                    navigateTo("qrc:/gui/SettingsView.qml", SettingsView)
                    complete = true
                } else {
                    navigateTo("qrc:/gui/SettingsView.qml", SettingsView)
                }
            } else if (spec === "sessionsettings" && view) {
                if (view.shotOpenPreview) {
                    complete = view.shotOpenPreview()
                } else {
                    stackView.push(
                        "qrc:/gui/session/SessionSettingsOverlay.qml")
                }
            } else if (spec === "controllers" && view) {
                stackView.push("qrc:/gui/controller/ControllerManagerView.qml")
                complete = true
            } else if (spec === "welcome" && view) {
                stackView.push("qrc:/gui/WelcomeView.qml")
                complete = true
            }

            if (complete)
                stop()
            else if (++tries > 30) {
                console.log("Jochona: uiShotNavigate gave up on", spec)
                stop()
            }
        }
    }

    function navigateTo(url, objectType)
    {
        var existingItem = stackView.find(function(item, index) {
            return item instanceof objectType
        })
        if (existingItem !== null)
            stackView.pop(existingItem)
        else
            stackView.push(url)
    }

    function showHomeDestination(destination)
    {
        stackView.pop(null)
        routeBar.currentKey = destination
        Qt.callLater(function() {
            var home = stackView.currentItem
            if (home && home.showDestination)
                home.showDestination(destination)
            else if (home && home.grabFirstFocus)
                home.grabFirstFocus()
        })
    }

    readonly property bool routeVisible: stackView.depth === 1
                                                 && stackView.currentItem instanceof HomeView

    // The screen stack owns the frame. Root Home leaves room for its named
    // route; focused tasks and pushed screens use the entire safe viewport.
    StackView {
        id: stackView
        anchors.fill: parent
        anchors.leftMargin: Tokens.safeInset
        anchors.rightMargin: Tokens.safeInset
        anchors.topMargin: Tokens.safeInset
        anchors.bottomMargin: window.routeVisible
                              ? Tokens.routeBarHeight + Tokens.safeInset
                                + Tokens.gutterTight
                              : Tokens.safeInset
        focus: true

        Behavior on anchors.bottomMargin {
            NumberAnimation {
                duration: Tokens.motion(Tokens.durationBase)
                easing.type: Easing.OutCubic
            }
        }

        pushEnter: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: Tokens.motion(Tokens.durationBase)
                }
                NumberAnimation {
                    property: "y"
                    from: Tokens.dp(18)
                    to: 0
                    duration: Tokens.motion(Tokens.durationBase)
                    easing.type: Easing.OutCubic
                }
            }
        }
        pushExit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: Tokens.motion(Tokens.durationFast)
            }
        }
        popEnter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: Tokens.motion(Tokens.durationBase)
            }
        }
        popExit: Transition {
            ParallelAnimation {
                NumberAnimation {
                    property: "opacity"
                    from: 1
                    to: 0
                    duration: Tokens.motion(Tokens.durationFast)
                }
                NumberAnimation {
                    property: "y"
                    from: 0
                    to: Tokens.dp(18)
                    duration: Tokens.motion(Tokens.durationFast)
                }
            }
        }

        Component.onCompleted: {
            doEarlyInit()
            push(initialView)

            if (String(initialView).indexOf("HomeView") !== -1) {
                var probe = Qt.createQmlObject(
                            'import ComputerModel 1.0; ComputerModel {}',
                            stackView, 'firstRunProbe')
                probe.initialize(ComputerManager)
                if (probe.rowCount() === 0)
                    push("qrc:/gui/WelcomeView.qml")
                probe.destroy()
            }
        }

        onCurrentItemChanged: {
            if (currentItem)
                currentItem.forceActiveFocus()
        }

        Keys.onEscapePressed: {
            if (depth > 1)
                goBack()
            else
                quitConfirmationDialog.open()
        }
        Keys.onBackPressed: {
            if (depth > 1)
                goBack()
            else
                quitConfirmationDialog.open()
        }
        Keys.onMenuPressed: routeBar.settingsRequested()
        Keys.onHangupPressed: routeBar.settingsRequested()
        Keys.onDownPressed: {
            if (window.routeVisible)
                routeBar.focusRoute(routeBar.currentKey)
        }
    }

    NavRail {
        id: routeBar
        z: 50
        visible: window.routeVisible
        opacity: visible ? 1.0 : 0.0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Tokens.safeInset
        anchors.rightMargin: Tokens.safeInset
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Tokens.safeInset

        Behavior on opacity {
            NumberAnimation { duration: Tokens.motion(Tokens.durationFast) }
        }

        onResumeRequested: window.showHomeDestination("resume")
        onRigsRequested: window.showHomeDestination("rigs")
        onLibraryRequested: window.showHomeDestination("library")
        onControllersRequested: {
            stackView.push("qrc:/gui/controller/ControllerManagerView.qml")
        }
        onSettingsRequested: navigateTo("qrc:/gui/SettingsView.qml", SettingsView)
        onEnterContentRequested: {
            if (stackView.currentItem && stackView.currentItem.grabFirstFocus)
                stackView.currentItem.grabFirstFocus()
        }
        onBackRequested: enterContentRequested()
    }

    // --- Window-level shortcuts (the deleted toolbar owned these) ---
    Shortcut {
        sequences: [StandardKey.New]
        onActivated: stackView.push("qrc:/gui/WelcomeView.qml", {"step": 1})
    }
    Shortcut {
        sequences: [StandardKey.Preferences]
        onActivated: navigateTo("qrc:/gui/SettingsView.qml", SettingsView)
    }
    Shortcut {
        sequences: [StandardKey.HelpContents]
        onActivated: compatibilityHelpDialog.open()
    }

    // --- Sheets & dialogs ---


    ErrorMessageDialog {
        id: noHwDecoderDialog
        text: qsTr("Jochona could not find a working hardware video decoder. "
                   + "Streaming may use much more power and may drop frames.")
        helpText: "\n\n" + qsTr("Open the upstream hardware-decoding guide?")
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Fixing-Hardware-Decoding-Problems"
    }

    ErrorMessageDialog {
        id: xWaylandDialog
        text: qsTr("Hardware decoding is unavailable through XWayland. "
                   + "Start Jochona with native Wayland, or switch to X11.")
        helpText: "\n\n" + qsTr("Open the upstream hardware-decoding guide?")
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Fixing-Hardware-Decoding-Problems"
    }

    NavigableMessageDialog {
        id: wow64Dialog
        standardButtons: Dialog.Ok
        okText: qsTr("Continue")
        text: qsTr("This Jochona build is not optimized for %1. Install a "
                   + "native %1 build from the same release channel for the "
                   + "best streaming performance.")
              .arg(SystemProperties.friendlyNativeArchName)
    }

    ErrorMessageDialog {
        id: unmappedGamepadDialog
        property string unmappedGamepads : ""
        text: qsTr("Jochona found controllers without mappings:")
              + "\n" + unmappedGamepads
        helpTextSeparator: "\n\n"
        helpText: qsTr("Open the upstream controller-mapping guide?")
        helpUrl: "https://github.com/moonlight-stream/moonlight-docs/wiki/Gamepad-Mapping"
    }

    // This dialog appears when quitting via keyboard or gamepad button
    NavigableMessageDialog {
        id: quitConfirmationDialog
        standardButtons: Dialog.Yes | Dialog.No
        yesText: qsTr("Quit Jochona")
        noText: qsTr("Keep playing")
        text: qsTr("Quit Jochona?")
        onAccepted: Qt.quit()
    }

    NavigableMessageDialog {
        id: compatibilityHelpDialog
        standardButtons: Dialog.Yes | Dialog.No
        yesText: qsTr("Open upstream guide")
        noText: qsTr("Stay in Jochona")
        text: qsTr("Jochona uses the Moonlight-compatible streaming protocol. "
                   + "The upstream setup guide documents compatible host setup.")
        onAccepted: Qt.openUrlExternally(
                        "https://github.com/moonlight-stream/moonlight-docs/wiki/Setup-Guide")
    }

}
