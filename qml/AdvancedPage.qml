import QtQuick
import Agape48

// Text sizes on a phone: a page over the settings page. See PageShell.qml.
PageShell {
    id: root

    // Passed through to the speed calibration. See AdvancedContent.qml.
    property var engine: null

    title: qsTr("Advanced")
    onBackRequested: root.close()

    AdvancedContent {
        anchors.fill: parent
        engine: root.engine
        onCloseRequested: root.close()
    }
}
