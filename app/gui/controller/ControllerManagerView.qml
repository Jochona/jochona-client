// Night Route Controller Manager: stable identity, Player Slot Order, live
// input, Controller Map calibration/remapping, and Kenney CC0 silhouettes.
import QtQuick 2.15
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import ControllerManager 1.0
import ControllerMapStore 1.0

import ".."
import "../style"

Item {
    id: root
    objectName: qsTr("Controllers")

    property var liveState: ({})
    property var selectedMap: ({})
    property real leftDeadzone: 0.10
    property real rightDeadzone: 0.10
    property real leftCurve: 1.0
    property real rightCurve: 1.0
    property string pendingSourceButton: "a"
    property string pendingTargetButton: "b"

    function silhouette(family) {
        if (family === "playstation")
            return "qrc:/res/controllers/playstation.svg"
        if (family === "switch")
            return "qrc:/res/controllers/switch.svg"
        if (family === "steam")
            return "qrc:/res/controllers/steam.svg"
        return "qrc:/res/controllers/xbox.svg"
    }
    function familyLabel(family) {
        if (family === "playstation")
            return qsTr("PlayStation layout")
        if (family === "switch")
            return qsTr("Nintendo layout")
        if (family === "steam")
            return qsTr("Steam layout")
        if (family === "xbox")
            return qsTr("Xbox layout")
        return qsTr("Generic SDL layout")
    }
    function reloadControllerMap() {
        if (selectedEntry === null) {
            selectedMap = ({})
            return
        }
        selectedMap = ControllerMapStore.mapFor(selectedEntry.path)
        var calibration = selectedMap.calibration || ({})
        leftDeadzone = calibration.deadzoneLeftStick !== undefined
                       ? calibration.deadzoneLeftStick : 0.10
        rightDeadzone = calibration.deadzoneRightStick !== undefined
                        ? calibration.deadzoneRightStick : 0.10
        leftCurve = calibration.curveLeftStick !== undefined
                    ? calibration.curveLeftStick : 1.0
        rightCurve = calibration.curveRightStick !== undefined
                     ? calibration.curveRightStick : 1.0
    }
    function saveCalibration() {
        if (selectedEntry === null)
            return
        ControllerMapStore.saveMap(selectedEntry.path, "controller", "", {
            calibration: {
                deadzoneLeftStick: leftDeadzone,
                deadzoneRightStick: rightDeadzone,
                curveLeftStick: leftCurve,
                curveRightStick: rightCurve
            },
            buttonRemap: selectedMap.buttonRemap || ({})
        })
        reloadControllerMap()
    }
    function commitRemap(swapConflict) {
        if (selectedEntry === null)
            return
        var remap = ({})
        var stored = selectedMap.buttonRemap || ({})
        for (var key in stored)
            remap[key] = stored[key]
        if (swapConflict) {
            var previousTarget = remap[pendingSourceButton]
                                 || pendingSourceButton
            var conflictingSource = pendingTargetButton
            for (var source in remap) {
                if (source !== pendingSourceButton
                        && remap[source] === pendingTargetButton) {
                    conflictingSource = source
                    break
                }
            }
            remap[conflictingSource] = previousTarget
        }
        remap[pendingSourceButton] = pendingTargetButton
        ControllerMapStore.saveMap(selectedEntry.path, "controller", "", {
            calibration: selectedMap.calibration || ({}),
            buttonRemap: remap
        })
        reloadControllerMap()
    }
    function requestRemap(source, target) {
        pendingSourceButton = source
        pendingTargetButton = target
        if (source === target) {
            commitRemap(false)
            return
        }
        var remap = selectedMap.buttonRemap || ({})
        var conflict = target
        for (var candidate in remap) {
            if (candidate !== source && remap[candidate] === target) {
                conflict = candidate
                break
            }
        }
        remapConflict.conflictingSource = conflict
        remapConflict.open()
    }
    readonly property var selectedEntry:
        controllerList.currentIndex >= 0
        && controllerList.currentIndex < ControllerManager.controllers.length
        ? ControllerManager.controllers[controllerList.currentIndex] : null
    readonly property var selectedLive:
        controllerList.currentIndex >= 0
        && liveState[controllerList.currentIndex] !== undefined
        ? liveState[controllerList.currentIndex]
        : ({buttons: ({}), axes: ({})})

    function familyGlyph(family) {
        var glyph = Glyphs.glyph(family, "controller")
        return glyph.length > 0 ? glyph : Glyphs.glyph(family, "a")
    }

    function statusText(connected) {
        return connected ? qsTr("Connected") : qsTr("Disconnected")
    }

    Component.onCompleted: {
        ControllerManager.start()
        reloadControllerMap()
    }
    Component.onDestruction: ControllerManager.stop()
    StackView.onActivated: controllerList.forceActiveFocus()
    onSelectedEntryChanged: reloadControllerMap()

    Connections {
        target: ControllerMapStore
        function onMapChanged(controllerId, scope, contextKey) {
            if (root.selectedEntry
                    && root.selectedEntry.path === controllerId)
                root.reloadControllerMap()
        }
    }

    Connections {
        target: ControllerManager

        function onControllerLiveUpdate(deviceIndex) {
            var next = ({})
            for (var key in root.liveState)
                next[key] = root.liveState[key]
            next[deviceIndex] = ControllerManager.controllerSnapshot(deviceIndex)
            root.liveState = next
        }

        function onControllersChanged() {
            root.liveState = ({})
            controllerList.currentIndex =
                ControllerManager.controllers.length > 0 ? 0 : -1
            root.reloadControllerMap()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Tokens.gutter

        Label {
            Layout.fillWidth: true
            text: qsTr("Controllers")
            font.family: Tokens.familyDisplay
            font.pixelSize: Tokens.tTitle
            font.weight: Font.Medium
            color: Tokens.textPrimary
            Accessible.role: Accessible.Heading
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: Tokens.handheld || width < Tokens.dp(1100) ? 1 : 2
            columnSpacing: Tokens.gutter
            rowSpacing: Tokens.gutter

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: Tokens.dp(260)
                radius: Tokens.radiusPanel
                color: Tokens.surface
                border.width: Tokens.routeStroke
                border.color: controllerList.activeFocus
                              ? Tokens.borderFocus : Tokens.border

                ListView {
                    id: controllerList
                    anchors.fill: parent
                    anchors.margins: Tokens.gutterTight
                    model: ControllerManager.controllers
                    currentIndex: count > 0 ? 0 : -1
                    focus: true
                    activeFocusOnTab: true
                    clip: true
                    spacing: Tokens.gutterTight
                    boundsBehavior: Flickable.StopAtBounds

                    Keys.onReturnPressed: function(event) {
                        // The selected row is already mirrored in the detail
                        // pane; Return deliberately has no hidden action.
                        event.accepted = true
                    }

                    delegate: Item {
                        id: controllerRow
                        required property var modelData
                        required property int index

                        width: controllerList.width
                        height: Tokens.rowHeight
                        readonly property bool selected:
                            controllerList.currentIndex === index
                        readonly property var live:
                            root.liveState[index] || {buttons: ({}), axes: ({})}

                        Accessible.role: Accessible.ListItem
                        Accessible.name: modelData.name
                        Accessible.description: root.statusText(modelData.connected)
                                                + ", "
                                                + qsTr("Slot %1").arg(modelData.slot + 1)

                        Rectangle {
                            anchors.fill: parent
                            radius: Tokens.radiusControl
                            color: controllerRow.selected
                                   ? Tokens.surfaceFocus : "transparent"
                            border.width: controllerRow.selected
                                          ? Tokens.focusStroke : 0
                            border.color: Tokens.borderFocus
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Tokens.gutterTight
                            spacing: Tokens.gutterTight

                            Rectangle {
                                Layout.preferredWidth: Tokens.dp(46)
                                Layout.preferredHeight: width
                                Layout.alignment: Qt.AlignVCenter
                                radius: Tokens.radiusControl
                                color: Tokens.surface
                                border.width: Tokens.routeStroke
                                border.color: Tokens.border

                                Text {
                                    anchors.centerIn: parent
                                    text: root.familyGlyph(modelData.family)
                                    font.family: Glyphs.fontFamily(modelData.family)
                                    font.pixelSize: Tokens.dp(24)
                                    color: Tokens.textPrimary
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                spacing: Tokens.dp(3)

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    font.family: Tokens.familyDisplay
                                    font.pixelSize: Tokens.tCard
                                    font.weight: Font.Medium
                                    color: Tokens.textPrimary
                                    elide: Text.ElideRight
                                }

                                Row {
                                    spacing: Tokens.dp(8)

                                    Rectangle {
                                        width: Tokens.dp(8)
                                        height: width
                                        radius: width / 2
                                        color: modelData.connected
                                               ? Tokens.statusOnline
                                               : Tokens.statusOffline
                                        anchors.verticalCenter: parent.verticalCenter
                                    }

                                    Label {
                                        text: root.statusText(modelData.connected)
                                              + " · "
                                              + qsTr("Slot %1").arg(modelData.slot + 1)
                                        font.family: Tokens.familyBody
                                        font.pixelSize: Tokens.tMicro
                                        color: Tokens.textSecondary
                                    }
                                }
                            }

                            Row {
                                Layout.alignment: Qt.AlignVCenter
                                spacing: Tokens.dp(4)
                                visible: !Tokens.handheld

                                Repeater {
                                    model: ["a", "b", "x", "y", "lb", "rb"]

                                    Rectangle {
                                        required property var modelData
                                        width: Tokens.dp(9)
                                        height: width
                                        radius: width / 2
                                        color: controllerRow.live.buttons
                                               && controllerRow.live.buttons[modelData]
                                               ? Tokens.moon : Tokens.border
                                        Behavior on color {
                                            ColorAnimation {
                                                duration: Tokens.motion(
                                                              Tokens.durationFast)
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        TapHandler {
                            onTapped: {
                                Tokens.inputMode = "pointer"
                                controllerList.currentIndex = index
                                controllerList.forceActiveFocus()
                            }
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        width: Math.min(parent.width - Tokens.gutter * 2,
                                        Tokens.dp(520))
                        visible: controllerList.count === 0
                        text: qsTr("No controllers detected. Connect one to "
                                   + "identify it and test live input.")
                        font.family: Tokens.familyBody
                        font.pixelSize: Tokens.tMeta
                        color: Tokens.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: Tokens.dp(260)
                radius: Tokens.radiusPanel
                color: Tokens.surface
                border.width: Tokens.routeStroke
                border.color: Tokens.border

                ScrollView {
                    id: detailScroll
                    anchors.fill: parent
                    anchors.margins: Tokens.gutter
                    clip: true
                    visible: root.selectedEntry !== null
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    ColumnLayout {
                        width: detailScroll.availableWidth
                        spacing: Tokens.gutter

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Tokens.dp(150)
                            radius: Tokens.radiusControl
                            color: "#08101C"
                            border.color: Tokens.border
                            border.width: Tokens.routeStroke

                            Image {
                                anchors.centerIn: parent
                                width: Math.min(parent.width * 0.72,
                                                Tokens.dp(230))
                                height: parent.height - Tokens.gutter
                                source: root.silhouette(
                                            root.selectedEntry
                                            ? root.selectedEntry.family
                                            : "generic")
                                fillMode: Image.PreserveAspectFit
                                sourceSize.width: 256
                                sourceSize.height: 256
                            }
                            Label {
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: Tokens.gutterTight
                                text: root.familyLabel(
                                          root.selectedEntry
                                          ? root.selectedEntry.family
                                          : "generic")
                                color: "#A6B4C6"
                                font.pixelSize: Tokens.tMicro
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.selectedEntry ? root.selectedEntry.name : ""
                            font.family: Tokens.familyDisplay
                            font.pixelSize: Tokens.tShelf
                            font.weight: Font.Medium
                            color: Tokens.textPrimary
                            elide: Text.ElideRight
                            Accessible.role: Accessible.Heading
                        }
                        Label {
                            Layout.fillWidth: true
                            text: root.selectedEntry
                                  ? root.statusText(root.selectedEntry.connected)
                                    + " · "
                                    + root.familyLabel(root.selectedEntry.family)
                                  : ""
                            font.family: Tokens.familyBody
                            font.pixelSize: Tokens.tMeta
                            color: Tokens.textSecondary
                            elide: Text.ElideRight
                        }

                        Label {
                            text: qsTr("Player Slot Order")
                            font.family: Tokens.familyDisplay
                            font.pixelSize: Tokens.tCard
                            color: Tokens.textPrimary
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: Tokens.gutterTight
                            Repeater {
                                model: [0, 1, 2, 3]
                                delegate: NavigableButton {
                                    required property var modelData
                                    compact: true
                                    text: qsTr("Player %1").arg(modelData + 1)
                                    highlighted: root.selectedEntry
                                                 && root.selectedEntry.slot
                                                    === modelData
                                    onClicked: if (root.selectedEntry)
                                        ControllerManager.assignSlot(
                                            root.selectedEntry.deviceId,
                                            modelData)
                                }
                            }
                        }

                        Label {
                            text: qsTr("Live input")
                            font.family: Tokens.familyDisplay
                            font.pixelSize: Tokens.tCard
                            color: Tokens.textPrimary
                        }
                        GridLayout {
                            Layout.fillWidth: true
                            columns: Tokens.handheld ? 4 : 6
                            columnSpacing: Tokens.gutterTight
                            rowSpacing: Tokens.gutterTight
                            Repeater {
                                model: ["a", "b", "x", "y", "lb", "rb",
                                        "lt", "rt", "dpad_up", "dpad_down",
                                        "dpad_left", "dpad_right"]
                                Rectangle {
                                    required property var modelData
                                    Layout.preferredWidth: Tokens.dp(52)
                                    Layout.preferredHeight: Tokens.dp(44)
                                    radius: Tokens.radiusControl
                                    color: root.selectedLive.buttons
                                           && root.selectedLive.buttons[modelData]
                                           ? Tokens.surfaceFocus : Tokens.night
                                    border.width: Tokens.routeStroke
                                    border.color: root.selectedLive.buttons
                                                  && root.selectedLive.buttons[modelData]
                                                  ? Tokens.borderFocus
                                                  : Tokens.border
                                    Text {
                                        anchors.centerIn: parent
                                        text: Glyphs.glyph(
                                                  root.selectedEntry
                                                  ? root.selectedEntry.family
                                                  : "generic", modelData)
                                        font.family: Glyphs.fontFamily(
                                                         root.selectedEntry
                                                         ? root.selectedEntry.family
                                                         : "generic")
                                        font.pixelSize: Tokens.dp(22)
                                        color: Tokens.textPrimary
                                    }
                                }
                            }
                        }

                        Label {
                            text: qsTr("Controller Map calibration")
                            font.family: Tokens.familyDisplay
                            font.pixelSize: Tokens.tCard
                            color: Tokens.textPrimary
                        }
                        Label {
                            text: qsTr("Left deadzone · %1%")
                                  .arg(Math.round(root.leftDeadzone * 100))
                            color: Tokens.textSecondary
                        }
                        Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: 0.4
                            stepSize: 0.01
                            value: root.leftDeadzone
                            onMoved: root.leftDeadzone = value
                            Accessible.name: qsTr("Left stick deadzone")
                        }
                        Label {
                            text: qsTr("Right deadzone · %1%")
                                  .arg(Math.round(root.rightDeadzone * 100))
                            color: Tokens.textSecondary
                        }
                        Slider {
                            Layout.fillWidth: true
                            from: 0
                            to: 0.4
                            stepSize: 0.01
                            value: root.rightDeadzone
                            onMoved: root.rightDeadzone = value
                            Accessible.name: qsTr("Right stick deadzone")
                        }
                        Label {
                            text: qsTr("Left response curve · %1")
                                  .arg(root.leftCurve.toFixed(1))
                            color: Tokens.textSecondary
                        }
                        Slider {
                            Layout.fillWidth: true
                            from: 0.5
                            to: 2.0
                            stepSize: 0.1
                            value: root.leftCurve
                            onMoved: root.leftCurve = value
                            Accessible.name: qsTr("Left stick response curve")
                        }
                        Label {
                            text: qsTr("Right response curve · %1")
                                  .arg(root.rightCurve.toFixed(1))
                            color: Tokens.textSecondary
                        }
                        Slider {
                            Layout.fillWidth: true
                            from: 0.5
                            to: 2.0
                            stepSize: 0.1
                            value: root.rightCurve
                            onMoved: root.rightCurve = value
                            Accessible.name: qsTr("Right stick response curve")
                        }
                        NavigableButton {
                            Layout.fillWidth: true
                            text: qsTr("Save calibration")
                            onClicked: root.saveCalibration()
                        }

                        Label {
                            text: qsTr("Button remap")
                            font.family: Tokens.familyDisplay
                            font.pixelSize: Tokens.tCard
                            color: Tokens.textPrimary
                        }
                        Row {
                            Layout.fillWidth: true
                            spacing: Tokens.gutterTight
                            AutoResizingComboBox {
                                id: remapSource
                                width: (parent.width - remapArrow.width
                                        - parent.spacing * 2) / 2
                                textRole: "name"
                                model: [
                                    {name: "A", keyValue: "a"},
                                    {name: "B", keyValue: "b"},
                                    {name: "X", keyValue: "x"},
                                    {name: "Y", keyValue: "y"},
                                    {name: "LB", keyValue: "lb"},
                                    {name: "RB", keyValue: "rb"}
                                ]
                            }
                            Label {
                                id: remapArrow
                                text: "→"
                                color: Tokens.link
                            }
                            AutoResizingComboBox {
                                id: remapTarget
                                width: remapSource.width
                                textRole: "name"
                                currentIndex: 1
                                model: remapSource.model
                            }
                        }
                        NavigableButton {
                            Layout.fillWidth: true
                            text: qsTr("Save button remap")
                            onClicked: root.requestRemap(
                                remapSource.model[remapSource.currentIndex]
                                    .keyValue,
                                remapTarget.model[remapTarget.currentIndex]
                                    .keyValue)
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.selectedEntry
                                  ? root.selectedEntry.path : ""
                            font.family: Tokens.familyBody
                            font.pixelSize: Tokens.tMicro
                            color: Tokens.textSecondary
                            elide: Text.ElideMiddle
                        }
                    }
                }

                Column {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - Tokens.gutter * 2,
                                    Tokens.dp(420))
                    spacing: Tokens.gutterTight
                    visible: root.selectedEntry === null
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: Tokens.dp(150)
                        height: Tokens.dp(110)
                        radius: Tokens.radiusControl
                        color: "#08101C"
                        Image {
                            anchors.fill: parent
                            anchors.margins: Tokens.gutterTight
                            source: "qrc:/res/controllers/xbox.svg"
                            fillMode: Image.PreserveAspectFit
                        }
                    }
                    Label {
                        width: parent.width
                        text: controllerList.count === 0
                              ? qsTr("Controller route waiting")
                              : qsTr("Choose a controller")
                        font.family: Tokens.familyDisplay
                        font.pixelSize: Tokens.tCard
                        color: Tokens.textPrimary
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Label {
                        width: parent.width
                        text: controllerList.count === 0
                              ? qsTr("Connect a controller to identify its "
                                     + "layout, assign its Player Slot, and "
                                     + "edit its Controller Map.")
                              : qsTr("Select a controller to inspect live input.")
                        font.family: Tokens.familyBody
                        font.pixelSize: Tokens.tMeta
                        color: Tokens.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    QuickSheet {
        id: remapConflict
        property string conflictingSource: ""
        title: qsTr("Button mapping conflict")

        Label {
            width: parent.width
            text: qsTr("%1 already reaches %2. Swap their targets, let both "
                       + "buttons reach %2, or cancel?")
                  .arg(remapConflict.conflictingSource.toUpperCase())
                  .arg(root.pendingTargetButton.toUpperCase())
            color: Tokens.textSecondary
            wrapMode: Text.WordWrap
        }
        Flow {
            width: parent.width
            spacing: Tokens.gutterTight
            NavigableButton {
                text: qsTr("Swap")
                primary: true
                onClicked: {
                    remapConflict.close()
                    root.commitRemap(true)
                }
            }
            NavigableButton {
                text: qsTr("Keep Both")
                onClicked: {
                    remapConflict.close()
                    root.commitRemap(false)
                }
            }
            NavigableButton {
                text: qsTr("Cancel")
                onClicked: remapConflict.close()
            }
        }
    }
}
