import QtQuick
import Agape48

// Settings on a phone: the same contents the desktop window frames, in a page.
// Everything about why is in PageShell.qml.
PageShell {
    id: root
    required property Agape48Engine engine

    title: qsTr("Settings")
    // The back arrow asks the contents first, because only they know whether
    // there is an unapplied path sitting in the folder field.
    onBackRequested: content.requestClose()

    SettingsContent {
        id: content
        anchors.fill: parent
        engine: root.engine
        onCloseRequested: root.close()
        // A page inside this page, which is what Gert asked for and what a
        // phone's settings do everywhere else: "the advanced wouldn't be a new
        // window. It would be a new page inside settings, right?"
        onAdvancedRequested: advanced.open()
    }

    // Declared inside the settings page, so it opens over it and the back arrow
    // takes it off again, leaving Settings where it was.
    AdvancedPage {
        id: advanced
        width: root.width
        height: root.height
    }
}
