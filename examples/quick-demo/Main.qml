pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QTerm 1.0

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
    property real selectionDragX: 0
    property real selectionDragY: 0
    property int selectionAutoScrollDirection: 0
    property int clickStreak: 0
    property int lastClickRow: -1
    property int lastClickColumn: -1
    property bool suppressSelectionRelease: false
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
        : root.statusNote + "  Bell " + root.bellCount + "  Copy " + root.copyCount

    width: 1120
    height: 760
    // minimumWidth: 760
    // minimumHeight: 520
    visible: true
    color: "#f5f5f7"
    title: root.terminal.title.length > 0 ? root.terminal.title : "QTerm"

    function updateSelectionAt(x, y) {
        if (root.selectionAnchorRow < 0 || root.selectionAnchorColumn < 0)
            return

        const row = terminalView.rowAtPosition(y)
        const column = Math.min(root.terminal.surfaceModel.columns, terminalView.columnAtPosition(x) + 1)
        root.terminal.setSelectionRange(root.selectionAnchorRow,
                                        root.selectionAnchorColumn,
                                        row,
                                        column)
    }

    function updateSelectionAutoScroll(x, y, pressed) {
        root.selectionDragX = x
        root.selectionDragY = y

        if (!pressed || root.suppressSelectionRelease || root.selectionAnchorRow < 0 || root.selectionAnchorColumn < 0) {
            root.selectionAutoScrollDirection = 0
            selectionAutoScrollTimer.stop()
            return
        }

        if (y < 0) {
            root.selectionAutoScrollDirection = 1
        } else if (y > terminalView.height) {
            root.selectionAutoScrollDirection = -1
        } else {
            root.selectionAutoScrollDirection = 0
        }

        if (root.selectionAutoScrollDirection === 0) {
            selectionAutoScrollTimer.stop()
            return
        }

        if (!selectionAutoScrollTimer.running)
            selectionAutoScrollTimer.start()
    }

    function selectionAutoScrollRowsPerTick() {
        if (root.selectionAutoScrollDirection === 0)
            return 0

        const overflow = root.selectionAutoScrollDirection > 0
            ? Math.max(0, -root.selectionDragY)
            : Math.max(0, root.selectionDragY - terminalView.height)
        const rowHeight = Math.max(1, terminalView.cellHeight)
        return root.selectionAutoScrollDirection * Math.max(1, Math.ceil(overflow / rowHeight))
    }

    function scrollBarThumbSize() {
        const totalRows = root.terminal.surfaceModel.rows + root.terminal.maxScrollOffset
        if (totalRows <= 0)
            return 1.0

        return Math.max(0.08, Math.min(1.0, root.terminal.surfaceModel.rows / totalRows))
    }

    function scrollBarPosition() {
        if (root.terminal.maxScrollOffset <= 0)
            return 0.0

        const trackSpan = Math.max(0.0, 1.0 - root.scrollBarThumbSize())
        if (trackSpan <= 0.0)
            return 0.0

        return trackSpan * ((root.terminal.maxScrollOffset - root.terminal.scrollOffset) / root.terminal.maxScrollOffset)
    }

    function scrollToOffset(targetOffset) {
        const boundedOffset = Math.max(0, Math.min(root.terminal.maxScrollOffset, Math.round(targetOffset)))
        const deltaRows = boundedOffset - root.terminal.scrollOffset
        if (deltaRows !== 0)
            root.terminal.scrollByLines(deltaRows)
    }

    function scrollOffsetFromBarPosition(position, size) {
        if (root.terminal.maxScrollOffset <= 0)
            return 0

        const trackSpan = Math.max(0.0, 1.0 - size)
        if (trackSpan <= 0.0)
            return 0

        const normalized = Math.max(0.0, Math.min(1.0, position / trackSpan))
        return root.terminal.maxScrollOffset * (1.0 - normalized)
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

    Timer {
        id: selectionAutoScrollTimer

        interval: 35
        repeat: true
        onTriggered: {
            const deltaRows = root.selectionAutoScrollRowsPerTick()
            if (deltaRows === 0) {
                stop()
                return
            }

            root.terminal.scrollByLines(deltaRows)
            root.updateSelectionAt(root.selectionDragX, root.selectionDragY)
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

            QTermQuickItem {
                id: terminalView

                anchors.fill: parent
                anchors.margins: 26
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

                Keys.onPressed: event => {
                    if (event.matches(StandardKey.Copy)) {
                        const selectedText = root.terminal.surfaceModel.selectedText
                        root.clipboardBridge.copyText(selectedText)
                        root.copyCount += 1
                        root.statusNote = selectedText.length > 0 ? "Copied selection" : "Copy requested"
                        event.accepted = true
                        return
                    }

                    if (root.terminal.scrollOffset > 0)
                        root.terminal.scrollToBottom()

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
                        const row = terminalView.rowAtPosition(mouse.y)
                        const column = terminalView.columnAtPosition(mouse.x)
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
                            root.selectionAutoScrollDirection = 0
                            selectionAutoScrollTimer.stop()
                            root.terminal.selectLogicalLineAt(row)
                            root.statusNote = root.terminal.surfaceModel.hasSelection ? "Logical line selected" : "Selection cleared"
                            return
                        }

                        root.selectionAnchorRow = row
                        root.selectionAnchorColumn = column
                        root.selectionDragX = mouse.x
                        root.selectionDragY = mouse.y
                        root.suppressSelectionRelease = false
                        root.selectionAutoScrollDirection = 0
                        selectionAutoScrollTimer.stop()
                        root.terminal.clearSelection()
                        root.statusNote = "Selecting"
                    }

                    onDoubleClicked: mouse => {
                        terminalView.forceActiveFocus()
                        root.selectionAnchorRow = -1
                        root.selectionAnchorColumn = -1
                        root.selectionAutoScrollDirection = 0
                        selectionAutoScrollTimer.stop()
                        root.terminal.selectWordAt(terminalView.rowAtPosition(mouse.y), terminalView.columnAtPosition(mouse.x))
                        root.statusNote = root.terminal.surfaceModel.hasSelection ? "Word selected" : "Selection cleared"
                    }

                    onPositionChanged: mouse => {
                        if (pressed && !root.suppressSelectionRelease) {
                            root.updateSelectionAt(mouse.x, mouse.y)
                            root.updateSelectionAutoScroll(mouse.x, mouse.y, pressed)
                        }
                    }

                    onReleased: mouse => {
                        root.selectionAutoScrollDirection = 0
                        selectionAutoScrollTimer.stop()
                        if (root.suppressSelectionRelease) {
                            root.suppressSelectionRelease = false
                            return
                        }

                        root.updateSelectionAt(mouse.x, mouse.y)
                        root.selectionAnchorRow = -1
                        root.selectionAnchorColumn = -1
                        root.statusNote = root.terminal.surfaceModel.hasSelection ? "Selection updated" : "Selection cleared"
                    }

                    onCanceled: {
                        root.selectionAutoScrollDirection = 0
                        selectionAutoScrollTimer.stop()
                        root.selectionAnchorRow = -1
                        root.selectionAnchorColumn = -1
                        root.suppressSelectionRelease = false
                    }
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
                size: root.scrollBarThumbSize()
                position: root.scrollBarPosition()
                hoverEnabled: true

                onPositionChanged: {
                    if (pressed)
                        root.scrollToOffset(root.scrollOffsetFromBarPosition(position, size))
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