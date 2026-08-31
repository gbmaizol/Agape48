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
    function close() { hide() }

    title: qsTr("Agape48 settings")
    flags: Qt.Dialog
    color: "#1b1b1b"
    // Set once, not bound: a binding on a window's size fights the user's own
    // drag and snaps it back, which is the trap Main.qml documents.
    width: 520
    height: 430
    minimumWidth: 380
    minimumHeight: 340

    // A path typed by a human is not a URL. Without this the state folder field
    // silently did nothing, because migrateTo() takes a QUrl and rejects
    // anything that is not a local file.
    // Esc closes it, same as the title bar's X. Dogfood #7: "I'm thinking this
    // program should be fully usable without a mouse."
    FolderDialog {
        id: folderPicker
        title: qsTr("Where should the calculator's memory live?")
        onAccepted: root.engine.state.migrateTo(selectedFolder)
    }

    FileDialog {
        id: romPicker
        title: qsTr("Choose an HP 48 ROM image")
        nameFilters: [qsTr("ROM images (rom rom.* *.rom *.bin)"), qsTr("All files (*)")]
        onAccepted: { root.engine.romSource = selectedFile; root.engine.start() }
    }

    // Both go through Qt rather than through string surgery here - see the
    // comment on Agape48Engine::pathToUrl. The hand-rolled pair worked on Linux
    // and was wrong on Windows for every path, because every Windows path has a
    // drive letter in front of it.
    function pathToUrl(p) { return root.engine.pathToUrl(p) }
    function urlToPath(u) { return root.engine.urlToPath(u) }

    // Esc closes the window. NOT a Shortcut: a Shortcut declared in a secondary
    // window goes on grabbing its sequence application-wide after the window is
    // hidden, and `enabled: root.active` does not stop it. That is what killed
    // Esc-is-ON on the calculator for the rest of the session once Settings had
    // been opened even once - dogfood #8 line 23. A key handler on an item
    // inside the window cannot leak out of it: an unhandled key bubbles up the
    // focus chain to here, and no further.
    Item {
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: (event) => { root.close(); event.accepted = true }

    Column {
        id: column
        anchors { fill: parent; margins: 18 }
        spacing: 10

        // The reason this window opened, when it opened itself. Before dogfood
        // #8 the only copy of this text was a banner in the calculator window,
        // underneath this one.
        Rectangle {
            width: parent.width
            height: errorText.implicitHeight + 16
            visible: root.engine.lastError !== ""
            color: "#5a1d18"
            radius: 4
            Text {
                id: errorText
                anchors { fill: parent; margins: 8 }
                text: root.engine.lastError
                color: "#ffdad6"; font.pixelSize: 12; wrapMode: Text.WordWrap
            }
        }

        Label {
            text: qsTr("HP 48 ROM")
            color: "#9a9a9a"; font.pixelSize: 12
        }
        Row {
            width: parent.width
            spacing: 6
            TextField {
                id: romField
                width: parent.width - romBrowse.width - parent.spacing
                // The ROM actually loaded, not an empty box. start() fills
                // romSource in with the "rom" it finds beside the state when
                // nobody has chosen one, and now says so, so this shows the
                // file really in use. Dogfood #9: "the current ROM's folder
                // and filename should be there".
                text: root.urlToPath(root.engine.romSource)
                placeholderText: qsTr("path to an HP 48 ROM image")
                onAccepted: {
                    root.engine.romSource = root.pathToUrl(text)
                    root.engine.start()
                }
            }
            Button {
                id: romBrowse
                text: "…"
                width: 40
                onClicked: romPicker.open()
            }
        }

        Item { width: 1; height: 6 }

        Label {
            text: qsTr("State folder")
            color: "#9a9a9a"; font.pixelSize: 12
        }
        Row {
            width: parent.width
            spacing: 6
            // A house, left of the field, for "put it back where it started".
            // It was a full-width "Use app storage" button; Gert asked for the
            // icon so the row reads as one thing.
            Button {
                id: homeButton
                width: 48
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Back to this computer's own app storage")
                onClicked: root.engine.state.useDefaultLocation()

                // Drawn rather than set as "⌂": that glyph is a thin outline
                // square with a lid in most fonts and reads as anything but a
                // house at this size. Canvas is part of QtQuick, so this costs
                // no module.
                Canvas {
                    id: houseIcon
                    anchors.centerIn: parent
                    width: 26; height: 24

                    // Take the button's own text colour rather than a fixed
                    // near-black. This window comes up on the system palette,
                    // which is dark on Gert's Windows, and "#1c1c1c" there is a
                    // near-black house on a near-black button - dogfood
                    // windows-01 line 12, "the house seems greyed out (low
                    // contrast)". The "…" button beside it stayed legible only
                    // because its glyph is real text that the style colours.
                    //
                    // Not a Windows bug: any dark palette does this, which is
                    // why Gert guessed it affects Linux too.
                    //
                    // Canvas does not repaint when a value its onPaint read
                    // changes, so the repaint has to be asked for explicitly.
                    property color ink: homeButton.palette.buttonText
                    onInkChanged: requestPaint()

                    onPaint: {
                        const ctx = getContext("2d")
                        ctx.reset()
                        ctx.lineWidth = 2
                        ctx.lineJoin = "round"
                        ctx.lineCap = "round"
                        ctx.strokeStyle = ink
                        // roof, wider than the walls so it overhangs
                        ctx.beginPath()
                        ctx.moveTo(1, 12); ctx.lineTo(13, 2); ctx.lineTo(25, 12)
                        ctx.stroke()
                        // walls
                        ctx.beginPath()
                        ctx.moveTo(4, 10); ctx.lineTo(4, 22); ctx.lineTo(22, 22); ctx.lineTo(22, 10)
                        ctx.stroke()
                        // door
                        ctx.beginPath()
                        ctx.rect(10, 15, 6, 7)
                        ctx.stroke()
                    }
                }
            }
            TextField {
                id: stateField
                width: parent.width - homeButton.width - browseButton.width - parent.spacing * 2
                text: root.engine.state.displayName
                placeholderText: qsTr("e.g. /home/you/Dropbox/Agape48")
                onAccepted: root.engine.state.migrateTo(root.pathToUrl(text))
            }
            Button {
                id: browseButton
                text: "…"
                width: 40
                onClicked: {
                    folderPicker.currentFolder = root.pathToUrl(stateField.text)
                    folderPicker.open()
                }
            }
        }
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            color: "#7d7d7d"; font.pixelSize: 11
            text: qsTr("Press Enter to move the calculator's memory there. Put it "
                       + "inside a synced folder to carry the machine between "
                       + "computers. You can also drop a folder on this window.")
        }

        Item { width: 1; height: 8 }

        // Debug logging, asked for in dogfood #8: there was no record at all of
        // why a start had failed, only a banner that vanished after six seconds.
        Row {
            spacing: 6
            // The label is a Label rather than the Switch's own text: the Basic
            // style paints that in a dark ink meant for a light window, and on
            // this background it was almost unreadable.
            Switch {
                id: logSwitch
                checked: root.engine.debugLogging
                onToggled: root.engine.debugLogging = checked
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Debug logging")
                color: "#e8e8e8"; font.pixelSize: 13
            }
        }
        Label {
            width: parent.width
            visible: logSwitch.checked
            text: root.engine.logPath
            color: "#7d7d7d"; font.pixelSize: 11
            elide: Text.ElideMiddle
        }

        Item { width: 1; height: 4 }

        Row {
            spacing: 10
            Button {
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }

    }

    // A folder dropped on the window is the cheapest folder picker there is,
    // and it needs no extra module at all.
    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls && drop.urls.length > 0)
                root.engine.state.migrateTo(drop.urls[0])
        }
    }
}
