import QtQuick
import QtQuickTerminal

// shadcn focus-visible ring: a 2px stroke of Theme.ring at 30% opacity hugging
// the parent's edge. Place inside the target background Rectangle and pass its
// radius as targetRadius.
Rectangle {
    id: root

    property bool active: false
    property real targetRadius: Theme.radiusMd

    anchors.fill: parent
    anchors.margins: -Theme.ringWidth
    radius: targetRadius <= 0 ? 0 : targetRadius + Theme.ringWidth
    color: "transparent"
    border.width: Theme.ringWidth
    border.color: Theme.alpha(Theme.ring, Theme.ringOpacity)
    visible: active
    z: -1
}
