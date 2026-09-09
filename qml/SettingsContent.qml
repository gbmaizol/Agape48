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
        onAccepted: root.moveTo(selectedFolder)
    }

    // EVERY ROUTE THAT MOVES THE MEMORY COMES THROUGH HERE - the picker, Enter
    // in the field, the Save button of the unsaved dialog, and the two Android
    // buttons - because all four have the same three outcomes and used to
    // handle two of them each.
    //
    // The third outcome is the one dogfood android-09 was written on. A folder
    // that Android will not let this app into cannot be told apart from a
    // folder that is not there, so the move fails, the switch that would fix it
    // is on a system screen in another app, and by the time the user comes back
    // the folder they picked is forgotten. It is remembered here instead, and
    // the move is finished for them the moment the app is in front again.
    property url pendingFolder
    // Re-asked rather than bound: the answer changes in the system settings,
    // while this process is in the background and has nothing to notice it
    // with.
    property bool anyFolder: root.engine.state.canUseAnyFolder()

    function moveTo(folder) {
        if (root.engine.state.migrateTo(folder)) {
            stateField.text = root.engine.state.displayName
            root.warnUnsaved = false
            root.pendingFolder = ""
            // A move that worked can still have something to say - joining a
            // shelf that already has calculators on it says which one opened -
            // and it should not be said in the colour of a failure.
            root.goodNews = root.engine.lastError !== ""
            return true
        }
        root.goodNews = false
        if (!root.engine.state.canUseAnyFolder())
            root.pendingFolder = folder
        return false
    }

    // The message strip is the same strip either way; only its colour and its
    // heading change.
    property bool goodNews: false

    // qmllint says "Member state not found on type QQmlApplication" here and is
    // wrong about the type: with QtQuick loaded, Qt.application is a
    // QQuickApplication, which adds state and stateChanged to what QQmlApplication
    // has. Measured on the phone at 22:58 on 2026sep09 - this handler is what
    // started the calculator again after the switch was turned on.
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (Qt.application.state !== Qt.ApplicationActive)
                return
            const had = root.anyFolder
            root.anyFolder = root.engine.state.canUseAnyFolder()
            if (!root.anyFolder)
                return
            if (String(root.pendingFolder) !== "") {
                const folder = root.pendingFolder
                root.pendingFolder = ""
                root.moveTo(folder)
            } else if (!had && !root.engine.ready) {
                // Nothing was picked, and the calculator is not running: the
                // app came up pointed at a folder it was not allowed into and
                // refused to start, which is exactly the state the switch was
                // just turned on to fix. Measured on the phone at 22:54 on
                // 2026sep09 - "Agape48 is not allowed into
                // /storage/emulated/0/Documents/Agape48Emulator/TestShelf any
                // more" - where turning the switch on left the message on
                // screen and the calculator still dark until the app was
                // killed and started again.
                root.engine.start()
            }
        }
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
            if (root.moveTo(root.pathToUrl(stateField.text)))
                root.closeRequested()
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
            color: root.goodNews ? "#1d3a24" : "#5a1d18"
            radius: 4
            Text {
                id: errorText
                anchors { fill: parent; margins: 8 }
                text: root.engine.lastError
                color: root.goodNews ? "#c8e6c9" : "#ffdad6"
                font.pixelSize: TextSizes.dialogBody; wrapMode: Text.WordWrap
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

        // TWO SHAPES FOR ONE SETTING, and the phone got the wrong one until
        // 2026sep09. Gert: "the 'press enter to move' mechanics is still a bit
        // confuse for a mobile app. Please look it over."
        //
        // He is right, and it is worse than confusing. A path typed by hand is
        // a desktop gesture: there is a keyboard, the path is short enough to
        // read in the box, and Enter is what a text field means. On a phone the
        // box shows the last thirty characters of a path nobody can type, the
        // keyboard covers half the screen, and Enter on a soft keyboard is a
        // key people press to dismiss it. Nothing on that screen said what
        // would happen, or when.
        //
        // So the phone gets no text field at all. It gets the path, whole,
        // where it can be read; and buttons, each of which says what it does
        // and does it when it is pressed. The desktop keeps the field, Enter,
        // the drop target and the unsaved-changes dialog, all of which were
        // asked for and all of which work there.
        Row {
            visible: Qt.platform.os !== "android"
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
            visible: Qt.platform.os !== "android"
            width: parent.width
            wrapMode: Text.WordWrap
            // Red and bold once the path in the field is not the one in use and
            // he has looked away from it. The sentence is already the
            // instruction, so it does not need rewording to become a warning.
            readonly property bool unsaved: root.warnUnsaved && root.statePending
            color: unsaved ? "#ff8a80" : "#7d7d7d"
            font.pixelSize: TextSizes.dialogHint
            font.bold: unsaved
            text: qsTr("Press Enter to move the calculator's memory there. "
                       + "Put it inside a synced folder to carry the machine "
                       + "between your devices. You can also drop a folder on "
                       + "this window.")
        }

        // ------------------------------------------------------------------
        // THE PHONE'S VERSION OF THE SAME SETTING.
        // ------------------------------------------------------------------

        // The path, whole and readable, rather than the last few characters of
        // it in a box too narrow to hold it. Nothing here is editable: on this
        // platform every way of changing it is a button.
        Rectangle {
            visible: Qt.platform.os === "android"
            width: parent.width
            height: statePath.height + 12
            color: "#141414"
            border.color: "#3a3a3a"
            radius: 3
            Label {
                id: statePath
                x: 6; y: 6
                width: parent.width - 12
                // A path is one word as far as wrapping is concerned - there is
                // nothing in it a word wrap is allowed to break - so it breaks
                // anywhere, the same rule the unsaved dialog needed.
                wrapMode: Text.WrapAnywhere
                color: "#8fc9ff"; font.pixelSize: TextSizes.dialogBody
                text: root.engine.state.displayName
            }
        }

        Button {
            visible: Qt.platform.os === "android"
            width: parent.width
            text: qsTr("Choose a folder…")
            // No Enter, no confirmation of its own: the system picker already
            // ends in a button that says USE THIS FOLDER, and asking again
            // after that would be asking twice.
            onClicked: folderPicker.open()
        }
        Label {
            visible: Qt.platform.os === "android"
            width: parent.width
            wrapMode: Text.WordWrap
            color: "#7d7d7d"; font.pixelSize: TextSizes.dialogHint
            text: qsTr("Moves the calculators to the folder you pick, and opens "
                       + "what is already there if that folder is another "
                       + "device's shelf. Pick one your sync app watches to "
                       + "carry the machine between your devices.")
        }

        // ANDROID ONLY. This used to be the answer to dogfood android-08 line
        // 7 - "I don't have access to the internal calculator folder, and I
        // can't change it to a visible folder before you implement this
        // possibility" - and since 2026sep09 it is no longer an answer to
        // anything, because that folder is where a phone now STARTS. What is
        // left for the button is the way back: it moves the calculators out of
        // whatever folder they are in and into Agape48's own, which needs no
        // permission and cannot be taken away.
        //
        // A button rather than a path he has to know: the folder is
        // Android/media/br.gbmaizol.agape48/Agape48 calculators, which nobody
        // would type and which the picker cannot return either - the system
        // picker does not show an app's own folders at all.
        Item { width: 1; height: 6; visible: sharedButton.visible }
        Button {
            id: sharedButton
            // Not while the calculators are already there, which on a phone is
            // where they start. A button whose whole effect is "yes, still
            // here" is one more thing to read on a page about storage.
            visible: Qt.platform.os === "android" && !root.engine.state.isDefault
            // NO `height: visible ? implicitHeight : 0` HERE OR BELOW. A Column
            // already leaves invisible children out of the layout, so that
            // binding bought nothing - and on a Label whose implicitHeight
            // comes from wrapping text inside a width, it is a cycle: measured
            // on the phone at c63966c, "SettingsContent.qml:419: QML Label:
            // Binding loop detected for property height", several times a
            // second for as long as the settings page was open.
            width: parent.width
            text: qsTr("Move them to Agape48's own folder")
            onClicked: {
                // Empty means no external storage, and sharedLocation() has
                // already put the reason where the error box will find it.
                const where = root.engine.state.sharedLocation()
                if (where.toString() === "")
                    return
                root.moveTo(where)
            }
        }
        Label {
            visible: sharedButton.visible
            width: parent.width
            wrapMode: Text.WordWrap
            color: "#7d7d7d"; font.pixelSize: TextSizes.dialogHint
            text: qsTr("Copies them into Android/media, where a file manager "
                       + "or a sync app can still find them and no permission "
                       + "is needed. Uninstalling Agape48 deletes that folder, "
                       + "so it is not the place for a calculator you want to "
                       + "keep.")
        }

        Item { width: 1; height: 6; visible: sharedButton.visible }

        // The house button's job, said in words, because there is no room for a
        // row of icons here and no tooltip on a phone to explain one.
        //
        // NOT THE SAME AS THE BUTTON ABOVE, and the two are worded to say so.
        // That one copies; this one walks away. It is the only way out of a
        // folder that cannot be read any more - a card taken out, or the
        // permission turned off in Android's settings - because copying out of
        // a folder needs to read it first.
        Button {
            visible: sharedButton.visible
            width: parent.width
            text: qsTr("Forget that folder and use Agape48's own")
            onClicked: {
                root.engine.state.useDefaultLocation()
                root.goodNews = false
                if (!root.engine.ready)
                    root.engine.start()
            }
        }

        Item { width: 1; height: 10; visible: Qt.platform.os === "android" }

        // THE ONE PERMISSION THIS APP ASKS FOR, offered rather than demanded,
        // and only on the platform that has it. Everything above works without
        // it; this is what makes a folder of HIS - the one his sync client
        // already watches - possible at all. See canUseAnyFolder() in
        // StateFileManager.cpp for why Android has no smaller answer.
        Button {
            id: anyFolderButton
            visible: Qt.platform.os === "android" && !root.anyFolder
            width: parent.width
            text: qsTr("Let Agape48 into your own folders")
            onClicked: root.engine.state.requestAnyFolderAccess()
        }
        Label {
            visible: Qt.platform.os === "android"
            width: parent.width
            wrapMode: Text.WordWrap
            color: "#7d7d7d"; font.pixelSize: TextSizes.dialogHint
            text: root.anyFolder
                      ? qsTr("Agape48 may keep the calculators in your own "
                             + "folders. Android's Settings can take that back "
                             + "whenever you like.")
                      : qsTr("Android keeps apps out of folders like Documents "
                             + "and Download. This opens the system switch for "
                             + "Agape48; turn it on, come back, and the folder "
                             + "you picked is moved into straight away. You do "
                             + "not have to: your sync app can watch Agape48's "
                             + "own folder above instead, and then nothing here "
                             + "needs any permission at all.")
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

        // Real calculator speed, asked for on 2026sep09: "a feature that's
        // required, default off in settings: Slow down to real calculator
        // speed... This is necessary to make calculator games playable."
        //
        // ON EVERY PLATFORM, and he said which matters most: "It's needed for
        // all builds, mainly the Android build." It costs less battery on,
        // not more - the throttle is a smaller instruction budget per tick,
        // never a wait. See kRealSpeedInstrPerSec in Agape48Engine.cpp.
        Row {
            spacing: 6
            Switch {
                id: realSpeedSwitch
                checked: root.engine.realSpeed
                onToggled: root.engine.realSpeed = checked
            }
            Label {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Slow down to real calculator speed")
                color: "#e8e8e8"; font.pixelSize: TextSizes.dialogBody
            }
        }
        Label {
            width: parent.width
            wrapMode: Text.WordWrap
            text: qsTr("Games written for a real HP 48 run about five times too "
                       + "fast otherwise. Everything else is quicker with this off.")
            color: "#7d7d7d"; font.pixelSize: TextSizes.dialogHint
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
