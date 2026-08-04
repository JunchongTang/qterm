pragma Singleton

import QtQuick

// Demo-wide switches driven from the tab bar.
QtObject {
    // Chooses between the two renderer frontends so both can be exercised
    // against the same session: QTermQuickItem (scene graph) when true,
    // QTermQuickPaintedItem (QPainter) when false.
    property bool useSceneGraphRenderer: true

    readonly property string rendererName: useSceneGraphRenderer ? qsTr("Scene Graph")
                                                                : qsTr("Painted")
}
