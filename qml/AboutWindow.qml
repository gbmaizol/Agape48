import QtQuick
import Agape48

// "About Agape48", en fenestro, sur labortablo. Ĉiu vorto estas en
// AboutContent.qml; ĉi tio estas la kadro, kaj la kadro estas ĉio kio diferencas
// inter tekokomputilo kaj telefono.
Window {
    id: root
    required property Agape48Engine engine

    readonly property bool opened: visible

    function place() {
        if (transientParent) {
            x = transientParent.x + (transientParent.width - width) / 2
            y = transientParent.y + 70
            engine.keepOnScreen(root)
        }
    }

    function open() { place(); show(); raise(); requestActivate() }

    title: qsTr("About Agape48")
    flags: Qt.Dialog
    color: "#1b1b1b"
    // Limigita al la ekrano; vidu SettingsWindow.qml por tio, kion telefono
    // faris al dialogo dimensiita por tekokomputilo.
    readonly property real fitW: Screen.desktopAvailableWidth  > 0
                                     ? Screen.desktopAvailableWidth  : 1e6
    readonly property real fitH: Screen.desktopAvailableHeight > 0
                                     ? Screen.desktopAvailableHeight : 1e6
    width: Math.min(540, fitW)
    height: Math.min(content.implicitHeight, fitH)
    minimumWidth: Math.min(420, fitW)
    minimumHeight: Math.min(280, fitH)

    AboutContent {
        id: content
        anchors.fill: parent
        engine: root.engine
        onCloseRequested: root.close()
    }
}
