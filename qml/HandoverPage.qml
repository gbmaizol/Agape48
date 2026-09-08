import QtQuick
import Agape48

// "This calculator is somebody else's just now.", as a page, on a phone.
// Everything about why this is a page and not a window is in PageShell.qml;
// everything about what it says is in HandoverContent.qml.
//
// It matters more here than the other two pages did. On a desktop this dialog
// is a courtesy - both Agape48s are usually on the same machine, and the loser
// can be closed by hand. On a phone the other holder is ANOTHER DEVICE, on the
// end of a sync client, and this page is the only way to ask it for the
// calculator, watch the countdown, or decide to take it anyway.
PageShell {
    id: root
    required property Agape48Engine engine
    property alias live: content.live

    readonly property string phase: content.phase

    signal pickAnother()
    signal changeFolder()

    function showFor(h, name) { content.showFor(h, name) }
    function showUnanswered(name, host, why) { content.showUnanswered(name, host, why) }

    title: qsTr("Calculator in use")
    // Both ways out mean the same thing here - there is nothing typed to lose,
    // and no page under this one to go up to. Leaving stops the wait, or the
    // engine would go on counting for a dialog nobody can see.
    onBackRequested: root.leave()
    onDismissRequested: root.leave()

    function leave() {
        if (root.engine.waitingForCalculator())
            root.engine.stopWaiting()
        root.close()
    }

    HandoverContent {
        id: content
        anchors.fill: parent
        engine: root.engine
        onCloseRequested: root.leave()
        onRaiseRequested: root.open()
        onPickAnother: root.pickAnother()
        onChangeFolder: root.changeFolder()
    }
}
