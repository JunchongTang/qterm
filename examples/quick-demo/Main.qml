pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root

    required property var terminal
    required property var clipboardBridge
    property real bellFlashOpacity: 0.0
    property real cursorBlinkOpacity: 1.0
    property string terminalFontFamily: "Menlo"
    property int terminalFontPixelSize: 18
    property int selectionAnchorRow: -1
    property int selectionAnchorColumn: -1
    property int clickStreak: 0
    property int lastClickRow: -1
    property int lastClickColumn: -1
    property bool suppressSelectionRelease: false
    property int bellCount: 0
    property int copyCount: 0
    property string lastBackendError: ""
    property string statusNote: "Local PTY ready"
    readonly property var ansiPalette: [
        "#10151c", "#ff5f56", "#27c93f", "#ffbd2e",
        "#4f8cff", "#c678dd", "#56b6c2", "#dce7f3",
        "#5b6574", "#ff8f88", "#58d26a", "#ffd866",
        "#7aa2ff", "#d7a6ff", "#7dd3d8", "#f5fbff"
    ]
    readonly property string sessionStateText: root.sessionStateLabel(root.terminal.session ? root.terminal.session.state : -1)
    readonly property string geometryText: root.terminal.surfaceModel.columns + " x " + root.terminal.surfaceModel.rows
    readonly property string cursorText: "Cursor " + (root.terminal.surfaceModel.cursorRow + 1) + ":" + (root.terminal.surfaceModel.cursorColumn + 1)
    readonly property string selectionText: root.terminal.surfaceModel.hasSelection
        ? "Selection " + root.terminal.surfaceModel.selectedText.length + " chars"
        : "Selection none"
    readonly property string activityText: root.lastBackendError.length > 0
        ? "Error: " + root.lastBackendError
        : root.statusNote + "  Bell " + root.bellCount + "  Copy " + root.copyCount

    width: 1120
    height: 760
    // minimumWidth: 760
    // minimumHeight: 520
    visible: true
    color: "#f5f5f7"
    title: root.terminal.title.length > 0 ? root.terminal.title : "QTerm"

    function updateTerminalSize() {
        const innerWidth = terminalView.width
        const innerHeight = terminalView.height
        const columns = Math.max(20, Math.floor(innerWidth / Math.max(1, terminalMetrics.averageCharacterWidth)))
        const rows = Math.max(8, Math.floor(innerHeight / Math.max(1, terminalMetrics.height)))
        root.terminal.setTerminalSize(columns, rows)
    }

    function cellColumnForX(x) {
        return Math.max(0, Math.min(root.terminal.surfaceModel.columns, Math.floor(x / Math.max(1, terminalMetrics.averageCharacterWidth))))
    }

    function cellRowForY(y) {
        return Math.max(0, Math.min(root.terminal.surfaceModel.rows - 1, Math.floor(y / Math.max(1, terminalMetrics.height))))
    }

    function updateSelectionAt(x, y) {
        if (root.selectionAnchorRow < 0 || root.selectionAnchorColumn < 0)
            return

        const row = root.cellRowForY(y)
        const column = Math.min(root.terminal.surfaceModel.columns, root.cellColumnForX(x) + 1)
        root.terminal.setSelectionRange(root.selectionAnchorRow,
                                        root.selectionAnchorColumn,
                                        row,
                                        column)
    }

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

    function hex2(value) {
        const clamped = Math.max(0, Math.min(255, value))
        return clamped.toString(16).padStart(2, "0")
    }

    function colorFromRgb(rgb) {
        if (rgb === undefined || rgb < 0)
            return "transparent"

        const red = (rgb >> 16) & 0xff
        const green = (rgb >> 8) & 0xff
        const blue = rgb & 0xff
        return "#" + hex2(red) + hex2(green) + hex2(blue)
    }

    function colorFromPaletteIndex(index) {
        if (index < 0)
            return "transparent"
        if (index < root.ansiPalette.length)
            return root.ansiPalette[index]
        if (index >= 232) {
            const level = 8 + (index - 232) * 10
            return colorFromRgb((level << 16) | (level << 8) | level)
        }

        const cubeIndex = Math.max(0, index - 16)
        const redIndex = Math.floor(cubeIndex / 36) % 6
        const greenIndex = Math.floor(cubeIndex / 6) % 6
        const blueIndex = cubeIndex % 6
        const componentValues = [0, 95, 135, 175, 215, 255]
        return colorFromRgb((componentValues[redIndex] << 16) |
                            (componentValues[greenIndex] << 8) |
                            componentValues[blueIndex])
    }

    function foregroundColor(index, rgb) {
        return rgb >= 0 ? colorFromRgb(rgb) : (index >= 0 ? colorFromPaletteIndex(index) : "#d2f7d0")
    }

    function backgroundColor(index, rgb) {
        return rgb >= 0 ? colorFromRgb(rgb) : colorFromPaletteIndex(index)
    }

    function effectiveForegroundColor(run) {
        if (run.inverse)
            return backgroundColor(run.backgroundIndex, run.backgroundRgb) === "transparent"
                ? "#0a0f15"
                : backgroundColor(run.backgroundIndex, run.backgroundRgb)

        return foregroundColor(run.foregroundIndex, run.foregroundRgb)
    }

    function effectiveBackgroundColor(run) {
        if (run.inverse)
            return foregroundColor(run.foregroundIndex, run.foregroundRgb)

        return backgroundColor(run.backgroundIndex, run.backgroundRgb)
    }

    function runOpacity(run) {
        return run.dim ? 0.65 : 1.0
    }

    FontMetrics {
        id: terminalMetrics
        font.family: root.terminalFontFamily
        font.pixelSize: root.terminalFontPixelSize
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
        running: terminalCursor.visible
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

    Timer {
        id: clickResetTimer

        interval: 360
        repeat: false
        onTriggered: {
            root.clickStreak = 0
            root.lastClickRow = -1
            root.lastClickColumn = -1
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

    footer: DebugStatusBar {
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
            color: "#0b1016"
            border.width: 1
            border.color: "#d8dde6"
            clip: true

            Rectangle {
                anchors.fill: parent
                color: "#d9ffe3"
                opacity: root.bellFlashOpacity
            }

            Item {
                id: terminalView

                anchors.fill: parent
                anchors.margins: 26
                focus: true

                Component.onCompleted: root.updateTerminalSize()
                onWidthChanged: root.updateTerminalSize()
                onHeightChanged: root.updateTerminalSize()

                Keys.onPressed: event => {
                    if (event.matches(StandardKey.Copy)) {
                        const selectedText = root.terminal.surfaceModel.selectedText
                        root.clipboardBridge.copyText(selectedText)
                        root.copyCount += 1
                        root.statusNote = selectedText.length > 0 ? "Copied selection" : "Copy requested"
                        event.accepted = true
                        return
                    }

                    root.statusNote = event.text.length > 0 ? "Input forwarded" : "Control key forwarded"
                    root.terminal.sendKey(event.key, event.text)
                    event.accepted = true
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    preventStealing: true

                    onPressed: mouse => {
                        terminalView.forceActiveFocus()
                        const row = root.cellRowForY(mouse.y)
                        const column = root.cellColumnForX(mouse.x)
                        if (clickResetTimer.running && root.lastClickRow === row && root.lastClickColumn === column)
                            root.clickStreak += 1
                        else
                            root.clickStreak = 1

                        root.lastClickRow = row
                        root.lastClickColumn = column
                        clickResetTimer.restart()

                        if (root.clickStreak >= 3) {
                            root.selectionAnchorRow = -1
                            root.selectionAnchorColumn = -1
                            root.suppressSelectionRelease = true
                            root.terminal.selectLogicalLineAt(row)
                            root.statusNote = root.terminal.surfaceModel.hasSelection ? "Logical line selected" : "Selection cleared"
                            return
                        }

                        root.selectionAnchorRow = row
                        root.selectionAnchorColumn = column
                        root.suppressSelectionRelease = false
                        root.terminal.clearSelection()
                        root.statusNote = "Selecting"
                    }

                    onDoubleClicked: mouse => {
                        terminalView.forceActiveFocus()
                        root.selectionAnchorRow = -1
                        root.selectionAnchorColumn = -1
                        root.terminal.selectWordAt(root.cellRowForY(mouse.y), root.cellColumnForX(mouse.x))
                        root.statusNote = root.terminal.surfaceModel.hasSelection ? "Word selected" : "Selection cleared"
                    }

                    onPositionChanged: mouse => {
                        if (pressed && !root.suppressSelectionRelease)
                            root.updateSelectionAt(mouse.x, mouse.y)
                    }

                    onReleased: mouse => {
                        if (root.suppressSelectionRelease) {
                            root.suppressSelectionRelease = false
                            return
                        }

                        root.updateSelectionAt(mouse.x, mouse.y)
                        root.selectionAnchorRow = -1
                        root.selectionAnchorColumn = -1
                        root.statusNote = root.terminal.surfaceModel.hasSelection ? "Selection updated" : "Selection cleared"
                    }
                }

                Column {
                    anchors.fill: parent
                    spacing: 0

                    Repeater {
                        model: root.terminal.surfaceModel.visibleLineRuns

                        Item {
                            id: lineItem

                            required property var modelData
                            required property int index

                            width: terminalView.width
                            height: terminalMetrics.height

                            Rectangle {
                                readonly property int selectionStart: !root.terminal.surfaceModel.hasSelection ? 0
                                    : lineItem.index < root.terminal.surfaceModel.selectionStartRow || lineItem.index > root.terminal.surfaceModel.selectionEndRow ? 0
                                    : lineItem.index === root.terminal.surfaceModel.selectionStartRow ? root.terminal.surfaceModel.selectionStartColumn : 0
                                readonly property int selectionEnd: !root.terminal.surfaceModel.hasSelection ? 0
                                    : lineItem.index < root.terminal.surfaceModel.selectionStartRow || lineItem.index > root.terminal.surfaceModel.selectionEndRow ? 0
                                    : lineItem.index === root.terminal.surfaceModel.selectionEndRow ? root.terminal.surfaceModel.selectionEndColumn : root.terminal.surfaceModel.columns

                                visible: root.terminal.surfaceModel.hasSelection && selectionEnd > selectionStart
                                x: selectionStart * terminalMetrics.averageCharacterWidth
                                y: 0
                                width: (selectionEnd - selectionStart) * terminalMetrics.averageCharacterWidth
                                height: parent.height
                                color: "#214f76"
                            }

                            Row {
                                anchors.fill: parent
                                spacing: 0

                                Repeater {
                                    model: lineItem.modelData

                                    Rectangle {
                                        id: runRect

                                        required property var modelData

                                        width: runText.implicitWidth
                                        height: terminalMetrics.height
                                        color: root.effectiveBackgroundColor(runRect.modelData)
                                        opacity: root.runOpacity(runRect.modelData)

                                        Text {
                                            id: runText

                                            anchors.verticalCenter: parent.verticalCenter
                                            color: root.effectiveForegroundColor(runRect.modelData)
                                            text: runRect.modelData.text
                                            textFormat: Text.PlainText
                                            font.family: root.terminalFontFamily
                                            font.pixelSize: root.terminalFontPixelSize
                                            font.bold: runRect.modelData.bold
                                            font.italic: runRect.modelData.italic
                                            font.underline: runRect.modelData.underline
                                            font.strikeout: runRect.modelData.strikethrough
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                id: terminalCursor

                readonly property real cellWidth: Math.max(1, terminalMetrics.averageCharacterWidth)
                readonly property real cellHeight: Math.max(1, terminalMetrics.height)

                x: terminalView.x + root.terminal.surfaceModel.cursorColumn * cellWidth
                y: terminalView.y + root.terminal.surfaceModel.cursorRow * cellHeight
                width: Math.max(2, cellWidth)
                height: cellHeight
                radius: 1
                color: "#d7fbe0"
                opacity: root.cursorBlinkOpacity * 0.72
                visible: root.terminal.surfaceModel.cursorVisible && terminalView.activeFocus
            }
        }
    }

    component DebugStatusBar: Rectangle {
        id: statusBar

        required property string titleText
        required property string sessionStateText
        required property string geometryText
        required property string cursorText
        required property string selectionText
        required property string noteText

        implicitHeight: 42
        color: "#fbfbfd"

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 1
            color: "#d7dbe2"
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            spacing: 10

            Text {
                Layout.preferredWidth: 220
                Layout.alignment: Qt.AlignVCenter
                elide: Text.ElideRight
                color: "#111827"
                font.pixelSize: 12
                font.weight: Font.DemiBold
                text: statusBar.titleText
            }

            Text {
                Layout.alignment: Qt.AlignVCenter
                color: "#6b7280"
                font.pixelSize: 12
                text: statusBar.sessionStateText
            }

            Text {
                Layout.alignment: Qt.AlignVCenter
                color: "#6b7280"
                font.pixelSize: 12
                text: statusBar.geometryText
            }

            Text {
                Layout.alignment: Qt.AlignVCenter
                color: "#6b7280"
                font.pixelSize: 12
                text: statusBar.cursorText
            }

            Text {
                Layout.alignment: Qt.AlignVCenter
                color: "#6b7280"
                font.pixelSize: 12
                text: statusBar.selectionText
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                Layout.maximumWidth: 420
                Layout.alignment: Qt.AlignVCenter
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
                color: "#4b5563"
                font.pixelSize: 12
                text: statusBar.noteText
            }
        }
    }
}