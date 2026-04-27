pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// PtySessionForm — configuration form for a local PTY (shell) session.
Item {
    id: root

    implicitHeight: column.implicitHeight

    readonly property var sessionConfig: ({
        type: "pty",
        label: programField.text.length > 0 ? "Shell \u2014 " + programField.text : "Shell",
        program: programField.text,
        arguments: argumentsField.text.trim().length > 0
            ? argumentsField.text.trim().split(/\s+/)
            : [],
        workingDirectory: workdirField.text
    })

    function isValid() {
        return true  // program defaults to $SHELL when empty
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 12

        // Shell program
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Label {
                text: qsTr("Shell program")
                color: "#8b949e"
                font.pixelSize: 13
                Layout.preferredWidth: 130
            }

            TextField {
                id: programField
                Layout.fillWidth: true
                placeholderText: qsTr("Default system shell ($SHELL)")
                color: "#c9d1d9"
                font.pixelSize: 13
                background: Rectangle {
                    color: "#21262d"
                    border.color: programField.activeFocus ? "#388bfd" : "#30363d"
                    radius: 6
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
                placeholderText: qsTr("-il")
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
