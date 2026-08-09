pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuickTerminal

// Tab strip plus a stack of terminal sessions. Sessions are created once and
// kept alive while their tab exists; the StackLayout controls visibility.
Item {
    id: root

    property var tabs: []
    property int currentIndex: -1

    readonly property var activeTab: currentIndex >= 0 && currentIndex < tabs.length
                                     ? tabs[currentIndex] : null

    function addTab(config) {
        const tab = tabComponent.createObject(stack, { sessionConfig: config })
        tab.newTabRequested.connect(() => newSessionDialog.open())
        tab.closeTabRequested.connect(() => root.closeTab(root.tabs.indexOf(tab)))
        tabs = tabs.concat([tab])
        currentIndex = tabs.length - 1
    }

    function closeTab(index) {
        if (index < 0 || index >= tabs.length)
            return
        const closed = tabs[index]
        tabs = tabs.filter((_, i) => i !== index)
        if (index < currentIndex)
            currentIndex -= 1
        else if (currentIndex >= tabs.length)
            currentIndex = tabs.length - 1
        closed.destroy()
    }

    Component {
        id: tabComponent
        TerminalTab {}
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            id: tabBar
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            color: Theme.background

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space2
                anchors.rightMargin: Theme.space2
                spacing: Theme.space1

                Repeater {
                    model: root.tabs.length

                    delegate: Rectangle {
                        id: tab

                        required property int index

                        readonly property var tabRef: root.tabs[index] ?? null
                        readonly property bool active: root.currentIndex === index

                        Layout.preferredWidth: Math.min(200, tabRow.implicitWidth + Theme.space2)
                        Layout.preferredHeight: 28
                        radius: Theme.radiusMd
                        color: active ? Theme.muted
                                      : hoverHandler.hovered ? Theme.alpha(Theme.muted, 0.5)
                                                             : "transparent"
                        border.width: active ? 1 : 0
                        border.color: Theme.border

                        Behavior on color { ColorAnimation { duration: Theme.durFast } }

                        HoverHandler { id: hoverHandler }

                        TapHandler {
                            onTapped: root.currentIndex = tab.index
                        }

                        RowLayout {
                            id: tabRow
                            anchors.fill: parent
                            anchors.leftMargin: Theme.space2_5
                            anchors.rightMargin: Theme.space1
                            spacing: Theme.space1

                            Text {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 40
                                text: tab.tabRef ? tab.tabRef.tabTitle : ""
                                color: tab.active ? Theme.foreground : Theme.mutedForeground
                                font.pixelSize: Theme.textXs
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }

                            IconButton {
                                Layout.preferredWidth: 20
                                Layout.preferredHeight: 20
                                iconName: "x"
                                size: IconButton.Small
                                opacity: tab.active || hoverHandler.hovered ? 1.0 : 0.0
                                onClicked: root.closeTab(tab.index)

                                Behavior on opacity { NumberAnimation { duration: Theme.durFast } }
                            }
                        }
                    }
                }

                IconButton {
                    iconName: "plus"
                    onClicked: newSessionDialog.open()
                }

                Item { Layout.fillWidth: true }

                // Renderer switch: both frontends drive the same session, so
                // flipping this compares them on identical terminal state.
                Button {
                    variant: Button.Outline
                    size: Button.Sm
                    iconName: "layers"
                    text: AppState.rendererName
                    onClicked: AppState.useSceneGraphRenderer = !AppState.useSceneGraphRenderer
                }

                IconButton {
                    iconName: Theme.dark ? "sun" : "moon"
                    onClicked: Theme.dark = !Theme.dark
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.border
            }
        }

        StackLayout {
            id: stack
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.currentIndex

            visible: root.tabs.length > 0
        }

        // Empty state shown before the first session is created.
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.tabs.length === 0

            ColumnLayout {
                anchors.centerIn: parent
                spacing: Theme.space2

                Icon {
                    Layout.alignment: Qt.AlignHCenter
                    name: "terminal"
                    size: 32
                    color: Theme.mutedForeground
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("No open sessions")
                    color: Theme.foreground
                    font.pixelSize: Theme.textSm
                    font.weight: Font.Medium
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: qsTr("Start a local shell, serial or telnet session.")
                    color: Theme.mutedForeground
                    font.pixelSize: Theme.textXs
                }
                Button {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: Theme.space2
                    text: qsTr("New Session")
                    iconName: "plus"
                    onClicked: newSessionDialog.open()
                }
            }
        }
    }

    NewSessionDialog {
        id: newSessionDialog
        onSessionRequested: config => root.addTab(config)
    }

}
