pragma ComponentBehavior: Bound

import QtQuick
import QTerm
import QtQuickTerminal

// One terminal session: terminal core + session + backend, created from a
// sessionConfig produced by NewSessionDialog.
Item {
    id: root

    required property var sessionConfig

    readonly property string tabTitle: term.title.length > 0
                                       ? term.title
                                       : (sessionConfig.label || qsTr("Terminal"))

    function createBackend(config) {
        if (config.type === "serial") {
            return serialBackendComponent.createObject(root, {
                portName: config.portName || "",
                baudRate: config.baudRate || 9600,
                dataBits: config.dataBits || 8,
                parity: config.parity || "N",
                stopBits: config.stopBits || 1,
                flowControl: config.flowControl || "none"
            })
        }
        if (config.type === "telnet") {
            return telnetBackendComponent.createObject(root, {
                host: config.host || "",
                port: config.port || 23
            })
        }

        const props = {}
        if (config.program)
            props.program = config.program
        if (config.arguments)
            props.arguments = config.arguments
        if (config.workingDirectory)
            props.workingDirectory = config.workingDirectory
        return localShellBackendComponent.createObject(root, props)
    }

    QTermTerminal {
        id: term
        session: termSession
    }

    QTermSession {
        id: termSession
    }

    Component { id: localShellBackendComponent; QTermLocalShellBackend {} }
    Component { id: serialBackendComponent; QTermSerialBackend {} }
    Component { id: telnetBackendComponent; QTermTelnetBackend {} }

    Component.onCompleted: {
        termSession.backend = createBackend(sessionConfig)
        termSession.open()
    }

    Component.onDestruction: termSession.close()

    TerminalPane {
        anchors.fill: parent
        terminal: term
    }
}
