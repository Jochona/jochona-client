// Jochona theme engine (M1). Owns the built-in palette set, resolves custom
// data-only theme packages, and picks the active one from ThemeManager
// (persisted id) + the OS light/dark hint.
//
// Contract with Tokens.qml: Tokens forwards its color names here, so screens
// never reference ThemeEngine directly. Adding a built-in theme = one palette
// object in `palettes` + one id in ThemeManager::s_BuiltinThemeIds. Custom
// packages (docs/theme-packages.md) must pass strict validation; anything
// invalid — bad JSON, unknown fields, unparseable colors, wrong schema —
// fails safe to the built-in "dark".
pragma Singleton
import QtQuick 2.0

QtObject {
    id: root

    readonly property var manager: themeManager

    // OS appearance; "system" resolves through this. Qt 6.5+ style hint.
    readonly property bool osDark: Qt.styleHints.colorScheme === Qt.Dark

    readonly property var palettes: ({
        // Shipped Jochona dark — the incumbent look
        "dark": {
            surface:       "#262b38",
            surfaceFocus:  "#323a4d",
            border:        "#3a4152",
            borderFocus:   "#7986cb",
            textPrimary:   "#ffffff",
            textSecondary: "#9fa8ba",
            accent:        "#7986cb",
            accentFocus:   "#9fa8ff",
            statusOnline:  "#4caf50",
            statusPairing: "#ffb300",
            statusOffline: "#757575",
            statusUnknown: "#9e9e9e"
        },
        // Pure black surfaces; borders strengthened to keep structure without
        // glow. OLED panels: black pixels are off, so no painted surface tint.
        "oled": {
            surface:       "#000000",
            surfaceFocus:  "#141a26",
            border:        "#2a3140",
            borderFocus:   "#9fa8ff",
            textPrimary:   "#ffffff",
            textSecondary: "#aab3c5",
            accent:        "#8ea2ff",
            accentFocus:   "#b3c1ff",
            statusOnline:  "#66bb6a",
            statusPairing: "#ffc233",
            statusOffline: "#8a8a8a",
            statusUnknown: "#a3a3a3"
        },
        // Daylight rooms / projection: cool paper, ink navy text, all pairs
        // at or above 4.5:1 against their surface.
        "light": {
            surface:       "#f2f4fa",
            surfaceFocus:  "#e2e7f5",
            border:        "#c3c9dd",
            borderFocus:   "#3949ab",
            textPrimary:   "#131a2e",
            textSecondary: "#4c5670",
            accent:        "#3949ab",
            accentFocus:   "#2c3a96",
            statusOnline:  "#2e7d32",
            statusPairing: "#8a5a00",
            statusOffline: "#5c5c5c",
            statusUnknown: "#6e6e6e"
        },
        // High contrast: pure ground, saturated ink, thick-focus-friendly
        // bright edges. Status colors are always paired with labels by rule.
        "highcontrast": {
            surface:       "#000000",
            surfaceFocus:  "#0b0b0b",
            border:        "#ffffff",
            borderFocus:   "#ffd60a",
            textPrimary:   "#ffffff",
            textSecondary: "#f2f2f2",
            accent:        "#ffd60a",
            accentFocus:   "#ffe45c",
            statusOnline:  "#00e676",
            statusPairing: "#ffd60a",
            statusOffline: "#d0d0d0",
            statusUnknown: "#e0e0e0"
        }
    })

    // Palette roles a custom theme may override. Anything else in a package's
    // colors object voids the whole package (schema strictness beats partial
    // application — a typo'd role must not half-theme the shell).
    readonly property var overridableRoles: [
        "surface", "surfaceFocus", "border", "borderFocus",
        "textPrimary", "textSecondary", "accent", "accentFocus",
        "statusOnline", "statusPairing", "statusOffline", "statusUnknown"
    ]

    // --- Custom theme package loading (data-only; docs/theme-packages.md) ---

    // Returns a validated merged palette, or null on any violation.
    function loadCustom(customId) {
        if (!manager) {
            return null
        }
        var url = "file://" + manager.themesPath() + "/" + customId + "/theme.json"
        var xhr = new XMLHttpRequest()
        xhr.open("GET", url, false)
        xhr.send()
        var raw = xhr.responseText
        if (raw === undefined || raw.length === 0) {
            return null
        }

        var doc
        try {
            doc = JSON.parse(raw)
        } catch (e) {
            console.warn("ThemeEngine: invalid JSON in theme package", customId)
            return null
        }

        if (!doc || doc.schema !== 1) {
            return null
        }
        if (doc.id !== customId) {
            return null
        }
        if (typeof doc.name !== "string" || doc.name.length === 0 || doc.name.length > 64) {
            return null
        }
        if (doc.extends === "system" || palettes[doc.extends] === undefined) {
            return null
        }
        var base = palettes[doc.extends]
        if (!doc.colors || typeof doc.colors !== "object") {
            return null
        }

        var hex = /^#[0-9a-fA-F]{6}([0-9a-fA-F]{2})?$/
        var merged = {}
        for (var k in base) {
            merged[k] = base[k]
        }
        var keys = Object.keys(doc.colors)
        for (var i = 0; i < keys.length; i++) {
            var role = keys[i]
            if (overridableRoles.indexOf(role) < 0) {
                return null // unknown role voids the package
            }
            var v = doc.colors[role]
            if (typeof v !== "string" || !hex.test(v)) {
                return null // only #rrggbb / #rrggbbaa hex, nothing else
            }
            merged[role] = v
        }
        return merged
    }

    function resolve(wanted) {
        if (manager && manager.isCustom(wanted)) {
            var customId = wanted.substring(7)
            var p = loadCustom(customId)
            if (p !== null) {
                return { id: wanted, palette: p }
            }
            console.warn("ThemeEngine: theme package failed validation; falling back to dark")
            return { id: "dark", palette: palettes["dark"] }
        }
        if (wanted === "system") {
            var sys = root.osDark ? "dark" : "light"
            return { id: sys, palette: palettes[sys] }
        }
        if (palettes[wanted] === undefined) {
            return { id: "dark", palette: palettes["dark"] }
        }
        return { id: wanted, palette: palettes[wanted] }
    }

    // Active palette + id. Binding into manager.theme, so a persisted change
    // re-resolves automatically through the NOTIFY signal.
    property var _resolved: resolve(manager ? manager.theme : "dark")

    readonly property string activeId: _resolved.id

    readonly property var _palette: _resolved.palette

    // User accent overrides the palette accent when set (ThemeManager
    // validates/normalizes in C++; empty = theme default).
    readonly property color accent: {
        var a = manager ? manager.accent : ""
        return a.length > 0 ? a : _palette.accent
    }

    readonly property color accentFocus: {
        var a = manager ? manager.accent : ""
        return a.length > 0 ? Qt.lighter(a, 1.25) : _palette.accentFocus
    }

    readonly property color surface:        _palette.surface
    readonly property color surfaceFocus:   _palette.surfaceFocus
    readonly property color border:         _palette.border
    readonly property color borderFocus:    _palette.borderFocus
    readonly property color textPrimary:    _palette.textPrimary
    readonly property color textSecondary:  _palette.textSecondary
    readonly property color statusOnline:   _palette.statusOnline
    readonly property color statusPairing:  _palette.statusPairing
    readonly property color statusOffline:  _palette.statusOffline
    readonly property color statusUnknown:  _palette.statusUnknown
}
