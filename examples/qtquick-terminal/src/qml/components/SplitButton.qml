import QtQuick
import QtQuickTerminal

// Two halves sharing one outline: the left runs the default action straight
// away, the right opens a menu of the alternatives. Saves a dialog round trip
// for the case people take most of the time, without hiding the others.
// An Item wraps the Row so the shared outline can sit behind both halves
// without the positioner treating it as another child to lay out.
Item {
    id: root

    property string iconName: "plus"
    property int size: 28

    signal primaryClicked()
    // Emitted with the item the menu should be positioned under.
    signal menuRequested(Item anchorItem)

    implicitHeight: size
    implicitWidth: primaryHalf.width + 1 + menuHalf.width

    // One shared outline, so the two halves read as a single control rather
    // than two buttons that happen to be adjacent.
    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusMd
        color: "transparent"
        border.width: 1
        border.color: Theme.border
    }

    Row {
        id: halves
        anchors.fill: parent
        spacing: 0

    Item {
        id: primaryHalf
        width: root.size + 4
        height: root.size

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusMd
            // Square off the shared edge so the two halves meet cleanly.
            Rectangle {
                anchors.right: parent.right
                width: parent.radius
                height: parent.height
                color: parent.color
            }
            color: primaryHover.hovered ? Theme.muted : Theme.alpha(Theme.muted, 0)
            Behavior on color { ColorAnimation { duration: Theme.durBase } }
        }

        Icon {
            anchors.centerIn: parent
            name: root.iconName
            size: 14
            color: Theme.foreground
        }

        HoverHandler { id: primaryHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: root.primaryClicked() }
    }

    Rectangle {
        width: 1
        height: root.size - 8
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.border
    }

    Item {
        id: menuHalf
        width: 22
        height: root.size

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusMd
            Rectangle {
                anchors.left: parent.left
                width: parent.radius
                height: parent.height
                color: parent.color
            }
            color: menuHover.hovered ? Theme.muted : Theme.alpha(Theme.muted, 0)
            Behavior on color { ColorAnimation { duration: Theme.durBase } }
        }

        Icon {
            anchors.centerIn: parent
            name: "chevron-down"
            size: 10
            color: Theme.mutedForeground
        }

        HoverHandler { id: menuHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: root.menuRequested(menuHalf) }
    }
    }
}
