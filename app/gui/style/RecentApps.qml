// Jochona Resume-route memory. Remembers the last applications launched per
// rig — local history only, never host state. Resume continues a running app
// or relaunches it through the same direct host path.
// Stored as a JSON array in the settings key "recent_apps" (SettingsDatabase,
// exposed to QML as the `database` context property; injected via setup()).
pragma Singleton
import QtQuick 2.0

QtObject {
    id: root

    property var database: null
    property var entries: []

    // (entries is reassigned on every mutation, so its notify signal is the change event.)
    readonly property int maxEntries: 10

    function setup(db) {
        root.database = db
        reload()
    }

    function reload() {
        if (database === null || !database.isOpen || !database.isOpen()) {
            entries = []
            entriesChanged()
            return
        }

        var raw = database.setting("recent_apps", "")
        var parsed = []
        if (raw.length > 0) {
            try {
                parsed = JSON.parse(raw)
            } catch (e) {
                parsed = []
            }
        }
        entries = Array.isArray(parsed) ? parsed : []
        entriesChanged()
    }

    // Record a launch. Dedupes on host uuid + app id, newest first, bounded.
    function record(hostUuid, hostName, appId, appName) {
        var next = []
        for (var i = 0; i < entries.length; i++) {
            var e = entries[i]
            if (e.uuid === hostUuid && e.appid === appId) {
                continue
            }
            next.push(e)
        }
        next.unshift({
            "uuid": hostUuid,
            "host": hostName,
            "appid": appId,
            "name": appName,
            "at": Date.now()
        })
        if (next.length > maxEntries) {
            next = next.slice(0, maxEntries)
        }
        entries = next
        entriesChanged()

        if (database !== null && database.isOpen && database.isOpen()) {
            database.setSetting("recent_apps", JSON.stringify(entries))
        }
    }

    // Drop entries for a removed Host.
    function forgetHost(hostUuid) {
        var next = []
        for (var i = 0; i < entries.length; i++) {
            if (entries[i].uuid !== hostUuid) {
                next.push(entries[i])
            }
        }
        if (next.length !== entries.length) {
            entries = next
            entriesChanged()
            if (database !== null && database.isOpen && database.isOpen()) {
                database.setSetting("recent_apps", JSON.stringify(entries))
            }
        }
    }

    // Up to `count` entries whose Host is currently online+paired, resolved
    // against the live ComputerModel (entries pointing at removed/offline
    // hosts are kept in storage but not shown).
    function visibleEntries(computerModel, count) {
        var result = []
        if (computerModel === null) {
            return result
        }
        for (var i = 0; i < entries.length && result.length < count; i++) {
            var e = entries[i]
            var idx = computerModel.indexOfUuid(e.uuid)
            if (idx < 0) {
                continue
            }
            if (!computerModel.isOnlinePaired(idx)) {
                continue
            }
            // Shallow copy carries its route position so delegates can require
            // `index` exactly like ComputerModel-backed destinations.
            result.push({ uuid: e.uuid, hostName: e.host || e.hostName || "",
                          appid: e.appid, name: e.name, index: result.length })
        }
        return result
    }
}
