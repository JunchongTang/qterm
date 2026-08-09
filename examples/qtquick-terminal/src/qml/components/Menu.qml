import QtQuick
import QtQuick.Controls.Basic as C
import QtQuickTerminal

// shadcn dropdown: popover fill, 1px border, soft shadow, subtle rise on open.
C.Menu {
    id: control

    implicitWidth: 220
    padding: Theme.space1
    margins: Theme.space2
    overlap: 0

    background: Rectangle {
        implicitWidth: 220
        radius: Theme.radiusMd
        color: Theme.popover
        border.width: 1
        border.color: Theme.border

        // Drawn as stacked rectangles rather than a MultiEffect: the menu is
        // small and short-lived, so a shader pass costs more than it saves.
        Rectangle {
            z: -1
            anchors.fill: parent
            anchors.margins: -1
            radius: parent.radius + 1
            color: Theme.alpha("#000000", Theme.dark ? 0.35 : 0.10)
        }
        Rectangle {
            z: -2
            anchors.fill: parent
            anchors.topMargin: 1
            anchors.margins: -3
            radius: parent.radius + 3
            color: Theme.alpha("#000000", Theme.dark ? 0.22 : 0.06)
        }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 90 }
        NumberAnimation { property: "scale"; from: 0.97; to: 1.0; duration: 90 }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 60 }
    }
}
