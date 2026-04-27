pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// TabManager — tab bar + per-tab terminal sessions.
// Tabs are tracked in a JS array; TerminalTab items are parented to tabStack.
Item {
    id: root

    // ── Public state ──────────────────────────────────────────────────────────

    property int currentIndex: -1

    // Read-only array of live TerminalTab instances (triggers bindings on assign)
    property var tabs: []

    // Convenience accessor used by Main.qml
    function activeTab() {
        return (currentIndex >= 0 && currentIndex < tabs.length)
            ? tabs[currentIndex]
            : null
    }

    // ── Component template ────────────────────────────────────────────────────

    Component {
        id: tabComponent
        TerminalTab { visible: false }
    }

    // ── Layout ────────────────────────────────────────────────────────────────

    // ── Tab bar ───────────────────────────────────────────────────────────────

    Rectangle {
        id: tabBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 42
        color: "#0d1117"

            Row {
                id: tabButtonRow
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                spacing: 0

                Repeater {
                    model: root.tabs.length

                    delegate: Rectangle {
                        id: tabBtn
                        required property int index

                        readonly property var tabRef: root.tabs[index] || null

                        width: Math.min(200, Math.max(100,
                            tabButtonRow.width / Math.max(root.tabs.length, 1)))
                        height: tabBar.height
                        color: root.currentIndex === index
                            ? "#21262d"
                            : (tabBtnMouse.containsMouse ? "#161b22" : "#0d1117")

                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 2
                            color: "#388bfd"
                            visible: root.currentIndex === tabBtn.index
                        }

                        // Tab click area — declared BEFORE RowLayout so close btn has higher Z
                        MouseArea {
                            id: tabBtnMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.currentIndex = tabBtn.index
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 6
                            spacing: 4

                            Text {
                                Layout.fillWidth: true
                                text: tabBtn.tabRef ? tabBtn.tabRef.tabTitle : ""
                                color: root.currentIndex === tabBtn.index ? "#c9d1d9" : "#8b949e"
                                font.pixelSize: 13
                                elide: Text.ElideRight
                            }

                            Rectangle {
                                width: 18
                                height: 18
                                radius: 9
                                color: closeMouse.containsMouse ? "#30363d" : "transparent"
                                visible: tabBtnMouse.containsMouse || root.currentIndex === tabBtn.index

                                Text {
                                    anchors.centerIn: parent
                                    text: "\u00d7"
                                    color: "#8b949e"
                                    font.pixelSize: 14
                                }

                                MouseArea {
                                    id: closeMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: mouse => {
                                        mouse.accepted = true
                                        root.removeTab(tabBtn.index)
                                    }
                                }
                            }
                        }
                    }
                }

                // Add (+) button
                Rectangle {
                    width: 42
                    height: tabBar.height
                    color: addMouse.containsMouse ? "#161b22" : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "+"
                        color: "#8b949e"
                        font.pixelSize: 20
                        font.weight: Font.Light
                    }

                    MouseArea {
                        id: addMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: newSessionDialog.open()
                    }
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: "#21262d"
            }
        }

    // ── Content area ───────────────────────────────────────────────────────

    Item {
        id: tabStack
        anchors.top: tabBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        // Empty state
        Column {
            anchors.centerIn: parent
            spacing: 16
            visible: root.tabs.length === 0

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("No open sessions")
                color: "#8b949e"
                font.pixelSize: 18
            }

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 160
                height: 38
                radius: 8
                color: emptyBtnMouse.containsMouse ? "#1f6feb" : "#388bfd"

                Text {
                    anchors.centerIn: parent
                    text: qsTr("New Session")
                    color: "#ffffff"
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }

                MouseArea {
                    id: emptyBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: newSessionDialog.open()
                }
            }
        }
    }

    // ── New session dialog ────────────────────────────────────────────────────

    NewSessionDialog {
        id: newSessionDialog
        onSessionRequested: config => root.addTab(config)
    }

    // ── Public API ────────────────────────────────────────────────────────────

    function addTab(config) {
        const tab = tabComponent.createObject(tabStack, { sessionConfig: config })
        tab.anchors.fill = tabStack
        const newTabs = tabs.concat([tab])
        tabs = newTabs
        currentIndex = tabs.length - 1
        updateVisibility()
    }

    function removeTab(index) {
        if (index < 0 || index >= tabs.length)
            return
        const tab = tabs[index]
        const newTabs = tabs.filter((_, i) => i !== index)
        tabs = newTabs
        tab.destroy()
        if (currentIndex >= tabs.length)
            currentIndex = Math.max(0, tabs.length - 1)
        updateVisibility()
    }

    function updateVisibility() {
        for (let i = 0; i < tabs.length; i++)
            tabs[i].visible = (i === currentIndex)
    }

    onCurrentIndexChanged: updateVisibility()
}
