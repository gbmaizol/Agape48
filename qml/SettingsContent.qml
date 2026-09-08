import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Agape48

// The settings themselves, with no shell around them.
//
// Split out of SettingsWindow.qml on 2026sep07 because ANDROID HAS NO SECOND
// WINDOW. Measured on the phone: opening this as a Window aborts the process
// 10 times out of 10 - "Failed to acquire deadlock protector for
// QAndroidPlatformOpenGLWindow::eglSurface() while already locked by
// QtAndroidAccessibility" - and the Vulkan build says vkSurface() in the same
// sentence, so it is asking the platform for a surface that does it, not the
// renderer. Gert saw the shape of it before the stack trace did: "What could be
// crashing is we trying to use some desktop feature that doesn't exist in
// Android", and the desktop feature is the window itself.
//
// So the contents live here and get a shell chosen by platform: a Window on a
// desktop, a full-screen in-scene page on a phone. ONE copy of the contents,
// deliberately - two would have drifted apart inside a week.
//
// Nothing in here knows which shell it is in. The three things that used to
// reach the window directly are signals now: closeRequested, advancedRequested,
// and requestClose() for "the user asked to leave, ask about unsaved work
// first". Everything else was already platform-neutral.
Item {
    id: root
    required property Agape48Engine engine

    signal closeRequested()
    signal advancedRequested()

    // Every way out goes through here - the Close button, Esc, the title bar's
    // X on a desktop and the back gesture on a phone - so the confirmation
    // cannot be walked around by picking a different exit.
    function requestClose() {
        if (statePending) { warnUnsaved = true; unsavedDialog.open(); return }
        closeRequested()
    }

    // A path in the state folder field is not the state folder until Enter, and
    // the field goes on showing it either way. Dogfood both-03 line 1 is Gert
    // reading the shelf path back out of that field and concluding, reasonably,
    // that he had set it: "Now I know I should have pressed ENTER." Nothing
    // moved, no message said so, and the field agreed with him. A field that
    // lies about where the calculator's memory is is worse than an empty one.
    //
    // An empty field is NOT pending: clearing it and closing is a discard, and
    // nagging about that would be noise.
    readonly property bool statePending: stateField.text.trim() !== ""
                                         && stateField.text.trim() !== engine.state.displayName

    // Deliberately not shown while he is still typing - only once he has walked
    // away from the window or tried to close it, which is what he asked for:
    // "The setting window, upon loosing focus would cause the help text to
    // become red and bold."
    property bool warnUnsaved: false

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

    // "Save" and "Discard" rather than OK and Cancel, in his words: "Clicking
    // 'Close' before pressing ENTER should also make it red and bold, and ask
    // for confirmation, with options 'Save'   'Discard'."
    // The labels inside carry no colour of their own - see the note in
    // CalculatorPickerContent.qml. A Dialog's background comes from the system
    // palette and the ink has to come from the same place, or it is unreadable
    // on whichever half of the world the hard-coded value was not chosen for.
    Dialog {
        id: unsavedDialog
        anchors.centerIn: parent
        modal: true
        title: qsTr("The state folder has not been applied")
        standardButtons: Dialog.Save | Dialog.Discard
        // Explicit, not implicit. A Dialog with standardButtons sizes itself
        // from the widest of its content, header and footer - and the footer is
        // a DialogButtonBox whose own implicitWidth depends on the width it is
        // handed, so the two chase each other and Qt logs "Binding loop
        // detected for property implicitWidth" several times a second for as
        // long as the dialog is open. Pinning the width breaks the cycle; the
        // label wraps into whatever it is given. Bounded by the window so it
        // cannot grow wider than the thing it belongs to.
        width: Math.min(420, root.width - 40)
        // The path gets a box of its own, and the sentence stops running
        // through it. A path is ONE WORD as far as Text is concerned - there is
        // nothing in "/home/gert/Dropbox/Claude/Agape48Emulator/TestShelf" that
        // WordWrap is allowed to break - so a deep one simply ran off the side
        // of the dialog. Gert, both-06 line 26: "the folder name needs to be in
        // a special container, because this time it was going outside the
        // dialog instead of wrapping." WrapAnywhere is what breaks a word; the
        // frame is what makes it read as a path rather than as prose.
        Column {
            width: parent.width
            spacing: 8

            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                font.pixelSize: TextSizes.dialogBody
                text: qsTr("Save moves the calculator's memory to:")
            }

            Rectangle {
                width: parent.width
                // The label's own height, not the other way round, so nothing
                // here can chase itself: its width comes from this rectangle,
                // whose width comes from the dialog.
                height: pathLabel.height + 12
                color: "#141414"
                border.color: "#3a3a3a"
                radius: 3

                Label {
                    id: pathLabel
                    x: 6; y: 6
                    width: parent.width - 12
                    wrapMode: Text.WrapAnywhere
                    // Four lines is about 220 characters at this width, which
                    // is longer than any path either machine can make. Past
                    // that it is cut, because a dialog taller than the window
                    // it belongs to would hide its own buttons.
                    maximumLineCount: 4
                    elide: Text.ElideRight
                    color: "#8fc9ff"; font.pixelSize: TextSizes.dialogBody
                    text: stateField.text.trim()
                }
            }

            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                font.pixelSize: TextSizes.dialogBody
                text: qsTr("Discard leaves it where it is now.")
            }
        }
        // A refused move leaves this window open with the red banner saying
        // why, rather than closing as though it had worked.
        onAccepted: {
            if (root.engine.state.migrateTo(root.pathToUrl(stateField.text))) {
                stateField.text = root.engine.state.displayName
                root.warnUnsaved = false
                root.closeRequested()
            }
        }
        onDiscarded: {
            stateField.text = root.engine.state.displayName
            root.warnUnsaved = false
            root.closeRequested()
        }
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
        Keys.onEscapePressed: (event) => { root.requestClose(); event.accepted = true }

    // The content scrolls; the buttons do not move. This window used to be one
    // Column with the buttons as its last child, so anything that made the
    // content taller pushed them off the bottom edge - and the red strip at the
    // top is exactly that: four lines wide-screen, on a first run, in a window
    // whose default is 520x430. It used to right itself after twelve seconds,
    // when the error cleared itself; since the error stopped clearing while
    // there is no calculator, a temporary annoyance became a window whose Close
    // button could not be reached at all. Escape and the title bar still close
    // it on a desktop. On the phone, where the window IS the screen and cannot
    // be dragged taller, there would have been neither.
    //
    // Same shape as AdvancedWindow: a Flickable for the content and one Row
    // pinned to the bottom, with the gap between them the height of that Row.
    Flickable {
        anchors { fill: parent; margins: 18; bottomMargin: 56 }
        contentWidth: width
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

    Column {
        id: column
        width: parent.width
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
                color: "#ffdad6"; font.pixelSize: TextSizes.dialogBody; wrapMode: Text.WordWrap
            }
        }

        Label {
            text: qsTr("HP 48 ROM")
            color: "#9a9a9a"; font.pixelSize: TextSizes.dialogHint
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
            color: "#9a9a9a"; font.pixelSize: TextSizes.dialogHint
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
                ToolTip.text: qsTr("Back to this device's own app storage")
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
                placeholderText: qsTr("a folder to keep the calculator's memory in")
                onAccepted: {
                    // Re-sync only on success: a rejected path stays in the box
                    // so it can be corrected, with the banner saying what was
                    // wrong with it.
                    if (root.engine.state.migrateTo(root.pathToUrl(text)))
                        text = root.engine.state.displayName
                    root.warnUnsaved = false
                }
                // Losing the BOX, not the window. The window's onActiveChanged
                // above is the same rule and it fires on Windows - both-05 line
                // 32 is "Now it works. Pass." there - but never on Linux, where
                // this window is a transient child of the calculator window and
                // Qt goes on calling it active while the parent has the focus.
                // Instrumented before believing it: over a whole session the
                // handler fired twice and said active=true both times.
                //
                // The field is the better question anyway. "Click away" is
                // something you do to a box, which is also how Gert put it, and
                // it makes the two machines agree instead of one of them being
                // right by accident. Both are kept: whichever notices first.
                onActiveFocusChanged: if (!activeFocus && root.statePending)
                                          root.warnUnsaved = true
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
            // Red and bold once the path in the field is not the one in use and
            // he has looked away from it. The sentence is already the
            // instruction, so it does not need rewording to become a warning.
            readonly property bool unsaved: root.warnUnsaved && root.statePending
            color: unsaved ? "#ff8a80" : "#7d7d7d"
            font.pixelSize: TextSizes.dialogHint
            font.bold: unsaved
            // The last sentence is a desktop gesture: there is nothing on a
            // phone to drag a folder FROM, and on a phone this is not a window.
            // Gert's complaint about the resize switch was the same complaint -
            // "there are functions that don't make sense in Android".
            text: Qt.platform.os === "android"
                      ? qsTr("Press Enter to move the calculator's memory there. "
                             + "To carry the machine between your devices it has "
                             + "to be somewhere your sync app can reach, and on "
                             + "Android that is not the folder it starts in.")
                      : qsTr("Press Enter to move the calculator's memory there. "
                             + "Put it inside a synced folder to carry the "
                             + "machine between your devices. You can also drop "
                             + "a folder on this window.")
        }

        // ANDROID ONLY, and it is the answer to dogfood android-08 line 7:
        // "I don't have access to the internal calculator folder, and I can't
        // change it to a visible folder before you implement this possibility."
        //
        // A button rather than a path he has to know: the folder is
        // Android/media/br.gbmaizol.agape48/Agape48 calculators, which nobody
        // would type and which the "…" picker cannot return either - that picker
        // hands back a content:// tree, and the emulator core needs a real path.
        // Everything about why this particular folder is in StateFileManager::
        // sharedLocation().
        Item { width: 1; height: 6; visible: sharedButton.visible }
        Button {
            id: sharedButton
            visible: Qt.platform.os === "android"
            height: visible ? implicitHeight : 0
            width: parent.width
            text: qsTr("Move it where other apps can see it")
            onClicked: {
                // Empty means no external storage, and sharedLocation() has
                // already put the reason where the error box will find it.
                const where = root.engine.state.sharedLocation()
                if (where.toString() === "")
                    return
                if (root.engine.state.migrateTo(where)) {
                    stateField.text = root.engine.state.displayName
                    root.warnUnsaved = false
                }
            }
        }
        Label {
            visible: sharedButton.visible
            height: visible ? implicitHeight : 0
            width: parent.width
            wrapMode: Text.WordWrap
            color: "#7d7d7d"; font.pixelSize: TextSizes.dialogHint
            text: qsTr("Puts the calculators in Android/media, where a file "
                       + "manager or a sync app can find them. Nothing is asked "
                       + "of you and no permission is needed - the folder "
                       + "belongs to Agape48, it is just not hidden.")
        }

        Item { width: 1; height: 8; visible: liveResizeRow.visible }

        // Off by default. A resize drag draws an outline and the window changes
        // shape once, when the mouse is released; ticked, the window follows the
        // pointer the whole way. Both are kept because the second is what most
        // programs do, and the first is what stops the face jumping about on a
        // fast drag - dogfood windows-02.
        //
        // Not on a phone, where there is no window to drag the edge of: Android
        // imposes the size, which is why 31be0c8 turned the aspect lock off
        // there. Gert, 2026sep07: "there are functions that don't make sense in
        // Android, like the resize option in the settings dialog."
        Row {
            id: liveResizeRow
            visible: Qt.platform.os !== "android"
            spacing: 6
            Switch {
                id: liveResizeSwitch
                checked: root.engine.liveResize
                onToggled: root.engine.liveResize = checked
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Resize the window live, without an outline")
                color: "#e8e8e8"; font.pixelSize: TextSizes.dialogBody
            }
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
                color: "#e8e8e8"; font.pixelSize: TextSizes.dialogBody
            }
        }
        Label {
            width: parent.width
            visible: logSwitch.checked
            text: root.engine.logPath
            color: "#7d7d7d"; font.pixelSize: TextSizes.dialogHint
            elide: Text.ElideMiddle
        }

    }
    }

    Row {
        anchors { left: parent.left; bottom: parent.bottom; margins: 18 }
        spacing: 10
        Button {
            text: qsTr("Close")
            onClicked: root.requestClose()
        }
        // Its own window rather than a page in here, because half of what it
        // tunes is drawn on the calculator and this window covers it.
        // On a phone there is no beside, so it is a page over this one -
        // Gert, 2026sep07: "the advanced wouldn't be a new window. It would be
        // a new page inside settings, right?"
        Button {
            text: qsTr("Advanced…")
            onClicked: root.advancedRequested()
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
