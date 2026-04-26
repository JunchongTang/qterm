pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QTerm 1.0

ApplicationWindow {
    id: root

    required property var terminal
    required property var clipboardBridge
    required property var themeHelper
    property bool isDarkTheme: true
    property bool useSGRenderer: false
    property color terminalBgColor: root.isDarkTheme ? "#0b1016" : "#fafafa"
    property real bellFlashOpacity: 0.0
    property real cursorBlinkOpacity: 1.0
    property string terminalFontFamily: "Menlo"
    property int terminalFontPixelSize: 18
    property int bellCount: 0
    property int copyCount: 0
    property string lastBackendError: ""
    property string statusNote: "Local PTY ready"
    readonly property string sessionStateText: root.sessionStateLabel(root.terminal.session ? root.terminal.session.state : -1)
    readonly property string geometryText: root.terminal.surfaceModel.columns + " x " + root.terminal.surfaceModel.rows
    readonly property string cursorText: "Cursor " + (root.terminal.surfaceModel.cursorRow + 1) + ":" + (root.terminal.surfaceModel.cursorColumn + 1)
    readonly property string selectionText: root.terminal.surfaceModel.hasSelection
        ? (root.terminal.surfaceModel.selectionVisible
            ? "Selection " + root.terminal.surfaceModel.selectedText.length + " chars"
            : "Selection offscreen " + root.terminal.surfaceModel.selectedText.length + " chars")
        : "Selection none"
    readonly property string activityText: root.lastBackendError.length > 0
        ? "Error: " + root.lastBackendError
        : "[" + (root.useSGRenderer ? "QSG" : "Painted") + "]  " + root.statusNote + "  Bell " + root.bellCount + "  Copy " + root.copyCount

    width: 1120
    height: 760
    // minimumWidth: 760
    // minimumHeight: 520
    visible: true
    color: "#f5f5f7"
    title: root.terminal.title.length > 0 ? root.terminal.title : "QTerm"

    function sessionStateLabel(state) {
        switch (state) {
        case 0:
            return "Closed"
        case 1:
            return "Opening"
        case 2:
            return "Open"
        case 3:
            return "Closing"
        case 4:
            return "Error"
        default:
            return "No session"
        }
    }

    Connections {
        target: root.terminal

        function onBell() {
            bellFlash.stop()
            root.bellFlashOpacity = 0.35
            root.bellCount += 1
            root.statusNote = "Bell received"
            bellFlash.start()
        }
    }

    Connections {
        target: root.terminal.session

        function onErrorOccurred(message) {
            root.lastBackendError = message
        }

        function onStateChanged() {
            if (root.terminal.session && root.terminal.session.state !== 4)
                root.lastBackendError = ""
        }
    }

    // ── 剪贴板快捷键（应用层定义，widget 不感知）────────────────────────────
    // Cmd+C: 复制选区。若无选区，通知 QML 层（可自定义：发响铃/忽略）
    Shortcut {
        sequence: StandardKey.Copy
        enabled: terminalView.activeFocus
        onActivated: {
            const text = root.terminal.surfaceModel.selectedText
            if (text.length > 0) {
                root.clipboardBridge.copyText(text)
                root.copyCount += 1
                root.statusNote = "Copied selection"
            }
        }
    }

    // Cmd+V: 粘贴系统剪贴板内容到终端
    Shortcut {
        sequence: StandardKey.Paste
        enabled: terminalView.activeFocus
        onActivated: {
            const text = root.clipboardBridge.clipboardText()
            if (text.length > 0)
                root.terminal.sendPaste(text)
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

    SequentialAnimation {
        id: cursorBlink
        running: root.terminal.surfaceModel.cursorVisible && terminalView.activeFocus
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

    background: Rectangle {
        color: "#f5f5f7"

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#fafafc" }
                GradientStop { position: 1.0; color: "#eef1f5" }
            }
        }
    }

    menuBar: TermMenuBar {
        isDark: root.isDarkTheme
        isSGRenderer: root.useSGRenderer
        onDarkThemeRequested: {
            root.themeHelper.applyDarkTheme(terminalView.item)
            root.isDarkTheme = true
        }
        onLightThemeRequested: {
            root.themeHelper.applyLightTheme(terminalView.item)
            root.isDarkTheme = false
        }
        onSgRendererRequested: {
            root.useSGRenderer = true
        }
        onPaintedRendererRequested: {
            root.useSGRenderer = false
        }
    }

    footer: TermStatusBar {
        titleText: root.title
        sessionStateText: root.sessionStateText
        geometryText: root.geometryText
        cursorText: root.cursorText
        selectionText: root.selectionText
        noteText: root.activityText
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 28
        spacing: 20

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                Layout.fillWidth: true
                color: "#6b7280"
                font.pixelSize: 12
                font.weight: Font.DemiBold
                font.letterSpacing: 2
                text: "QTERM"
            }

            Text {
                Layout.fillWidth: true
                color: "#111827"
                font.pixelSize: 34
                font.weight: Font.DemiBold
                text: "Local PTY Demo"
            }

            Text {
                Layout.fillWidth: true
                color: "#667085"
                font.pixelSize: 15
                wrapMode: Text.WordWrap
                text: "A minimal qterm shell surface. Type directly into the terminal. Use printf '\\a' to validate bell handling."
            }
        }

        Rectangle {
            id: terminalSurface

            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 20
            color: root.terminalBgColor
            border.width: 1
            border.color: "#d8dde6"
            clip: true

            Rectangle {
                anchors.fill: parent
                color: "#d9ffe3"
                opacity: root.bellFlashOpacity
            }

            // ── Renderer components ───────────────────────────────────────────

            Component {
                id: paintedRendererComponent
                QTermQuickPaintedItem {
                    focus: true
                    terminal: root.terminal
                    fontFamily: root.terminalFontFamily
                    fontPixelSize: root.terminalFontPixelSize
                    cursorOpacity: root.cursorBlinkOpacity

                    onWheelScrolled: scrollOffset => {
                        root.statusNote = scrollOffset > 0
                            ? "Scrolled " + scrollOffset + " rows above bottom"
                            : "Returned to bottom"
                    }
                    onCopyRequested: text => {
                        root.clipboardBridge.copyText(text)
                        root.copyCount += 1
                        root.statusNote = text.length > 0 ? "Copied selection" : "Copy requested"
                    }
                    onHyperlinkActivated: url => {
                        root.statusNote = "Opening: " + url
                        Qt.openUrlExternally(url)
                    }
                }
            }

            Component {
                id: sgRendererComponent
                QTermQuickItem {
                    focus: true
                    terminal: root.terminal
                    fontFamily: root.terminalFontFamily
                    fontPixelSize: root.terminalFontPixelSize
                    cursorOpacity: root.cursorBlinkOpacity

                    onWheelScrolled: angleDelta => {
                        root.statusNote = angleDelta > 0
                            ? "Scrolled above bottom (QSG)"
                            : "Returned to bottom (QSG)"
                    }
                    onCopyRequested: {
                        const text = root.terminal.surfaceModel.selectedText
                        root.clipboardBridge.copyText(text)
                        root.copyCount += 1
                        root.statusNote = text.length > 0 ? "Copied selection (QSG)" : "Copy requested (QSG)"
                    }
                    onHyperlinkActivated: url => {
                        root.statusNote = "Opening: " + url
                        Qt.openUrlExternally(url)
                    }
                }
            }

            Loader {
                id: terminalView

                anchors.fill: parent
                anchors.margins: 26
                focus: true
                sourceComponent: root.useSGRenderer ? sgRendererComponent : paintedRendererComponent

                onLoaded: {
                    // Re-apply theme after renderer swap
                    if (root.isDarkTheme)
                        root.themeHelper.applyDarkTheme(item)
                    else
                        root.themeHelper.applyLightTheme(item)
                }
            }

            ScrollBar {
                id: viewportScrollBar

                anchors.top: terminalView.top
                anchors.right: parent.right
                anchors.bottom: terminalView.bottom
                anchors.rightMargin: 8
                orientation: Qt.Vertical
                policy: root.terminal.maxScrollOffset > 0 ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                active: hovered || pressed || root.terminal.maxScrollOffset > 0
                size: terminalView.item ? terminalView.item.scrollSize : 1.0
                position: terminalView.item ? terminalView.item.scrollPosition : 0.0
                hoverEnabled: true

                onPositionChanged: {
                    if (pressed && terminalView.item)
                        terminalView.item.scrollPosition = position
                }

                contentItem: Rectangle {
                    implicitWidth: 10
                    radius: width / 2
                    color: viewportScrollBar.pressed ? "#d7fbe0" : (viewportScrollBar.hovered ? "#a8c4db" : "#6c7b8b")
                    opacity: viewportScrollBar.policy === ScrollBar.AlwaysOff ? 0.0 : 0.92
                }

                background: Rectangle {
                    implicitWidth: 10
                    radius: width / 2
                    color: "#18212b"
                    opacity: viewportScrollBar.policy === ScrollBar.AlwaysOff ? 0.0 : 0.7
                }
            }
        }
    }

}