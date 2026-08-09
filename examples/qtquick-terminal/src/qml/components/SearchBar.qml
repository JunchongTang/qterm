import QtQuick
import QtQuick.Controls.Basic as QQC
import QtQuick.Layouts
import QtQuickTerminal

// Find bar pinned to the top-right of the terminal, the way editors and
// browsers do it -- a modal dialog would block typing into the session, and
// the search results are only meaningful while the output stays visible.
Rectangle {
    id: root

    required property var terminal
    // Pre-filled when opened with an active selection, per terminal convention.
    property string initialQuery: ""

    signal closeRequested()

    readonly property int matchCount: terminal ? terminal.searchMatchCount : 0
    readonly property int currentIndex: terminal ? terminal.searchCurrentIndex : 0
    readonly property bool noResults: field.text.length > 0 && matchCount === 0

    implicitWidth: layout.implicitWidth + Theme.space3 * 2
    implicitHeight: 40
    radius: Theme.radiusMd
    color: Theme.popover
    border.width: 1
    border.color: Theme.border

    function activate() {
        if (initialQuery.length > 0)
            field.text = initialQuery
        field.forceActiveFocus()
        field.selectAll()
        runSearch()
    }

    function runSearch() {
        if (!terminal)
            return
        if (field.text.length === 0)
            terminal.clearSearch()
        else
            terminal.search(field.text, caseToggle.checked)
    }

    // Searching rescans the whole buffer including scrollback, so it waits for
    // a pause in typing rather than running on every keystroke.
    Timer {
        id: debounce
        interval: 150
        onTriggered: root.runSearch()
    }

    RowLayout {
        id: layout
        anchors.fill: parent
        anchors.leftMargin: Theme.space3
        anchors.rightMargin: Theme.space3
        spacing: Theme.space2

        Input {
            id: field
            Layout.preferredWidth: 200
            Layout.alignment: Qt.AlignVCenter
            placeholderText: qsTr("Find")
            invalid: root.noResults
            onTextChanged: debounce.restart()
            Keys.onPressed: function(event) {
                switch (event.key) {
                case Qt.Key_Return:
                case Qt.Key_Enter:
                    if (event.modifiers & Qt.ShiftModifier)
                        root.terminal.findPrevious()
                    else
                        root.terminal.findNext()
                    event.accepted = true
                    break
                case Qt.Key_Escape:
                    root.closeRequested()
                    event.accepted = true
                    break
                }
            }
        }

        Label {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 58
            horizontalAlignment: Text.AlignRight
            color: root.noResults ? Theme.destructive : Theme.mutedForeground
            font.pixelSize: Theme.textXs
            text: root.noResults ? qsTr("none")
                                 : (root.matchCount === 0 ? ""
                                    : qsTr("%1/%2").arg(root.currentIndex).arg(root.matchCount))
        }

        IconButton {
            Layout.alignment: Qt.AlignVCenter
            iconName: "chevron-up"
            size: IconButton.Small
            enabled: root.matchCount > 0
            onClicked: root.terminal.findPrevious()
            QQC.ToolTip.text: qsTr("Previous match")
        }

        IconButton {
            Layout.alignment: Qt.AlignVCenter
            iconName: "chevron-down"
            size: IconButton.Small
            enabled: root.matchCount > 0
            onClicked: root.terminal.findNext()
            QQC.ToolTip.text: qsTr("Next match")
        }

        // Case sensitivity, styled as a toggle rather than a checkbox to keep
        // the bar compact.
        Rectangle {
            id: caseToggle
            property bool checked: false
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: 26
            implicitHeight: 24
            radius: Theme.radiusSm
            color: checked ? Theme.accent : "transparent"
            border.width: 1
            border.color: checked ? Theme.ring : "transparent"

            Text {
                anchors.centerIn: parent
                text: qsTr("Aa")
                font.pixelSize: Theme.textXs
                color: caseToggle.checked ? Theme.accentForeground : Theme.mutedForeground
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    caseToggle.checked = !caseToggle.checked
                    root.runSearch()
                }
            }
            QQC.ToolTip.visible: hoverHandler.hovered
            QQC.ToolTip.text: qsTr("Match case")
            HoverHandler { id: hoverHandler }
        }

        IconButton {
            Layout.alignment: Qt.AlignVCenter
            iconName: "x"
            size: IconButton.Small
            onClicked: root.closeRequested()
            QQC.ToolTip.text: qsTr("Close")
        }
    }
}
