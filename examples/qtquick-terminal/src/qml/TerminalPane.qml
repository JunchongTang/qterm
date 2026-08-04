pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic as QQC
import QTerm
import QtQuickTerminal

// Hosts the terminal renderer plus scrollbar, cursor blink, bell flash and
// clipboard shortcuts.
Item {
    id: root

    required property QTermTerminal terminal
    property int contentPadding: 10
    property real cursorBlinkOpacity: 1.0
    property real bellFlashOpacity: 0.0
    property string fontFamily: Qt.platform.os === "windows" ? "Consolas"
                                : Qt.platform.os === "osx" ? "Menlo" : "Monospace"
    property int fontPixelSize: 16

    // The renderer is a value type consumer, so the palette has to be pushed
    // again whenever the renderer is rebuilt or the app theme flips.
    function applyTerminalTheme() {
        if (rendererLoader.item)
            themeHelper.applyTheme(rendererLoader.item, Theme.dark)
    }

    Connections {
        target: Theme
        function onDarkChanged() { root.applyTerminalTheme() }
    }

    SequentialAnimation {
        id: cursorBlink
        running: root.terminal.surfaceModel.cursorVisible && root.visible
        loops: Animation.Infinite

        PauseAnimation { duration: 420 }
        NumberAnimation { target: root; property: "cursorBlinkOpacity"; to: 0.16; duration: 120 }
        PauseAnimation { duration: 260 }
        NumberAnimation { target: root; property: "cursorBlinkOpacity"; to: 1.0; duration: 120 }
    }

    SequentialAnimation {
        id: bellFlash
        NumberAnimation { target: root; property: "bellFlashOpacity"; to: 0.0; duration: 180 }
    }

    Connections {
        target: root.terminal
        function onBell() {
            bellFlash.stop()
            root.bellFlashOpacity = 0.3
            bellFlash.start()
        }
    }

    Component {
        id: sgRendererComponent
        QTermQuickItem {
            anchors.fill: parent
            focus: true
            terminal: root.terminal
            fontFamily: root.fontFamily
            fontPixelSize: root.fontPixelSize
            cursorOpacity: root.cursorBlinkOpacity
            onCopyRequested: clipboardBridge.copyText(root.terminal.surfaceModel.selectedText)
            onHyperlinkActivated: url => Qt.openUrlExternally(url)
        }
    }

    Component {
        id: paintedRendererComponent
        QTermQuickPaintedItem {
            anchors.fill: parent
            focus: true
            terminal: root.terminal
            fontFamily: root.fontFamily
            fontPixelSize: root.fontPixelSize
            cursorOpacity: root.cursorBlinkOpacity
            onCopyRequested: text => clipboardBridge.copyText(text)
            onHyperlinkActivated: url => Qt.openUrlExternally(url)
        }
    }

    Item {
        id: contentArea
        anchors.fill: parent
        anchors.margins: root.contentPadding

        Loader {
            id: rendererLoader
            anchors.fill: parent
            focus: true
            sourceComponent: AppState.useSceneGraphRenderer ? sgRendererComponent
                                                            : paintedRendererComponent
            onLoaded: {
                root.applyTerminalTheme()
                rendererLoader.item.forceActiveFocus()
            }
        }
    }

    QQC.ScrollBar {
        id: termScrollBar
        anchors.right: parent.right
        anchors.top: contentArea.top
        anchors.bottom: contentArea.bottom
        anchors.rightMargin: 2
        orientation: Qt.Vertical
        policy: QQC.ScrollBar.AsNeeded

        size: rendererLoader.item ? rendererLoader.item.scrollSize : 1.0

        Binding {
            target: termScrollBar
            property: "position"
            value: rendererLoader.item ? rendererLoader.item.scrollPosition : 0.0
            when: !termScrollBar.pressed
        }

        onPositionChanged: {
            if (pressed && rendererLoader.item)
                rendererLoader.item.scrollPosition = position
        }

        contentItem: Rectangle {
            implicitWidth: 6
            radius: 3
            color: Theme.alpha(Theme.foreground,
                               termScrollBar.pressed ? 0.5 : termScrollBar.hovered ? 0.35 : 0.25)
        }

        background: Item { implicitWidth: 6 }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.foreground
        opacity: root.bellFlashOpacity
        enabled: false
    }

    Shortcut {
        sequence: StandardKey.Copy
        enabled: rendererLoader.activeFocus
        onActivated: {
            const text = root.terminal.surfaceModel.selectedText
            if (text.length > 0)
                clipboardBridge.copyText(text)
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
