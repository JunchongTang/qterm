pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QTerm

// PtySessionForm — configuration form for a local shell session.
Item {
    id: root

    implicitHeight: column.implicitHeight

    // Shell infos populated once from the scanner.
    property var shellInfos: []

    // Resolved program path: use the selected shell's program, or fall back to
    // whatever the user typed when currentIndex is -1 (custom entry).
    readonly property string resolvedProgram: {
        const idx = shellCombo.currentIndex
        if (idx >= 0 && idx < shellInfos.length)
            return shellInfos[idx].program
        return shellCombo.editText
    }

    readonly property var sessionConfig: ({
        type: "pty",
        label: shellCombo.displayText.length > 0
            ? "Shell — " + shellCombo.displayText
            : "Shell",
        program: resolvedProgram,
        arguments: argumentsField.text.trim().length > 0
            ? argumentsField.text.trim().split(/\s+/)
            : [],
        workingDirectory: workdirField.text
    })

    function isValid() {
        return true  // empty program defaults to the platform shell
    }

    Component.onCompleted: {
        shellInfos = QTermLocalShellScanner.availableShells()
        const names = shellInfos.map(s => s.name)
        shellCombo.model = names
        shellCombo.currentIndex = names.length > 0 ? 0 : -1
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 12

        // Shell selector
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Shell")
                color: "#8b949e"
                font.pixelSize: 13
                Layout.preferredWidth: 130
            }

            ComboBox {
                id: shellCombo
                Layout.fillWidth: true
                editable: true
                font.pixelSize: 13
                contentItem: TextField {
                    text: shellCombo.editText
                    font: shellCombo.font
                    color: "#c9d1d9"
                    background: null
                    leftPadding: 8
                }
                background: Rectangle {
                    color: "#21262d"
                    border.color: shellCombo.activeFocus ? "#388bfd" : "#30363d"
                    radius: 6
                }
                popup: Popup {
                    y: shellCombo.height
                    width: shellCombo.width
                    padding: 4
                    background: Rectangle {
                        color: "#161b22"
                        border.color: "#30363d"
                        radius: 6
                    }
                    contentItem: ListView {
                        implicitHeight: contentHeight
                        model: shellCombo.delegateModel
                        clip: true
                    }
                }
                delegate: ItemDelegate {
                    required property string modelData
                    required property int index
                    width: shellCombo.width
                    contentItem: Text {
                        text: modelData
                        color: "#c9d1d9"
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.highlighted ? "#2d333b" : "transparent"
                    }
                    highlighted: shellCombo.highlightedIndex === index
                }
            }
        }

        // Arguments
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Arguments")
                color: "#8b949e"
                font.pixelSize: 13
                Layout.preferredWidth: 130
            }

            TextField {
                id: argumentsField
                Layout.fillWidth: true
                placeholderText: qsTr("Optional, e.g. -il")
                color: "#c9d1d9"
                font.pixelSize: 13
                background: Rectangle {
                    color: "#21262d"
                    border.color: argumentsField.activeFocus ? "#388bfd" : "#30363d"
                    radius: 6
                }
            }
        }

        // Working directory
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Working directory")
                color: "#8b949e"
                font.pixelSize: 13
                Layout.preferredWidth: 130
            }

            TextField {
                id: workdirField
                Layout.fillWidth: true
                placeholderText: qsTr("Default (home directory)")
                color: "#c9d1d9"
                font.pixelSize: 13
                background: Rectangle {
                    color: "#21262d"
                    border.color: workdirField.activeFocus ? "#388bfd" : "#30363d"
                    radius: 6
                }
            }
        }
    }
}
