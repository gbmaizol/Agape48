import QtQuick
import Agape48

// "This calculator is somebody else's just now.", en fenestro, sur
// labortablo. La enhavo kaj ĉiu vorto en ĝi estas en HandoverContent.qml; ĉi
// tio estas la kadro, kaj la kadro estas ĉio kio diferencas inter tekokomputilo
// kaj telefono. Sama divido kiel Agordoj kaj la breto, kaj farita en la nokto
// kiam la telefono unuafoje povis atingi ĝin - vidu la noton ĉe la supro de la
// enhavo.
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
            engine.keepOnScreen(root)
        }
    }

    title: qsTr("Calculator in use")
    flags: Qt.Dialog
    color: "#1b1b1b"
    // Limigita al la ekrano; vidu SettingsWindow.qml por tio, kion telefono
    // faris al dialogo dimensiita por tekokomputilo.
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
