pragma Singleton
import QtQuick
import QtCore

// Every text size the app draws at RUNTIME, in one place, so they can be tuned
// while looking at them instead of guessed at and rebuilt.
//
// Gert, 2026sep03: "expose the size of all kinds of text on banners messages,
// calculator buttons text, text over buttons, on a separate settings window
// that appears when clicking on 'Advanced' on the settings window. I'll find a
// size that works and make it default."
//
// WHAT IS NOT HERE, AND CANNOT BE. The key labels and the legends printed above
// them are not text at run time - they are painted into face.png by
// tools/makeface.py from seven numbers in tools/face.json, at build time. That
// is why they stay crisp at any window size, and it is why no slider can move
// them. AdvancedWindow.qml lists those seven so they are at least visible and
// nameable, and says plainly that they need a rebuild.
//
// The defaults are the values that were hard-coded before this existed, so a
// fresh profile looks exactly as it did. Making one of his numbers permanent
// means changing the default HERE; the stored value only records an override.
//
// Machine-local, like the window geometry and the keymap: a text size belongs
// to a screen and a pair of eyes, not to the calculator memory that syncs
// between machines.
QtObject {
    id: root

    // Aliases rather than bindings. A slider writing to a bound property breaks
    // the binding on the first drag and the value stops being saved - an alias
    // has one storage location and cannot get out of step with itself.
    property alias banner: store.banner                 // errors and notices over the face
    property alias screenMessage: store.screenMessage   // painted on a blank LCD
    property alias dialogTitle: store.dialogTitle       // secondary windows
    property alias dialogBody: store.dialogBody
    property alias dialogHint: store.dialogHint         // the grey explanatory lines

    readonly property int defaultBanner: 12
    readonly property int defaultScreenMessage: 52
    readonly property int defaultDialogTitle: 16
    readonly property int defaultDialogBody: 13
    readonly property int defaultDialogHint: 11

    // The span worth dragging through, not a validation rule.
    readonly property int minSize: 6
    readonly property int maxSize: 72

    function reset() {
        banner = defaultBanner
        screenMessage = defaultScreenMessage
        dialogTitle = defaultDialogTitle
        dialogBody = defaultDialogBody
        dialogHint = defaultDialogHint
    }

    readonly property bool changed: banner !== defaultBanner
                                    || screenMessage !== defaultScreenMessage
                                    || dialogTitle !== defaultDialogTitle
                                    || dialogBody !== defaultDialogBody
                                    || dialogHint !== defaultDialogHint

    property Settings store: Settings {
        id: store
        category: "textsize"
        property int banner: root.defaultBanner
        property int screenMessage: root.defaultScreenMessage
        property int dialogTitle: root.defaultDialogTitle
        property int dialogBody: root.defaultDialogBody
        property int dialogHint: root.defaultDialogHint
    }
}
