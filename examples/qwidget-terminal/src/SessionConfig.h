#pragma once

#include <QString>
#include <QStringList>

// What NewSessionDialog produces and TerminalTab consumes. Mirrors the
// sessionConfig object the QML demo passes around.
struct SessionConfig
{
    enum Type { Pty, Serial, Telnet };

    Type type = Pty;
    QString label;

    // Pty
    QString program;
    QStringList arguments;
    QString workingDirectory;

    // Serial
    QString portName;
    int baudRate = 115200;
    int dataBits = 8;
    QString parity = QStringLiteral("N");
    int stopBits = 1;
    QString flowControl = QStringLiteral("none");

    // Telnet
    QString host;
    int port = 23;
};
