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

    // Passed through to the speed calibration. See AdvancedContent.qml.
    property var engine: null

    readonly property bool opened: visible

    function open() {
        if (transientParent) {
            x = transientParent.x + transientParent.width + 12
            y = transientParent.y
        }
        show(); raise(); requestActivate()
    }
    function close() { hide() }

    // "Advanced" rather than "Text sizes" since 2026sep10, when the speed
    // calibration moved in here and made the old title a lie. It is also what
    // the button that opens it says, and what Gert calls it: "the advanced
    // settings window".
    title: qsTr("Advanced")
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
        engine: root.engine
        onCloseRequested: root.hide()
    }
}
