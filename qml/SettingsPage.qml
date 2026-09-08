import QtQuick
import Agape48

// Settings on a phone: the same contents the desktop window frames, in a page.
// Everything about why is in PageShell.qml.
PageShell {
    id: root
    required property Agape48Engine engine

    // Whoever placed this page places the next one too - see Main.qml. A page
    // declared INSIDE a PageShell lands under its header, which put the Text
    // sizes page below the Settings header on the phone: two back arrows
    // stacked, the lower page hanging off the bottom edge by the height of the
    // header it was pushed down by, and the top arrow dead because the modal
    // page above it swallowed the tap. Measured at b221c14.
    signal advancedRequested()

    title: qsTr("Settings")
    // The back arrow asks the contents first, because only they know whether
    // there is an unapplied path sitting in the folder field.
    onBackRequested: content.requestClose()

    // Closing on somebody else's say-so - the phone's back, which Main.qml
    // turns into "shut everything and show me the calculator". It goes through
    // the contents for the same reason the arrow does: a folder typed and not
    // applied is still worth a question, and the gesture is if anything MORE
    // likely to be the accident that loses it.
    function dismiss() { content.requestClose() }

    SettingsContent {
        id: content
        anchors.fill: parent
        engine: root.engine
        onCloseRequested: root.close()
        // A page inside this page, which is what Gert asked for and what a
        // phone's settings do everywhere else: "the advanced wouldn't be a new
        // window. It would be a new page inside settings, right?"
        onAdvancedRequested: root.advancedRequested()
    }
}
