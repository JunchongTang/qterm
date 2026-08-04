pragma Singleton

import QtQuick

// Design tokens ported from shadcn/ui (base-mira), dark palette by default.
QtObject {
    property bool dark: true

    // Color tokens. The dark surfaces sit on zinc-900 rather than shadcn's
    // near-black default, which reads as too heavy at terminal-window size.
    readonly property color background: dark ? "#18181b" : "#ffffff"
    readonly property color foreground: dark ? "#fafafa" : "#0a0a0a"
    readonly property color card: dark ? "#1f1f23" : "#ffffff"
    readonly property color cardForeground: dark ? "#fafafa" : "#0a0a0a"
    readonly property color popover: dark ? "#1f1f23" : "#ffffff"
    readonly property color popoverForeground: dark ? "#fafafa" : "#0a0a0a"
    readonly property color primary: dark ? "#3b82f6" : "#2563eb"
    readonly property color primaryForeground: "#ffffff"
    readonly property color secondary: dark ? "#2e2e33" : "#f4f4f5"
    readonly property color secondaryForeground: dark ? "#fafafa" : "#18181b"
    readonly property color muted: dark ? "#2e2e33" : "#f5f5f5"
    readonly property color mutedForeground: dark ? "#a1a1aa" : "#737373"
    readonly property color accent: dark ? "#2e2e33" : "#f5f5f5"
    readonly property color accentForeground: dark ? "#fafafa" : "#171717"
    readonly property color destructive: dark ? "#ff6467" : "#e7000b"
    readonly property color border: dark ? "#1affffff" : "#e5e5e5"
    readonly property color input: dark ? "#26ffffff" : "#e5e5e5"
    readonly property color ring: dark ? "#737373" : "#a1a1a1"

    // Corner radii (base radius 10, ratios from base-mira)
    readonly property real radius: 10
    readonly property real radiusSm: radius * 0.6
    readonly property real radiusMd: radius * 0.8
    readonly property real radiusLg: radius
    readonly property real radiusXl: radius * 1.4
    readonly property real radiusFull: 9999

    // Focus ring (ring-2 ring-ring/30)
    readonly property real ringWidth: 2
    readonly property real ringOpacity: 0.30

    // Overlay elevation (ring-1 ring-foreground/10 + shadow-md)
    readonly property color overlayRing: alpha(foreground, 0.10)
    readonly property real overlayRingWidth: 1
    readonly property color shadowColor: alpha("#000000", dark ? 0.5 : 0.12)
    readonly property real shadowBlur: 0.5
    readonly property real shadowOffset: 4

    // Spacing scale (Tailwind 0.25rem x n)
    readonly property real space1: 4
    readonly property real space1_5: 6
    readonly property real space2: 8
    readonly property real space2_5: 10
    readonly property real space3: 12
    readonly property real space4: 16
    readonly property real space6: 24
    readonly property real space8: 32

    // Font sizes (Tailwind text-xs..text-lg)
    readonly property int textXs: 12
    readonly property int textSm: 14
    readonly property int textBase: 16
    readonly property int textLg: 18

    // Motion
    readonly property int durFast: 100
    readonly property int durBase: 150

    function alpha(c, a) { return Qt.rgba(c.r, c.g, c.b, a) }
}
