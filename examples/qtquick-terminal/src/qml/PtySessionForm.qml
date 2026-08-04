pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QTerm
import QtQuickTerminal

ColumnLayout {
    id: root

    property bool showErrors: false
    property var shellInfos: []

    readonly property bool useCustomProgram: shellSelect.currentIndex === shellInfos.length
    readonly property bool valid: !useCustomProgram || programField.text.trim().length > 0
    readonly property string errorMessage: qsTr("Executable path is required.")

    readonly property var sessionConfig: ({
        type: "pty",
        label: shellSelect.currentText,
        program: useCustomProgram ? programField.text.trim()
                                  : (shellInfos[shellSelect.currentIndex]
                                     ? shellInfos[shellSelect.currentIndex].program : ""),
        arguments: argumentsField.text.trim().length > 0
                   ? argumentsField.text.trim().split(/\s+/) : [],
        workingDirectory: workdirField.text.trim()
    })

    spacing: Theme.space4

    Component.onCompleted: {
        shellInfos = QTermLocalShellScanner.availableShells()
        shellSelect.model = shellInfos.map(s => s.name).concat([qsTr("Custom executable...")])
        shellSelect.currentIndex = 0
    }

    // The custom-executable field shares this row rather than adding a fourth
    // one, so the form stays three rows tall in every state.
    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space1_5

        Label { text: root.useCustomProgram ? qsTr("Shell / executable") : qsTr("Shell") }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space2

            Select {
                id: shellSelect
                Layout.fillWidth: true
            }
            Input {
                id: programField
                Layout.fillWidth: true
                visible: root.useCustomProgram
                placeholderText: Qt.platform.os === "windows"
                                 ? qsTr("e.g. C:/Windows/System32/cmd.exe")
                                 : qsTr("e.g. /bin/zsh")
                invalid: root.showErrors && !root.valid
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space1_5

        Label { text: qsTr("Arguments") }
        Input {
            id: argumentsField
            Layout.fillWidth: true
            placeholderText: qsTr("Optional, e.g. -NoLogo")
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: Theme.space1_5

        Label { text: qsTr("Working directory") }
        Input {
            id: workdirField
            Layout.fillWidth: true
            placeholderText: qsTr("Default")
        }
    }

    Item { Layout.fillHeight: true }
}
