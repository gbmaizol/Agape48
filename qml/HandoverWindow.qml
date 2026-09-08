import QtQuick
import Agape48

// "This calculator is somebody else's just now.", in a window, on a desktop.
// The contents and every word in them are in HandoverContent.qml; this is the
// frame, and the frame is all that differs between a laptop and a phone. Same
// split as Settings and the shelf, and made on the night the phone could first
// reach it - see the note at the top of the contents.
Window {
    id: root
    required property Agape48Engine engine
    property alias live: content.live

    readonly property bool opened: visible
    readonly property string phase: content.phase

    signal pickAnother()
    signal changeFolder()

    function showFor(h, name) { content.showFor(h, name) }
    function showUnanswered(name, host, why) { content.showUnanswered(name, host, why) }

    function place() {
        if (transientParent) {
            x = transientParent.x + (transientParent.width - width) / 2
            y = transientParent.y + 90
        }
    }

    title: qsTr("Calculator in use")
    flags: Qt.Dialog
    color: "#1b1b1b"
    // Clamped to the screen; see SettingsWindow.qml for what a phone did to a
    // dialog sized for a laptop.
    readonly property real fitW: Screen.desktopAvailableWidth  > 0
                                     ? Screen.desktopAvailableWidth  : 1e6
    readonly property real fitH: Screen.desktopAvailableHeight > 0
                                     ? Screen.desktopAvailableHeight : 1e6
    width: Math.min(560, fitW)
    height: Math.min(340, fitH)
    minimumWidth: Math.min(460, fitW)
    minimumHeight: Math.min(300, fitH)

    onVisibleChanged: if (!visible && root.engine.waitingForCalculator()) root.engine.stopWaiting()

    HandoverContent {
        id: content
        anchors.fill: parent
        engine: root.engine
        onCloseRequested: root.close()
        onRaiseRequested: { root.place(); root.show(); root.raise(); root.requestActivate() }
        onPickAnother: root.pickAnother()
        onChangeFolder: root.changeFolder()
    }
}
