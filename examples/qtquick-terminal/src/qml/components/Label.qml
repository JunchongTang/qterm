import QtQuick
import QtQuick.Controls.Basic as C
import QtQuickTerminal

// shadcn form label: 12px medium foreground text, dimmed when disabled.
C.Label {
    color: Theme.foreground
    font.pixelSize: Theme.textXs
    font.weight: Font.Medium
    opacity: enabled ? 1.0 : 0.5
    verticalAlignment: Text.AlignVCenter
}
