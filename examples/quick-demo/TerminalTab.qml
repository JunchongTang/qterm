pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QTerm

// TerminalTab owns a QTermSession + QTermTerminal pair and dynamically
// creates the appropriate backend from sessionConfig.
//
// sessionConfig object shape:
//   { type: "pty"|"serial"|"telnet", label: string, ...type-specific params }
//
// PTY params:    program?, arguments?
// Serial params: portName, baudRate?, dataBits?, parity?, stopBits?, flowControl?
// Telnet params: host, port?
Item {
    id: root

    required property var sessionConfig

    // Title for the tab button — follows OSC 2, falls back to config label.
    readonly property string tabTitle:
        terminal.title.length > 0 ? terminal.title : sessionConfig.label

    readonly property int sessionState: session.state

    // ── Session objects (QML-native) ──────────────────────────────────────────

    QTermTerminal {
        id: terminal
        session: session
    }

    QTermSession {
        id: session
    }

    // ── Backend component templates ───────────────────────────────────────────

    Component {
        id: ptyBackendComponent
        QTermLocalPtyBackend { }
    }

    Component {
        id: serialBackendComponent
        QTermSerialBackend { }
    }

    Component {
        id: telnetBackendComponent
        QTermTelnetBackend { }
    }

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    Component.onCompleted: {
        const backend = createBackend(root.sessionConfig)
        session.backend = backend
        session.open()
    }

    Component.onDestruction: {
        session.close()
    }

    // ── Terminal view ─────────────────────────────────────────────────────────

    TerminalPane {
        anchors.fill: parent
        terminal: terminal
    }

    // ── Private helpers ───────────────────────────────────────────────────────

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
        // Default: local PTY
        const props = {}
        if (config.program)
            props.program = config.program
        if (config.arguments)
            props.arguments = config.arguments
        if (config.workingDirectory)
            props.workingDirectory = config.workingDirectory
        return ptyBackendComponent.createObject(root, props)
    }
}
