// Jochona Night Route design tokens. One responsive system serves handheld,
// desktop, and ten-foot layouts; screens use dimensions from dp()/tv() and
// text roles from the t* or size* ramps instead of raw pixels.
pragma Singleton
import QtQuick 2.0

QtObject {
    readonly property var _engine: themeManager

    // Palette roles. ThemeEngine owns concrete values and custom-package
    // validation; screens never reach into a palette directly.
    readonly property color night:          ThemeEngine.night
    readonly property color surface:        ThemeEngine.surface
    readonly property color surfaceFocus:   ThemeEngine.surfaceFocus
    readonly property color border:         ThemeEngine.border
    readonly property color borderFocus:    ThemeEngine.borderFocus
    readonly property color textPrimary:    ThemeEngine.textPrimary
    readonly property color textSecondary:  ThemeEngine.textSecondary
    readonly property color focusInk:       ThemeEngine.focusInk
    readonly property color scrim:          ThemeEngine.scrim
    readonly property color accent:         ThemeEngine.accent
    readonly property color accentFocus:    ThemeEngine.accentFocus
    readonly property color moon:           ThemeEngine.moon
    readonly property color moonDim:        ThemeEngine.moonDim
    readonly property color link:           ThemeEngine.link
    readonly property color statusOnline:   ThemeEngine.statusOnline
    readonly property color statusPairing:  ThemeEngine.statusPairing
    readonly property color statusOffline:  ThemeEngine.statusOffline
    readonly property color statusUnknown:  ThemeEngine.statusUnknown

    // Runtime scene. The shell updates viewport dimensions and inputMode.
    // Layout is not a scaled TV mockup: each scene gets its own composition.
    property int viewportWidth: 1280
    property int viewportHeight: 720
    property string inputMode: "pointer" // controller | keyboard | pointer

    readonly property int shortEdge: Math.min(viewportWidth, viewportHeight)
    readonly property int longEdge: Math.max(viewportWidth, viewportHeight)
    readonly property bool controllerMode: inputMode === "controller"
    readonly property bool pointerMode: inputMode === "pointer"
    readonly property bool handheld: controllerMode
                                         && shortEdge <= 900 && longEdge <= 1600
    readonly property bool tenFoot: controllerMode && viewportWidth >= 1600
                                        && viewportHeight >= 900
    readonly property bool desktop: !handheld && !tenFoot
    readonly property bool compactViewport: viewportWidth < 1100
                                                 || viewportHeight < 720
    readonly property bool wideViewport: !handheld && viewportWidth >= 1200
                                              && viewportWidth / Math.max(1, viewportHeight) >= 1.45

    property real uiScale: 1.0
    function scaleFor(width, height) {
        var shortSide = Math.min(width, height)
        var longSide = Math.max(width, height)
        if (controllerMode && shortSide <= 900 && longSide <= 1600) {
            return Math.max(0.62, Math.min(0.90, shortSide / 1080))
        }
        if (controllerMode && width >= 1600 && height >= 900) {
            return Math.max(0.90, Math.min(1.60, shortSide / 1080))
        }
        return Math.max(0.70, Math.min(1.15, shortSide / 900))
    }

    readonly property int fontScalePct: _engine
                                                ? Math.round(_engine.fontScale * 100)
                                                : 100

    // Geometry follows the scene; text also follows the user's font scale.
    function dp(value) {
        return Math.max(1, Math.round(value * uiScale))
    }
    function tv(value) {
        return dp(value)
    }
    function textPx(value) {
        return Math.max(1, Math.round(value * uiScale * fontScalePct / 100))
    }
    function textPt(referencePx) {
        return Math.max(8, Math.round(textPx(referencePx) * 0.75))
    }

    // Point-size compatibility for Qt Controls that still consume pointSize.
    // These now share the same device and font scaling as the pixel ramp.
    readonly property int sizeMicro:   textPt(16)
    readonly property int sizeBody:    textPt(22)
    readonly property int sizeSection: textPt(26)
    readonly property int sizeTitle:   textPt(36)
    readonly property int sizeHero:    textPt(58)

    readonly property string familyDisplay: "Space Grotesk"
    readonly property string familyBody: "Inter"

    readonly property int tHero:   textPx(handheld ? 48 : 58)
    readonly property int tTitle:  textPx(handheld ? 32 : 38)
    readonly property int tShelf:  textPx(26)
    readonly property int tCard:   textPx(24)
    readonly property int tMeta:   textPx(22)
    readonly property int tChip:   textPx(20)
    readonly property int tMicro:  textPx(16)

    // Route geometry. Safe insets do not grow with font scale; content
    // containers do, so 200% text remains inside the viewport.
    readonly property int safeInset: Math.max(dp(20),
                                              Math.round(shortEdge
                                                         * (tenFoot ? 0.045
                                                                    : handheld ? 0.025 : 0.035)))
    readonly property bool compact: _engine ? _engine.compactDensity : false
    readonly property int gutter: dp(compact ? 20 : 28)
    readonly property int gutterTight: dp(compact ? 10 : 14)
    readonly property int rowHeight: Math.max(
                                         dp(compact ? 72 : 88),
                                         tCard + tMicro + gutterTight * 2)
    readonly property int actionHeight: Math.max(
                                            dp(compact ? 50 : 60),
                                            tMeta + gutterTight * 2)
    readonly property int routeBarHeight: Math.max(
                                              dp(handheld ? 76
                                                          : tenFoot ? 92 : 68),
                                              tMeta + tMicro
                                              + gutterTight * 2)
    readonly property int panelWidth: Math.min(dp(560),
                                               Math.round(viewportWidth * (handheld ? 0.92 : 0.44)))
    readonly property int listMaxWidth: dp(1320)
    readonly property int radiusCard: dp(10)
    readonly property int radiusControl: dp(8)
    readonly property int radiusPanel: dp(12)
    readonly property int focusRadius: radiusControl
    readonly property int focusStroke: Math.max(2, dp(2))
    readonly property int routeStroke: Math.max(1, dp(1))

    readonly property int durationFast: 110
    readonly property int durationBase: 180
    readonly property int durationRoute: 240

    function motion(duration) {
        return (_engine && _engine.reducedMotion) ? 0 : duration
    }
}
