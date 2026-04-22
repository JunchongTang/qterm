import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QTerm 1.0

ApplicationWindow {
    id: root

    title: qsTr("QTerm Demo") + (terminalItem.windowTitle ? " – " + terminalItem.windowTitle : "")
    width: 900
    height: 600
    visible: true
    color: "#1e1e1e"

    // Forward title changes from the terminal
    property string windowTitle: ""

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ── Terminal area ────────────────────────────────────────────────────
        TerminalItem {
            id: terminalItem
            Layout.fillWidth: true
            Layout.fillHeight: true

            session: termSession

            // Keep the session's scroll offset in sync
            scrollOffset: termSession ? termSession.scrollOffset : 0

            // Title changes emitted via QTermSession::titleChanged are wired
            // through the context property; surface them to the window title.
            Connections {
                target: termSession
                function onTitleChanged(title) {
                    root.windowTitle = title
                }
                function onFinished(exitCode) {
                    Qt.quit()
                }
            }

            // Request focus on startup
            Component.onCompleted: forceActiveFocus()
        }

        // ── Scrollbar ────────────────────────────────────────────────────────
        ScrollBar {
            id: scrollBar
            Layout.fillHeight: true
            policy: ScrollBar.AlwaysOn
            orientation: Qt.Vertical

            // Map scrollback lines to [0, 1] range (inverted: 0 = top / max scrollback)
            readonly property int maxOff: termSession ? termSession.maxScrollOffset : 0
            readonly property int curOff: termSession ? termSession.scrollOffset    : 0

            size: maxOff > 0 ? 1.0 / (maxOff + 1) : 1.0
            position: maxOff > 0 ? (maxOff - curOff) / maxOff * (1.0 - size) : 0.0

            onPositionChanged: {
                if (!termSession || maxOff <= 0)
                    return
                // Convert scrollbar position back to a scroll offset
                const newOff = Math.round(maxOff - position * maxOff / (1.0 - size))
                if (newOff !== curOff)
                    termSession.setScrollOffset(newOff)
            }
        }
    }
}
