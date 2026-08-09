pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Layouts
import QtQuickTerminal

Dialog {
    id: root

    signal sessionRequested(var config)

    readonly property var activeForm: [ptyForm, serialForm, telnetForm][typeTabs.currentIndex]

    // Opens straight onto one session type, for the split button's dropdown.
    function openWithType(index) {
        typeTabs.currentIndex = index
        open()
    }

    title: qsTr("New Session")
    description: qsTr("Configure and start a new terminal session.")
    implicitWidth: 480

    onOpened: {
        ptyForm.showErrors = false
        serialForm.showErrors = false
        telnetForm.showErrors = false
        serialForm.refreshPorts()
    }

    // Assigned as contentItem so the Popup stretches it to availableWidth;
    // declaring it as a plain child would leave it at its implicit width.
    contentItem: ColumnLayout {
        spacing: Theme.space4

        Tabs {
            id: typeTabs
            Layout.fillWidth: true

            TabButton { text: qsTr("Shell") }
            TabButton { text: qsTr("Serial") }
            TabButton { text: qsTr("Telnet") }
        }

        StackLayout {
            Layout.fillWidth: true
            // Every form is laid out to three parameter rows and pads the
            // remainder with a filler, so pinning the area to one form's
            // height keeps the dialog from resizing as tabs are switched.
            Layout.preferredHeight: ptyForm.implicitHeight
            currentIndex: typeTabs.currentIndex

            PtySessionForm { id: ptyForm }
            SerialSessionForm { id: serialForm }
            TelnetSessionForm { id: telnetForm }
        }

        Text {
            Layout.fillWidth: true
            visible: root.activeForm.showErrors && !root.activeForm.valid
            text: root.activeForm.errorMessage
            color: Theme.destructive
            font.pixelSize: Theme.textXs
            wrapMode: Text.Wrap
        }
    }

    footerContent: RowLayout {
        spacing: Theme.space2

        Item { Layout.fillWidth: true }

        Button {
            text: qsTr("Cancel")
            variant: Button.Outline
            onClicked: root.close()
        }
        Button {
            text: qsTr("Connect")
            onClicked: {
                if (!root.activeForm.valid) {
                    root.activeForm.showErrors = true
                    return
                }
                root.sessionRequested(root.activeForm.sessionConfig)
                root.close()
            }
        }
    }
}
