import QtQuick
import QtQuick.Controls
import Agape48

// Text sizes, live, with the number showing.
//
// Gert, 2026sep03: "I'll find a size that works and make it default." So the
// point of this window is not the sliders, it is the NUMBER beside each one -
// he drags until it looks right, reads the figure off, and that figure becomes
// a default in TextSizes.qml. Every one of them applies instantly to the window
// underneath, which is why this is a separate window rather than a page inside
// Settings: Settings covers the calculator, and half of what is being tuned is
// drawn on the calculator.
Window {
    id: root

    readonly property bool opened: visible

    function open() {
        if (transientParent) {
            x = transientParent.x + transientParent.width + 12
            y = transientParent.y
        }
        show(); raise(); requestActivate()
    }
    function close() { hide() }

    title: qsTr("Text sizes")
    flags: Qt.Dialog
    color: "#1b1b1b"
    // Clamped to the screen; see SettingsWindow.qml for what a phone did to a
    // dialog sized for a laptop.
    readonly property real fitW: Screen.desktopAvailableWidth  > 0
                                     ? Screen.desktopAvailableWidth  : 1e6
    readonly property real fitH: Screen.desktopAvailableHeight > 0
                                     ? Screen.desktopAvailableHeight : 1e6
    width: Math.min(460, fitW)
    height: Math.min(560, fitH)
    minimumWidth: Math.min(380, fitW)
    minimumHeight: Math.min(420, fitH)

    AdvancedContent {
        anchors.fill: parent
        onCloseRequested: root.hide()
    }
}
