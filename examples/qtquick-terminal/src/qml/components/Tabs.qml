import QtQuick
import QtQuick.Controls.Basic as C
import QtQuickTerminal

// shadcn segmented tab strip: a muted rounded background with the active
// trigger rendered as a filled pill. Content panels are left to the caller
// (bind a StackLayout's currentIndex to currentIndex).
C.TabBar {
    id: control

    // Sum of child implicit widths; when the strip is stretched wider than
    // this, triggers share the width equally (shadcn's flex-1 sizing).
    readonly property real _sumChildWidth: {
        let s = 0, n = 0
        for (let i = 0; i < contentChildren.length; i++) {
            const it = contentChildren[i]
            if (it) { s += it.implicitWidth; n++ }
        }
        return s + Math.max(0, n - 1) * spacing
    }

    padding: 3
    spacing: 0

    implicitWidth: _sumChildWidth + leftPadding + rightPadding
    implicitHeight: 32

    contentItem: ListView {
        model: control.contentModel
        currentIndex: control.currentIndex
        spacing: control.spacing
        orientation: ListView.Horizontal
        boundsBehavior: Flickable.StopAtBounds
        snapMode: ListView.SnapToItem
        highlightMoveDuration: 0
    }

    background: Rectangle {
        radius: Theme.radiusLg
        color: Theme.muted
    }
}
