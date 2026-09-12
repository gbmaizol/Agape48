import QtQuick
import Agape48

// "About Agape48", kiel paĝo, sur telefono. Ĉio pri kial ĉi tio estas paĝo kaj
// ne fenestro estas en PageShell.qml: peti duan fenestron sur Androido abortas
// la procezon.
PageShell {
    id: root
    required property Agape48Engine engine

    title: qsTr("About Agape48")
    // Nenio tajpita estas perdebla ĉi tie, do ambaŭ elirvojoj signifas la samon.
    onBackRequested: root.close()
    onDismissRequested: root.close()

    AboutContent {
        anchors.fill: parent
        engine: root.engine
        onCloseRequested: root.close()
    }
}
