// Jochona design tokens (M1). Single source of truth for the modern shell's
// palette, type scale, metrics, and motion.
//
// - Colors are live bindings into ThemeEngine (built-in palette set + user
//   accent override). Switching themes restyles every consumer instantly.
// - Sizes scale with the user's fontScale; row/gutter follow density.
// - Every animation duration MUST route through motion() so reduced-motion
//   collapses it to 0.
pragma Singleton
import QtQuick 2.0

QtObject {
    readonly property var _engine: themeManager

    // --- Color ---
    readonly property color surface:        ThemeEngine.surface
    readonly property color surfaceFocus:   ThemeEngine.surfaceFocus
    readonly property color border:         ThemeEngine.border
    readonly property color borderFocus:    ThemeEngine.borderFocus
    readonly property color textPrimary:    ThemeEngine.textPrimary
    readonly property color textSecondary:  ThemeEngine.textSecondary
    readonly property color accent:         ThemeEngine.accent
    readonly property color accentFocus:    ThemeEngine.accentFocus
    readonly property color statusOnline:   ThemeEngine.statusOnline
    readonly property color statusPairing:  ThemeEngine.statusPairing
    readonly property color statusOffline:  ThemeEngine.statusOffline
    readonly property color statusUnknown:  ThemeEngine.statusUnknown

    // --- Type scale (scaled by ui.fontScale, 0.8–1.6) ---
    // 4.5:1 contrast minimum is guaranteed for the base pairs in every theme;
    // scaling only makes glyphs larger, never changes color.
    readonly property int fontScalePct: _engine ? Math.round(_engine.fontScale * 100) : 100

    readonly property int sizeMicro:  Math.round(11 * fontScalePct / 100)
    readonly property int sizeBody:   Math.round(14 * fontScalePct / 100)
    readonly property int sizeSection: Math.round(17 * fontScalePct / 100)
    readonly property int sizeTitle:  Math.round(22 * fontScalePct / 100)
    readonly property int sizeHero:   Math.round(32 * fontScalePct / 100)

    // Widest the shell columns/rows stretch to on big desktops; keeps the
    // living-room line measure readable on a 4K monitor.
    readonly property int listMaxWidth: 1200
    readonly property string familyDisplay: "Space Grotesk"
    readonly property string familyBody: "Inter"

    // --- Metrics (density-switched) ---
    // rowHeight is the 96dp controller focus target at comfortable density;
    // compact still exceeds the 72dp lower bound (proposal §8: legibility/
    // reachability floor is preserved, density trades space, not usability).
    readonly property bool compact: _engine ? _engine.compactDensity : false

    readonly property int rowHeight:    compact ? 76 : 96
    readonly property int focusRadius:  4
    readonly property int gutter:       compact ? 14 : 20
    readonly property int radiusCard:   10

    // --- Motion ---
    readonly property int durationFast: 120
    readonly property int durationBase: 200

    // Every animation MUST use this; reduced-motion sets duration to 0.
    function motion(duration) {
        return (_engine && _engine.reducedMotion) ? 0 : duration
    }
}
