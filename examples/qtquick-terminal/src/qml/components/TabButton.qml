import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic as C
import QtQuickTerminal

// A single trigger inside a Tabs strip; the active trigger shows a background
// pill (with a 1px input border in dark mode).
C.TabButton {
    id: control

    property string iconName: ""

    readonly property var _bar: C.TabBar.tabBar
    readonly property color _fg: (control.checked || control.hovered || control.down)
                                 ? Theme.foreground
                                 : (Theme.dark ? Theme.mutedForeground
                                               : Theme.alpha(Theme.foreground, 0.6))

    leftPadding: iconName !== "" ? Theme.space1 : Theme.space1_5
    rightPadding: Theme.space1_5
    implicitHeight: 26
    // shadcn flex-1: at the strip's content-fit width every trigger keeps its
    // own width; stretched wider, the triggers share the width equally.
    width: {
        if (!ListView.view)
            return implicitWidth
        const bar = control._bar
        const n = ListView.view.count
        if (!bar || n <= 0 || ListView.view.width <= bar._sumChildWidth)
            return implicitWidth
        return (ListView.view.width - Math.max(0, n - 1) * bar.spacing) / n
    }
    font.pixelSize: Theme.textXs
    font.weight: Font.Medium
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    opacity: enabled ? 1.0 : 0.5

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight

        RowLayout {
            id: row
            spacing: Theme.space1_5
            anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter

            Icon {
                visible: control.iconName !== ""
                name: control.iconName
                size: 14
                color: control._fg
                Behavior on color { ColorAnimation { duration: Theme.durFast } }
            }
            Text {
                visible: control.text !== ""
                text: control.text
                font: control.font
                color: control._fg
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                Behavior on color { ColorAnimation { duration: Theme.durFast } }
            }
        }
    }

    background: Item {
        Rectangle {
            id: pill
            anchors.fill: parent
            radius: Theme.radiusMd
            color: control.checked ? Theme.background : Theme.alpha(Theme.background, 0)
            border.width: control.checked && Theme.dark ? 1 : 0
            border.color: Theme.input
        }

        FocusRing { active: control.visualFocus; targetRadius: pill.radius }
    }
}
