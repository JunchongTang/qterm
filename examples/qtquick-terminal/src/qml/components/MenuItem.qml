import QtQuick
import QtQuick.Controls.Basic as C
import QtQuick.Layouts
import QtQuickTerminal

// One row of a Menu: label on the left, shortcut hint right-aligned in muted
// colour, accent fill on hover.
C.MenuItem {
    id: control

    // Shown right-aligned; purely a hint, the actual binding lives with the
    // window's Shortcut declarations.
    property string shortcut: ""

    implicitHeight: 30
    leftPadding: Theme.space2
    rightPadding: Theme.space2
    opacity: enabled ? 1.0 : 0.45

    // A layout rather than hand-computed widths: the hint has to keep its own
    // width and stay right-aligned while the label takes the remaining space.
    contentItem: RowLayout {
        spacing: Theme.space5

        Text {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: control.text
            color: Theme.popoverForeground
            font.pixelSize: Theme.textXs
            elide: Text.ElideRight
        }

        Text {
            Layout.alignment: Qt.AlignVCenter
            text: control.shortcut
            color: Theme.mutedForeground
            font.pixelSize: Theme.textXs
            visible: text.length > 0
        }
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.enabled && (control.hovered || control.highlighted)
               ? Theme.accent : "transparent"
    }
}
