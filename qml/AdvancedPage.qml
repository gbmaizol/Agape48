import QtQuick
import Agape48

// Text sizes on a phone: a page over the settings page. See PageShell.qml.
PageShell {
    id: root

    title: qsTr("Text sizes")
    onBackRequested: root.close()

    AdvancedContent {
        anchors.fill: parent
        onCloseRequested: root.close()
    }
}
