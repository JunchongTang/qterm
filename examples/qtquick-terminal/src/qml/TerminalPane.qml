pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic as QQC
import QTerm
import QtQuickTerminal

// Hosts the terminal renderer plus scrollbar, cursor blink, bell flash,
// clipboard shortcuts, the find bar and the context menu.
Item {
    id: root

    required property QTermTerminal terminal
    property int contentPadding: 10
    property real cursorBlinkOpacity: 1.0
    property real bellFlashOpacity: 0.0
    property string fontFamily: Qt.platform.os === "windows" ? "Consolas"
                                : Qt.platform.os === "osx" ? "Menlo" : "Monospace"
    property int fontPixelSize: 16

    // Set while the context menu is open, so the link entries can act on the
    // cell that was actually right-clicked rather than the current hover.
    property int menuHyperlinkId: 0

    signal newTabRequested()
    signal closeTabRequested()

    // Exposed so the demo's screenshot probe can open them; the app itself
    // reaches them through the right-click handler and the shortcuts.
    property alias contextMenu: contextMenu
    readonly property bool findVisible: searchLoader.active

    function copySelection() {
        const text = root.terminal.surfaceModel.selectedText
        if (text.length > 0)
            clipboardBridge.copyText(text)
    }

    function pasteClipboard() {
        const text = clipboardBridge.clipboardText()
        if (text.length > 0)
            root.terminal.sendPaste(text)
    }

    // Without an argument the bar opens seeded with the current selection,
    // which is what a user who selected something and hit find almost always
    // wants.
    function openFind(query) {
        searchLoader.initialQuery = query !== undefined
                                    ? query : root.terminal.surfaceModel.selectedText
        searchLoader.active = true
    }

    function closeFind() {
        searchLoader.active = false
        root.terminal.clearSearch()
        if (rendererLoader.item)
            rendererLoader.item.forceActiveFocus()
    }

    // ESC[2J leaves the scrollback intact, matching what the shell's own clear
    // command does. Resetting the whole terminal is a separate, heavier action.
    function clearScreen() { root.terminal.feedText("\u001b[H\u001b[2J") }
    function clearScrollback() { root.terminal.feedText("\u001b[3J") }

    // Style runs carry the OSC 8 link id, so a cell's link is found by walking
    // the row's runs until the one covering that column.
    function hyperlinkIdAt(row, column) {
        const allRows = root.terminal.surfaceModel.visibleLineRuns
        if (row < 0 || row >= allRows.length)
            return 0
        let start = 0
        for (const run of allRows[row]) {
            const span = run.columns
            if (column >= start && column < start + span)
                return run.hyperlinkId ? run.hyperlinkId : 0
            start += span
        }
        return 0
    }

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

    // Right-click only: every other button belongs to the renderer, which
    // handles selection and the mouse protocol.
    MouseArea {
        anchors.fill: contentArea
        acceptedButtons: Qt.RightButton
        onPressed: function(mouse) {
            const item = rendererLoader.item
            if (!item)
                return
            const local = mapToItem(item, mouse.x, mouse.y)
            root.menuHyperlinkId = root.hyperlinkIdAt(item.rowAtPosition(local.y),
                                                      item.columnAtPosition(local.x))
            contextMenu.popup()
        }
    }

    Menu {
        id: contextMenu

        readonly property bool hasSelection: root.terminal.surfaceModel.hasSelection
        readonly property string linkUrl: root.menuHyperlinkId > 0
                                          ? root.terminal.hyperlinkUrl(root.menuHyperlinkId) : ""

        MenuItem {
            text: qsTr("Copy")
            shortcut: "⌘C"
            enabled: contextMenu.hasSelection
            onTriggered: root.copySelection()
        }
        MenuItem {
            text: qsTr("Paste")
            shortcut: "⌘V"
            enabled: clipboardBridge.clipboardText().length > 0
            onTriggered: root.pasteClipboard()
        }
        MenuItem {
            text: qsTr("Select All")
            shortcut: "⌘A"
            onTriggered: root.terminal.selectAll()
        }

        QQC.MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.border } }

        MenuItem {
            text: qsTr("Find…")
            shortcut: "⌘F"
            onTriggered: root.openFind()
        }
        MenuItem {
            text: qsTr("Find Selection")
            shortcut: "⌘E"
            enabled: contextMenu.hasSelection
            onTriggered: root.openFind()
        }

        QQC.MenuSeparator {
            contentItem: Rectangle { implicitHeight: 1; color: Theme.border }
            visible: contextMenu.linkUrl.length > 0
            height: visible ? implicitHeight : 0
        }

        MenuItem {
            text: qsTr("Open Link")
            visible: contextMenu.linkUrl.length > 0
            height: visible ? implicitHeight : 0
            onTriggered: Qt.openUrlExternally(contextMenu.linkUrl)
        }
        MenuItem {
            text: qsTr("Copy Link Address")
            visible: contextMenu.linkUrl.length > 0
            height: visible ? implicitHeight : 0
            onTriggered: clipboardBridge.copyText(contextMenu.linkUrl)
        }

        QQC.MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.border } }

        MenuItem {
            text: qsTr("Clear Screen")
            shortcut: "⌘K"
            onTriggered: root.clearScreen()
        }
        MenuItem {
            text: qsTr("Clear Scrollback")
            onTriggered: root.clearScrollback()
        }
        MenuItem {
            text: qsTr("Reset Terminal")
            onTriggered: root.terminal.clear()
        }

        QQC.MenuSeparator { contentItem: Rectangle { implicitHeight: 1; color: Theme.border } }

        MenuItem {
            text: qsTr("New Tab")
            shortcut: "⌘T"
            onTriggered: root.newTabRequested()
        }
        MenuItem {
            text: qsTr("Close Tab")
            shortcut: "⌘W"
            onTriggered: root.closeTabRequested()
        }
    }

    // Loaded on demand: the find bar is absent most of the time, and keeping it
    // unloaded also keeps its Timer and bindings from running.
    Loader {
        id: searchLoader
        property string initialQuery: ""
        active: false
        anchors.right: contentArea.right
        anchors.top: contentArea.top
        anchors.rightMargin: Theme.space3
        anchors.topMargin: Theme.space2
        z: 10

        sourceComponent: SearchBar {
            terminal: root.terminal
            initialQuery: searchLoader.initialQuery
            onCloseRequested: root.closeFind()
            Component.onCompleted: activate()
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
        sequence: StandardKey.Find
        enabled: rendererLoader.activeFocus || searchLoader.active
        onActivated: root.openFind()
    }

    Shortcut {
        sequence: "Ctrl+E"
        enabled: rendererLoader.activeFocus
        onActivated: root.openFind()
    }

    Shortcut {
        sequence: "Ctrl+K"
        enabled: rendererLoader.activeFocus
        onActivated: root.clearScreen()
    }

    Shortcut {
        sequence: "Ctrl+T"
        enabled: rendererLoader.activeFocus
        onActivated: root.newTabRequested()
    }

    Shortcut {
        sequence: StandardKey.Close
        enabled: rendererLoader.activeFocus
        onActivated: root.closeTabRequested()
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
