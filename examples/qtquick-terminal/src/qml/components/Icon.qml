import QtQuick
import QtQuick.Effects
import QtQuickTerminal

// Renders one of the bundled monochrome SVG icons tinted with `color`.
// Icons are authored as 24x24 white strokes in assets/icons/.
Item {
    id: root

    property string name: ""
    property int size: 16
    property color color: Theme.foreground

    implicitWidth: size
    implicitHeight: size
    visible: name !== ""

    Image {
        id: glyph
        anchors.fill: parent
        source: root.name !== "" ? Qt.resolvedUrl("../assets/icons/" + root.name + ".svg") : ""
        sourceSize: Qt.size(root.size * 2, root.size * 2)
        fillMode: Image.PreserveAspectFit
        visible: false
    }

    MultiEffect {
        anchors.fill: glyph
        source: glyph
        colorization: 1.0
        colorizationColor: root.color
    }
}
