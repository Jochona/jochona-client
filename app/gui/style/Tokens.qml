// Jochona design tokens (M1). Single source of truth for the modern shell's
// palette, type scale, and metrics. Values currently mirror the first Jochona
// renders so adopting Tokens is a no-op refactor; identity shifts (electric
// blue accent, navy surfaces) happen here as one deliberate diff.
//
// Registry: style/qmldir (import "style" or "../style" from gui screens).
pragma Singleton
import QtQuick 2.0

QtObject {
    // --- Surfaces & structure ---
    readonly property color surface:        "#262b38"  // card background
    readonly property color surfaceFocus:   "#323a4d"  // focused/selected card
    readonly property color border:         "#3a4152"
    readonly property color borderFocus:    "#7986cb"

    // --- Brand (icon/hero masters) ---
    readonly property color brandNavy:      "#070E35"  // deep space navy (icon ink)
    readonly property color brandElectric:  "#5B8CFF"  // electric blue (icon rim)
    readonly property color brandLavender:  "#C7CDF2"  // lavender (light-mode marks)

    // --- Text ---
    readonly property color textPrimary:    "#ffffff"
    readonly property color textSecondary:  "#9fa8ba"
    readonly property color accent:         "#7986cb"
    readonly property color accentFocus:    "#9fa8ff"

    // --- Status (never color alone: every use pairs a text label) ---
    readonly property color statusOnline:   "#4caf50"
    readonly property color statusPairing:  "#ffb300"
    readonly property color statusOffline:  "#757575"
    readonly property color statusUnknown:  "#9e9e9e"

    // --- Type ---
    readonly property string fontUi:        "Inter"          // app-wide via main.cpp
    readonly property string fontDisplay:   "Space Grotesk"  // headings, host names
    readonly property int sizeBody:         11
    readonly property int sizeAction:       13
    readonly property int sizeSection:      18
    readonly property int sizeTitle:        22

    // --- Metrics ---
    readonly property int radiusCard:       10
    readonly property int rowHeight:        96
    readonly property int gutter:           20
    readonly property int listMaxWidth:     900
}
