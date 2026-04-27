pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QTerm

// TerminalPane wraps the actual terminal renderer (QSG or Painted) and
// owns the cursor-blink animation, bell-flash overlay, and clipboard shortcuts.
// The caller provides a QTermTerminal and the global clipboardBridge.
Item {
    id: root

    required property QTermTerminal terminal
    // clipboardBridge is a context property set by main.cpp

    property bool useSGRenderer: true
    property real cursorBlinkOpacity: 1.0
    property real bellFlashOpacity: 0.0
    property string fontFamily: "Menlo"
    property int fontPixelSize: 16

    signal copyRequested(string text)

    // ── Animations ────────────────────────────────────────────────────────────

    SequentialAnimation {
        id: cursorBlink
        running: root.terminal.surfaceModel.cursorVisible
        loops: Animation.Infinite

        PauseAnimation { duration: 420 }
        NumberAnimation {
            target: root
            property: "cursorBlinkOpacity"
            to: 0.15
            duration: 120
        }
        PauseAnimation { duration: 260 }
        NumberAnimation {
            target: root
            property: "cursorBlinkOpacity"
            to: 1.0
            duration: 120
        }
    }

    SequentialAnimation {
        id: bellFlash

        NumberAnimation {
            target: root
            property: "bellFlashOpacity"
            to: 0.0
            duration: 180
        }
    }

    // ── Event connections ─────────────────────────────────────────────────────

    Connections {
        target: root.terminal

        function onBell() {
            bellFlash.stop()
            root.bellFlashOpacity = 0.35
            bellFlash.start()
        }
    }

    // ── Renderer components ───────────────────────────────────────────────────

    Component {
        id: sgRendererComponent

        QTermQuickItem {
            focus: true
            terminal: root.terminal
            fontFamily: root.fontFamily
            fontPixelSize: root.fontPixelSize
            cursorOpacity: root.cursorBlinkOpacity

            onCopyRequested: {
                const text = root.terminal.surfaceModel.selectedText
                root.copyRequested(text)
            }
            onHyperlinkActivated: url => Qt.openUrlExternally(url)
        }
    }

    Component {
        id: paintedRendererComponent

        QTermQuickPaintedItem {
            focus: true
            terminal: root.terminal
            fontFamily: root.fontFamily
            fontPixelSize: root.fontPixelSize
            cursorOpacity: root.cursorBlinkOpacity

            onCopyRequested: text => root.copyRequested(text)
            onHyperlinkActivated: url => Qt.openUrlExternally(url)
        }
    }

    Loader {
        id: rendererLoader

        anchors.fill: parent
        focus: true
        sourceComponent: root.useSGRenderer ? sgRendererComponent : paintedRendererComponent

        onLoaded: {
            themeHelper.applyDarkTheme(rendererLoader.item)
        }
    }

    // ── Bell flash overlay ────────────────────────────────────────────────────

    Rectangle {
        anchors.fill: parent
        color: "#d9ffe3"
        opacity: root.bellFlashOpacity
        z: 10
        enabled: false
    }

    // ── Keyboard shortcuts ────────────────────────────────────────────────────

    Shortcut {
        sequence: StandardKey.Copy
        enabled: rendererLoader.activeFocus

        onActivated: {
            const text = root.terminal.surfaceModel.selectedText
            if (text.length > 0) {
                clipboardBridge.copyText(text)
                root.copyRequested(text)
            }
        }
    }

    Shortcut {
        sequence: StandardKey.Paste
        enabled: rendererLoader.activeFocus

        onActivated: {
            const text = clipboardBridge.clipboardText()
            if (text.length > 0)
                root.terminal.sendPaste(text)
        }
    }
}
