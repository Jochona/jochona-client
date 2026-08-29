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

    // Night Route roles: `night` is the environment, `surface` a raised route
    // field, `moon` the narrow active cue, `link` the live connection line,
    // and `scrim` the scene-preserving modal veil. Classic role names remain
    // part of schema 1 so existing data-only theme packages still merge.
    readonly property var palettes: ({
        // Dark: the default living-room and desktop night scene.
        "dark": {
            night:         "#08101C",
            surface:       "#111D2E",
            surfaceFocus:  "#192A43",
            border:        "#2A3A52",
            borderFocus:   "#AFC8E8",
            textPrimary:   "#EAF0F7",
            textSecondary: "#A6B4C6",
            moon:          "#B7A7FF",
            moonDim:       "#96A8C0",
            link:          "#88B7DA",
            scrim:         "#02060CD9",
            accent:        "#88B7DA",
            accentFocus:   "#B7A7FF",
            statusOnline:  "#65C58A",
            statusPairing: "#E0B968",
            statusOffline: "#7F8A9C",
            statusUnknown: "#A0ABBC"
        },
        // OLED: black is structural, not a tinted card color.
        "oled": {
            night:         "#000000",
            surface:       "#05070B",
            surfaceFocus:  "#101A2A",
            border:        "#2C3544",
            borderFocus:   "#BFD7F2",
            textPrimary:   "#F1F5FA",
            textSecondary: "#AEB9C8",
            moon:          "#C1B5FF",
            moonDim:       "#9BA9BC",
            link:          "#92C4E8",
            scrim:         "#000000EB",
            accent:        "#92C4E8",
            accentFocus:   "#C1B5FF",
            statusOnline:  "#6BD695",
            statusPairing: "#F0C66E",
            statusOffline: "#929CAA",
            statusUnknown: "#BAC3CF"
        },
        // Light: fogged glass and blue-black ink for bright rooms.
        "light": {
            night:         "#E8EDF3",
            surface:       "#F7F9FC",
            surfaceFocus:  "#DCE8F3",
            border:        "#B5C4D3",
            borderFocus:   "#536F91",
            textPrimary:   "#111B2B",
            textSecondary: "#465972",
            moon:          "#66569D",
            moonDim:       "#596A80",
            link:          "#365F82",
            scrim:         "#111B2BBF",
            accent:        "#365F82",
            accentFocus:   "#66569D",
            statusOnline:  "#237A46",
            statusPairing: "#805D12",
            statusOffline: "#59616D",
            statusUnknown: "#4E6074"
        },
        // High contrast favors discrimination over brand nuance.
        "highcontrast": {
            night:         "#000000",
            surface:       "#000000",
            surfaceFocus:  "#111111",
            border:        "#FFFFFF",
            borderFocus:   "#FFD60A",
            textPrimary:   "#FFFFFF",
            textSecondary: "#FFFFFF",
            moon:          "#FFD60A",
            moonDim:       "#FFFFFF",
            link:          "#6EC1FF",
            scrim:         "#000000F2",
            accent:        "#6EC1FF",
            accentFocus:   "#FFD60A",
            statusOnline:  "#00E676",
            statusPairing: "#FFD60A",
            statusOffline: "#FFFFFF",
            statusUnknown: "#FFFFFF"
        }
    })

    // Palette roles a custom theme may override. Anything else voids the
    // package; typos never create a half-themed interface.
    readonly property var overridableRoles: [
        "surface", "surfaceFocus", "border", "borderFocus",
        "textPrimary", "textSecondary", "accent", "accentFocus",
        "statusOnline", "statusPairing", "statusOffline", "statusUnknown",
        "night", "moon", "moonDim", "link", "scrim"
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

    function readableInk(colorValue) {
        var brightness = colorValue.r * 0.299 + colorValue.g * 0.587
                         + colorValue.b * 0.114
        return brightness > 0.58 ? "#07101C" : "#FFFFFF"
    }
    readonly property color focusInk: readableInk(accentFocus)

    readonly property color surface:        _palette.surface
    readonly property color surfaceFocus:   _palette.surfaceFocus
    readonly property color border:         _palette.border
    readonly property color borderFocus:    _palette.borderFocus
    readonly property color textPrimary:    _palette.textPrimary
    readonly property color textSecondary:  _palette.textSecondary
    readonly property color statusOnline:   _palette.statusOnline
    readonly property color statusPairing:  _palette.statusPairing
    readonly property color statusOffline:  _palette.statusOffline

    // Night Route roles. Schema-1 packages may omit them; every missing role
    // falls back to a classic role or a safe scene veil.
    readonly property color night:     _palette.night !== undefined ? _palette.night : _palette.surface
    readonly property color moon:      _palette.moon !== undefined ? _palette.moon : _palette.accent
    readonly property color moonDim:   _palette.moonDim !== undefined ? _palette.moonDim : _palette.textSecondary
    readonly property color link:      _palette.link !== undefined ? _palette.link : _palette.accent
    readonly property color scrim:     _palette.scrim !== undefined ? _palette.scrim : "#000000D9"
    readonly property color statusUnknown: _palette.statusUnknown
}
