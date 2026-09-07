import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Agape48

// Settings, in a window of its own.
//
// This was a bottom sheet lying over the calculator face until 2026aug29, and
// Gert called that out three times running before asking outright why. The
// honest answer is that it was written phone-first - a sheet sliding up from
// the bottom edge is the Android idiom - and then never revisited when the
// desktop build turned out to be the one that exists. On a 451-wide window it
// did not fit, it hid the calculator, and it was the direct cause of the dead
// keyboard in dogfood #5: its text fields took focus inside the main window
// and nothing gave it back. A separate window cannot do that, because closing
// it reactivates the main window, and the main window hands the keyboard back
// to the keypad on the way in.
//
// Everything in here does something. Three controls were removed rather than
// left looking real:
//
//   Copy stack / Paste / Save now - already in the ⋮ menu, and Gert's rule is
//     that one command belongs in one place.
//   Haptics / Beep - two switches over a Feedback singleton that is still a
//     TODO, so they toggled nothing at all.
//   Pick folder… - it called StateFileManager::requestLocation(), which on
//     desktop only emits pickerRequested() and waits for a FolderDialog that
//     nobody showed. Gert asked for a real one in dogfood #7, so the "…" button
//     beside the state folder is now a QtQuick.Dialogs FolderDialog - native on
//     this desktop, because Qt ships the gtk3 platform theme. That is one more
//     Qt module against the lean-module rule, taken on his say-so.
// Android note for later: a second top-level Window is a desktop idiom. When
// the Android build happens this wants to become a full-screen page or a sheet
// again, because a phone has no window manager to put it anywhere sensible.
Window {
    id: root
    required property Agape48Engine engine

    // Main.qml asks this to decide whether the keypad may take the keyboard.
    readonly property bool opened: visible

    function open() {
        if (transientParent) {
            x = transientParent.x + (transientParent.width - width) / 2
            y = transientParent.y + 48
        }
        show(); raise(); requestActivate()
    }
    // Both exits ask the contents first, because only they know whether there
    // is an unapplied path in the folder field.
    function close() { content.requestClose() }

    onActiveChanged: if (!active && content.statePending) content.warnUnsaved = true

    onClosing: (event) => {
        if (content.statePending) {
            event.accepted = false
            content.requestClose()
        }
    }

    title: qsTr("Agape48 settings")
    flags: Qt.Dialog
    color: "#1b1b1b"
    // Set once, not bound: a binding on a window's size fights the user's own
    // drag and snaps it back, which is the trap Main.qml documents.
    // Clamped to the screen, because a phone's screen can be smaller than a
    // dialog written for a laptop. Measured on the phone at ef76dd2: the screen
    // is 458 units wide and this window asked for 520, so it was centred on
    // something wider than the display and clipped on BOTH edges - "P 48 ROM"
    // for "HP 48 ROM", the browse buttons off the right. The minimums matter as
    // much as the sizes: a minimumWidth wider than the screen would push it
    // straight back out. The guard is for desktopAvailable* coming back 0
    // before the window is mapped, which Main.qml documents; Math.min against 0
    // would give a window with no size at all.
    readonly property real fitW: Screen.desktopAvailableWidth  > 0
                                     ? Screen.desktopAvailableWidth  : 1e6
    readonly property real fitH: Screen.desktopAvailableHeight > 0
                                     ? Screen.desktopAvailableHeight : 1e6
    width: Math.min(520, fitW)
    height: Math.min(430, fitH)
    minimumWidth: Math.min(380, fitW)
    minimumHeight: Math.min(340, fitH)

    SettingsContent {
        id: content
        anchors.fill: parent
        engine: root.engine
        onCloseRequested: root.hide()
        onAdvancedRequested: advanced.open()
    }

    // Its own window rather than a page in here, because half of what it tunes
    // is drawn on the calculator and this window covers it. A window, and
    // therefore desktop-only until it gets the same treatment as this one.
    AdvancedWindow {
        id: advanced
        transientParent: root
    }
}
