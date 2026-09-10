import QtQuick
import Agape48

// Tekstgrandoj sur telefono: paĝo super la agordpaĝo. Vidu PageShell.qml.
PageShell {
    id: root

    // Transdonita al la rapidkalibrado. Vidu AdvancedContent.qml.
    property var engine: null

    title: qsTr("Advanced")
    onBackRequested: root.close()

    AdvancedContent {
        anchors.fill: parent
        engine: root.engine
        onCloseRequested: root.close()
    }
}
