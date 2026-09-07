import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtCore
import Agape48

Window {
    id: root
    // Shown only once applyDefaultGeometry() has sized it. Mapping the window
    // first and resizing after leaves the window manager free to negotiate a
    // size of its own, and keepAspect() then "corrects" whatever it picked and
    // locks that in - which is how dogfood #1 got a 160x300 window on one
    // machine and 403x756 on another from the same code.
    visible: false
    // The letterbox bars, on the rare frames where the window is not yet the
    // face's shape - during a drag, mostly. In the body colour rather than
    // near-black, so what shows is the calculator's own edge and not a hole.
    color: "#222226"
    title: qsTr("Agape48")

    // No title bar, like Emu48. The window manager then offers no way to move
    // or resize it, so the app has to ask for both itself: dragging any part
    // of the body that is not a key moves it, and the outer few pixels resize
    // it. Both hand off to the WM through QWindow::startSystemMove/Resize, so
    // snapping and edge tiling still behave normally.
    flags: Qt.Window | Qt.FramelessWindowHint

    // The face's proportions. Everything about window sizing is expressed
    // against these, and Calculator.qml scales the face by a single Math.min,
    // so a window of the wrong ratio can only ever add bars - it can never
    // stretch the calculator.
    readonly property real faceW: engine.skin.faceSize.width  || 480
    readonly property real faceH: engine.skin.faceSize.height || 900
    readonly property real faceAspect: faceW / faceH

    // Machine-local on purpose. Window geometry belongs to this screen and this
    // machine, not to the state a user syncs between them - the same rule that
    // keeps the keymap out of sync in design-questions item 10b. Settings maps
    // to QSettings, which is per-machine by construction.
    Settings {
        id: geom
        category: "window"
        property int  w: 0                  // 0 = never sized; pick a default
        property int  h: 0
        property int  x: -1                 // -1 = never placed; let the WM decide
        property int  y: -1
    }

    // Chosen once at startup, deliberately NOT a binding on width/height: a
    // binding fights the user's drag and snaps the window back mid-resize,
    // which is what the previous version of this file did.
    property bool geometryApplied: false

    // force: show the window even though the skin never arrived. See showAnyway
    // below - the ordinary calls pass nothing and wait, as they always have.
    function applyDefaultGeometry(force) {
        if (geometryApplied)
            return
        // The skin decides the ratio and may still be loading, so wait for it
        // rather than sizing from the 480x900 fallback and never correcting.
        // SkinModel emits changed() when the face is in.
        if (!force && (engine.skin.faceSize.width <= 0
                       || engine.skin.faceSize.height <= 0))
            return
        geometryApplied = true

        if (geom.w > 0 && geom.h > 0) {
            // The saved size was taken against whatever face was loaded then.
            // Keep the width and re-derive the height, or a face whose
            // proportions have changed since - they did when the annunciators
            // got their real size - sits letterboxed until the next resize.
            //
            // AND IT HAS TO FIT THE SCREEN IT IS BEING RESTORED ONTO, which is
            // not the screen it was saved from. On Android that is not an edge
            // case, it is every upgrade: the window is forced full screen, the
            // aspect lock then derives a WIDTH from that height - 1018 * 0.594
            // = 605 on Gert's phone - and 605 is remembered on a screen 458
            // wide. The next run restored it and the face lost its whole
            // right-hand column, NXT to DROP, off the edge. Measured on the
            // upgrade install of 7e0624a; a fresh install has nothing saved and
            // was correct.
            //
            // The same thing on a desktop is a laptop undocked from a wide
            // monitor, which is why this is not written as an Android case.
            // Width first, then height, and the aspect is kept through both so
            // a window that has to lose height loses the width to match.
            const maxW = Screen.desktopAvailableWidth  > 0 ? Screen.desktopAvailableWidth  : 1e6
            const maxH = Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight : 1e6
            let w = Math.min(geom.w, maxW)
            let h = Math.round(w / root.faceAspect)
            if (h > maxH) {
                h = maxH
                w = Math.round(h * root.faceAspect)
            }
            console.info("agape48: saved", geom.w + "x" + geom.h,
                         "screen", maxW + "x" + maxH, "-> window", w + "x" + h)
            setGeometry(w, h)
            return
        }
        // 70% of the available height, never past the face's native size.
        // Upscaling a photograph is the one thing a skin cannot recover from,
        // which is why item 11 says ship the face at twice the phone width
        // rather than reach for a cleverer resampler.
        // desktopAvailableHeight can come back 0 before the window is mapped,
        // which sized the first Linux build at Qt's fallback instead of 70% of
        // the screen. Fall back to the raw screen height, then floor it.
        const avail = Screen.desktopAvailableHeight > 0 ? Screen.desktopAvailableHeight
                                                        : Screen.height
        const h = Math.max(400, Math.min(avail * 0.70, root.faceH))
        console.info("agape48: screen", Screen.width + "x" + Screen.height,
                     "avail", Screen.desktopAvailableHeight,
                     "-> window", Math.round(h * root.faceAspect) + "x" + Math.round(h))
        setGeometry(Math.round(h * root.faceAspect), Math.round(h))
    }

    // Sizing then showing, in that order, and with the aspect lock held off
    // until both dimensions are in.
    function setGeometry(w, h) {
        fixingAspect = true
        width  = w
        height = h
        if (geom.x >= 0 && geom.y >= 0) {   // remembered position, dogfood #2
            root.x = geom.x
            root.y = geom.y
        }
        fixingAspect = false
        visible = true
    }

    // The window always keeps the face's proportions.
    //
    // Dogfood #1 dropped the Ctrl escape hatch that used to be here: unlocking
    // could only ever add letterbox bars, never squash the calculator, so it
    // bought a mode for no visible effect.
    property bool fixingAspect: false


    function keepAspect(drivenByWidth) {
        if (fixingAspect || !visible || faceAspect <= 0)
            return
        fixingAspect = true
        if (drivenByWidth) height = Math.round(width / faceAspect)
        else               width  = Math.round(height * faceAspect)
        fixingAspect = false
    }

    onWidthChanged:  { keepAspect(true);  saveGeometry.restart() }
    onHeightChanged: { keepAspect(false); saveGeometry.restart() }
    onXChanged:      saveGeometry.restart()
    onYChanged:      saveGeometry.restart()

    Connections {
        target: engine.skin
        function onChanged() { root.applyDefaultGeometry() }
        // A skin that fails LATER - someone loads their own and it is broken -
        // says so on the calculator rather than only in the settings window.
        function onLastErrorChanged() {
            if (engine.skin.lastError !== "")
                banner.show(engine.skin.lastError)
        }
    }

    // A window that has never been shown cannot report why it has not been
    // shown. Waiting for the skin was written as an early return, and the only
    // retry is SkinModel's changed(), which it emits ONLY on success - so a
    // skin that fails to load left this window invisible for the rest of the
    // session: no calculator, no settings window, no message, and nothing in
    // the log. That is a black screen that cannot be diagnosed from the outside,
    // and it is what the first Android build came up as.
    //
    // So the wait now has an end. The skin loads in Agape48Engine's constructor,
    // which is before any of this exists, so on a healthy start geometryApplied
    // is already true when this fires and it does nothing at all. When it does
    // fire, faceW/faceH fall back to 480x900 on their own, and the window
    // arrives with the reason written across it.
    Timer {
        id: showAnyway
        interval: 1500
        running: true
        onTriggered: {
            if (root.geometryApplied)
                return
            console.warn("agape48: the skin never arrived, showing the window "
                         + "at the fallback size.", engine.skin.lastError)
            root.applyDefaultGeometry(true)
            banner.show(engine.skin.lastError !== ""
                        ? engine.skin.lastError
                        : qsTr("The calculator's face did not load, so this "
                               + "window is at its fallback size."))
        }
    }

    Agape48Engine {
        id: engine
        onRomRequired: settings.open()
        // ON could not take the calculator back. Who has it, and what can be
        // done about it, rather than a dead window and no reason.
        onAttachRefused: (h) => handover.showFor(h)
        onBeep: (hz, ms) => feedback.beep(hz, ms)
        onKeyFeedback: (keyId) => feedback.tap()
        onLastErrorChanged: if (lastError) banner.show(lastError)
        onNotice: (text) => banner.hint(text)
    }

    // "Customize keyboard…" mode: a plain click on the face opens that key's
    // binding dialog instead of pressing it. Ctrl+click does the same at any
    // time; this is the half of the pair that can be discovered, which is what
    // design item 10d is about.
    property bool customizing: false

    // Nothing of its own; it exists to be asked where the system bars are. Qt
    // reports the safe area as an attached property of an item, and an item
    // whose own margins came from its own safe area would be describing a
    // circle - so this one fills the window unconditionally and the calculator
    // reads the answer off it.
    Item {
        id: safeArea
        anchors.fill: parent
    }

    // Inset by the system bars on Android, and by nothing at all anywhere else,
    // because the margins are zero there. Measured on the phone at ef76dd2: the
    // status bar clock and the notification icons sat on top of the calculator's
    // own HEWLETT-PACKARD line, because an app targeting SDK 35 draws edge to
    // edge whether it planned to or not.
    Calculator {
        id: calculator
        anchors.fill: parent
        anchors.topMargin: safeArea.SafeArea.margins.top
        anchors.bottomMargin: safeArea.SafeArea.margins.bottom
        anchors.leftMargin: safeArea.SafeArea.margins.left
        anchors.rightMargin: safeArea.SafeArea.margins.right
        engine: engine
        customizing: root.customizing
        onRemapRequested: (k) => rebind.openFor(k)
        onCustomizeCancelled: root.customizing = false
        onBodyPressed: engine.startSystemMove(root)
        onUnassignedKey: (label) => banner.hint(
            qsTr("%1 is not assigned to any key. Ctrl+right-click a key to give it one.")
                .arg(label))
    }

    KeyBindingWindow {
        id: rebind
        engine: engine
        onOpenedChanged: focusGuard.restart()
    }

    // The mode has to say it is on and how to leave, or it is a trap.
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: modeText.implicitHeight + 18
        visible: root.customizing
        color: "#1f3a5f"
        Text {
            id: modeText
            anchors { fill: parent; margins: 9 }
            color: "#dce8f5"; font.pixelSize: TextSizes.banner; wrapMode: Text.WordWrap
            text: qsTr("Click a key to change its keyboard shortcut. Click here, or press Esc, when you are done.")
        }
        MouseArea { anchors.fill: parent; onClicked: root.customizing = false }
    }

    // No Shortcut here for Esc. A Shortcut grabs its sequence across every
    // window the app owns, hidden ones included, so one declared for this mode
    // ate the binding dialog's own Esc as well. Keypad.qml handles it instead,
    // where it cannot reach past this window.

    // --- who owns the keyboard ----------------------------------------------
    //
    // Keypad.qml declares focus: true, which is granted once, at creation, and
    // never again. Anything that takes focus and then goes away leaves it on
    // the floor and the keyboard dead for the rest of the session - and there
    // is no way out, because clicking the face does not restore it: a
    // MultiPointTouchArea never takes focus.
    //
    // Dogfood #5 line 15 was that bug: open Settings, click any text field
    // in it, close it, and every key was dead from then on, ESC included,
    // while the mouse still worked. The sheet that caused it is a separate
    // window now, but the guard stays - the next thing to take focus and
    // drop it will not announce itself either.
    //
    // So the keypad gets the keyboard back whenever nothing else wants it.
    // The two things that legitimately want it say so, and are asked first.
    onActiveFocusItemChanged: focusGuard.restart()

    Timer {
        id: focusGuard
        interval: 50            // let a popup finish taking focus before judging
        onTriggered: {
            // The question is not "who has focus" - after the sheet hides, its
            // text field is still what activeFocusItem names, so testing for
            // null missed it. The question is "does the keypad have it", and
            // if not, whether anything with a better claim is on screen.
            if (settings.opened || appMenu.opened || rebind.opened
                    || picker.opened || handover.opened)
                return
            if (!calculator.hasKeyboardFocus())
                calculator.grabKeyboardFocus()
        }
    }

    // --- the menu -----------------------------------------------------------
    //
    // Decision 2c makes Droid48's overflow menu the whole UI spec, and until
    // now none of it existed: the settings window's only trigger was onRomRequired,
    // which stopped firing the moment start() learned to find a ROM beside the
    // state. The sheet was unreachable.
    //
    // One way in as of 2026aug30, Gert's call: the ⋮ button. Right-click on the
    // face used to open this too; it is gone, so the only click gesture the
    // face answers to is Ctrl+click, which rebinds a key.
    // Two of Droid48's six items are missing because the features are: "show
    // minimal controls" has nothing to show, and "put program on stack" is the
    // object import from 2b, not built yet.
    Menu {
        id: appMenu
        onClosed: focusGuard.restart()
        MenuItem {
            text: qsTr("Open another calculator…")
            onTriggered: picker.openPicker()
        }
        MenuItem { text: qsTr("Settings…");       onTriggered: settings.open() }
        MenuItem {
            text: qsTr("Customize keyboard…")
            onTriggered: root.customizing = true
        }
        MenuItem { text: qsTr("Save memory now"); onTriggered: engine.saveState() }
        MenuSeparator {}
        MenuItem { text: qsTr("Copy stack");      onTriggered: engine.copyStackToClipboard() }
        MenuItem { text: qsTr("Paste");           onTriggered: engine.pasteClipboardToStack() }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Import file to stack…")
            onTriggered: importPicker.open()
        }
        MenuItem {
            text: qsTr("Export from stack to file…")
            // Ask before the dialog, not after: being told there is nothing to
            // export once you have already typed a filename is the wrong way
            // round. Dogfood #15 line 6.
            onTriggered: {
                if (engine.hasStackObject())
                    exportPicker.open()
                else
                    banner.show(qsTr("There is nothing on level 1 to export."))
            }
        }
        MenuSeparator {}
        MenuItem {
            text: qsTr("Reset memory and quit")
            onTriggered: { engine.reset(true); Qt.quit() }
        }
        MenuItem {
            text: qsTr("Quit")
            onTriggered: { engine.saveState(); Qt.quit() }
        }
    }

    // Raw QtQuick, not a Controls Button: the Quick Controls usage rule from
    // 2026aug28 keeps Controls off the calculator face, and this sits on it.
    Rectangle {
        id: menuButton
        anchors { top: parent.top; right: parent.right; margins: 8 }
        width: 28; height: 28; radius: 14
        color: menuMouse.containsMouse ? "#ffffff" : "#000000"
        opacity: menuMouse.containsMouse ? 0.22 : 0.28
        Text {
            anchors.centerIn: parent
            text: "\u22ee"
            color: "#e8e8e8"; font.pixelSize: TextSizes.dialogTitle
        }
        MouseArea {
            id: menuMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: appMenu.popup(menuButton.x, menuButton.y + menuButton.height)
        }
    }

    // A window of its own since 2026aug29, not a sheet over the face. That is
    // also why the focus bug below cannot come back through this door: closing
    // a separate window reactivates the main one, and onActiveChanged hands the
    // keyboard back.
    // Object interchange. One format both ways - "HPHP48-" and the object's raw
    // nibbles - which is what every emulator in this family and a real HP 48
    // over Kermit already agree on. A library is not a special case: it is an
    // object like any other, so it travels through these same two items.
    FileDialog {
        id: importPicker
        title: qsTr("Choose an HP 48 object file")
        nameFilters: [qsTr("All files (*)"),
                      qsTr("HP 48 objects (*.hp *.HP *.lib *.LIB *.bin *.BIN *.48 *.obj)")]
        // Agape48 presses ON itself afterwards so the ROM repaints the stack -
        // Gert's choice in dogfood #16, knowing that ON is also CANCEL. Any
        // latched shift is cancelled first, or ON would be OFF.
        onAccepted: if (engine.importFile(selectedFile))
                        banner.hint(qsTr("Imported to level 1."))
    }

    FileDialog {
        id: exportPicker
        title: qsTr("Save the object on level 1 as")
        fileMode: FileDialog.SaveFile
        nameFilters: [qsTr("All files (*)"), qsTr("HP 48 objects (*.hp)")]
        onAccepted: if (engine.exportFile(selectedFile))
                        banner.hint(qsTr("Level 1 saved to %1").arg(engine.urlToPath(selectedFile)))
    }

    HandoverWindow {
        id: handover
        engine: engine
        transientParent: root
        onPickAnother: picker.openPicker()
        onChangeFolder: settings.open()
    }

    CalculatorPickerWindow {
        id: picker
        engine: engine
        transientParent: root
        onBusyCalculator: (name, holder) => handover.showFor(holder, name)
    }

    SettingsWindow {
        id: settings
        engine: engine
        onOpenedChanged: focusGuard.restart()
    }

    // Error banner. Raw QtQuick: Quick Controls was allowed on 2026aug28, but
    // the usage rule from that decision keeps it off the calculator face, and
    // this banner sits on the face.
    //
    // At the BOTTOM since dogfood #8. It used to be along the top, which is
    // exactly where the settings window opens - so the one error that opens
    // the settings window by itself, "no ROM", posted its explanation behind
    // the window it had just raised. The settings window now repeats it too.
    Rectangle {
        id: banner
        property bool isError: true
        // Errors stay up long enough to read twice; a hint about a key nobody
        // claimed is not an error and goes after three seconds, as asked.
        function show(msg) { isError = true;  text.text = msg; opacity = 1; hideTimer.interval = 12000; hideTimer.restart() }
        function hint(msg) { isError = false; text.text = msg; opacity = 1; hideTimer.interval = 3000;  hideTimer.restart() }
        // Over the centre of the calculator's SCREEN since 2026sep03, on Gert's
        // instruction. Along the bottom it lay across the bottom two rows of
        // keys; the top is still not available, for the reason above; and the
        // screen is where he is already looking. It also lands on a blank LCD
        // in the case that matters most - a calculator handed to the other
        // machine has nothing on its screen to cover.
        anchors { left: parent.left; right: parent.right }
        y: Math.max(0, Math.min(root.height - height,
                                calculator.lcdCenterY - height / 2))
        height: text.implicitHeight + 24
        color: banner.isError ? "#8c1d18" : "#33383f"
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 180 } }
        // Clearing the engine's error too, not just hiding the strip: the
        // settings window shows lastError, so a failed import was still on
        // display there long after the banner had gone. Dogfood #15 line 20.
        //
        // But NOT WHEN THERE IS NO CALCULATOR. A message about something that
        // went wrong while the machine is running has done its job after twelve
        // seconds; a message saying why there is no machine at all is the only
        // thing on screen that explains the state the program is in, and wiping
        // it leaves a dead calculator and no reason anywhere.
        //
        // That is the whole of the "Windows says nothing, Linux says something"
        // difference, and it was neither: same run, same window, the strip is
        // there at six seconds and gone at twenty-two. Windows measured late and
        // Linux measured early. Gert's own machine is in this state right now -
        // its remembered shelf was deleted - so what it gives him is a complete
        // calculator, a blank screen and, twelve seconds in, silence.
        function forget() {
            banner.opacity = 0
            if (banner.isError && engine.running)
                engine.clearError()
        }
        Timer {
            id: hideTimer
            interval: 12000
            onTriggered: banner.forget()
        }
        Text {
            id: text
            anchors { fill: parent; margins: 12 }
            color: "white"; wrapMode: Text.WordWrap; font.pixelSize: TextSizes.banner
        }
        // Same on a deliberate dismissal, or the error the user just waved away
        // reappears the next time Settings is opened.
        MouseArea {
            anchors.fill: parent
            onClicked: banner.forget()
        }
    }

    // The resize border. It sits above everything and hands back any press that
    // is not within `margin` of an edge, so a click on a key still reaches the
    // keypad underneath.
    MouseArea {
        id: resizeBorder
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        readonly property int margin: 6

        function edgesAt(x, y) {
            let e = 0
            if (x < margin)                 e |= Qt.LeftEdge
            if (x > width - margin)         e |= Qt.RightEdge
            if (y < margin)                 e |= Qt.TopEdge
            if (y > height - margin)        e |= Qt.BottomEdge
            return e
        }

        cursorShape: {
            const e = edgesAt(mouseX, mouseY)
            if (e === (Qt.LeftEdge | Qt.TopEdge) || e === (Qt.RightEdge | Qt.BottomEdge))
                return Qt.SizeFDiagCursor
            if (e === (Qt.RightEdge | Qt.TopEdge) || e === (Qt.LeftEdge | Qt.BottomEdge))
                return Qt.SizeBDiagCursor
            if (e & (Qt.LeftEdge | Qt.RightEdge))  return Qt.SizeHorCursor
            if (e & (Qt.TopEdge | Qt.BottomEdge))  return Qt.SizeVerCursor
            return Qt.ArrowCursor
        }

        // Resized here rather than by QWindow::startSystemResize. The window
        // manager's own resize is interactive and ignores the height we set
        // from onWidthChanged while the drag is running, so the aspect lock
        // never got a word in and the window ended up the wrong shape - 523x756
        // where 451x756 went in. Driving it ourselves keeps the proportions
        // exact on every frame of the drag, which also means the letterbox
        // bars Gert saw in dogfood #9 never appear at all.
        property int  edges: 0
        property real startW: 0
        property real startH: 0
        property real startX: 0
        property real startY: 0
        property point startCursor: Qt.point(0, 0)

        onPressed: (mouse) => {
            const e = edgesAt(mouse.x, mouse.y)
            if (e === 0) {
                mouse.accepted = false      // not an edge: let the keypad have it
                return
            }
            edges = e
            startW = root.width;  startH = root.height
            startX = root.x;      startY = root.y
            startCursor = mapToGlobal(mouse.x, mouse.y)

            // Hold the aspect lock off for the WHOLE drag, not around each
            // setGeometry call. While a drag is running this handler already
            // derives both dimensions from faceAspect, so keepAspect() has
            // nothing left to contribute - all it can do is re-derive the same
            // numbers one frame late, and each of those is another window
            // geometry request and another full repaint of the face.
            //
            // Guarding only the call itself assumed widthChanged/heightChanged
            // fire synchronously inside it. On Windows they do not:
            // QWindow::setGeometry is SetWindowPos, and Qt delivers the
            // resulting geometry change after it returns, by which point the
            // guard had already been cleared. So one intended resize became
            // three - the size we asked for, then keepAspect(true) correcting
            // the height, then keepAspect(false) correcting the width back.
            // That is dogfood windows-01 line 14: "one direction first, then a
            // redraw top to bottom, then a third at the same size".
            root.fixingAspect = true

            // Seed the outline at the window's current shape and show it, so it
            // appears exactly over the calculator rather than springing in from
            // wherever the last drag left it.
            pendingW = startW; pendingH = startH
            pendingX = startX; pendingY = startY
            pendingGeometry = false
            outline.visible = !engine.liveResize

        }

        onPositionChanged: (mouse) => {
            if (edges === 0 || root.faceAspect <= 0)
                return
            const g = mapToGlobal(mouse.x, mouse.y)
            aimAt(g.x, g.y)
        }

        // The target shape for a pointer at (gx, gy), in screen coordinates.
        // Absolute, never incremental: it depends only on the cursor and on what
        // was captured at press, so a dropped or coalesced move event costs
        // nothing - the next one lands in exactly the right place.
        function aimAt(gx, gy) {
            if (edges === 0 || root.faceAspect <= 0)
                return
            // One dimension drives; the other follows from the face's ratio.
            let dw = 0
            if      (edges & Qt.RightEdge)  dw =   gx - startCursor.x
            else if (edges & Qt.LeftEdge)   dw = -(gx - startCursor.x)
            else if (edges & Qt.BottomEdge) dw =  (gy - startCursor.y) * root.faceAspect
            else if (edges & Qt.TopEdge)    dw = -(gy - startCursor.y) * root.faceAspect

            const w = Math.max(220, Math.round(startW + dw))
            const h = Math.round(w / root.faceAspect)

            // Compared against the OUTLINE's current shape, not the window's.
            // The window deliberately lags behind now, so testing against it
            // would skip every move until the drag had already moved a full
            // window's worth.
            if (w === pendingW && h === pendingH) {
                return
            }

            // One call, not four. Setting x, y, width and height separately is
            // four window-geometry requests per mouse event, and the window
            // visibly steps through the intermediate shapes - dogfood #12
            // line 4, "a bit jerky when the size is changing". setGeometry is
            // a single XMoveResizeWindow, so the window arrives in one piece.
            const nx = (edges & Qt.LeftEdge) ? Math.round(startX + (startW - w)) : root.x
            const ny = (edges & Qt.TopEdge)  ? Math.round(startY + (startH - h)) : root.y
            // The window is NOT resized here. The outline follows the pointer
            // and the real geometry is applied when the drag ends - Gert's
            // design, dogfood windows-02, after four other approaches failed.
            //
            // It works because it removes the cause rather than managing it.
            // Every artefact chased in that round came from the window changing
            // shape mid-drag: the OS resizes it and blits the old pixels into
            // the new rectangle a frame before our paint arrives, and when the
            // left or top edge moves, that blit lands the whole face at a
            // shifted position. A window that does not move cannot do any of
            // that. The outline has no content to misplace.
            pendingW = w; pendingH = h; pendingX = nx; pendingY = ny
            pendingGeometry = true
            if (!engine.liveResize)
                settle.restart()
        }

        property bool pendingGeometry: false
        property int  pendingW: 0
        property int  pendingH: 0
        property int  pendingX: 0
        property int  pendingY: 0

        // Live mode only. Mouse events arrive faster than the screen refreshes
        // - measured 9-12 ms apart on a fast drag against a 16.7 ms frame - and
        // a second SetWindowPos inside one frame is a geometry the compositor
        // never presents, so it is pure cost. This applies at most one per
        // rendered frame. Measured: 23 mouse moves became 11 geometry changes,
        // and the duplicated repaints Windows was reporting went away.
        FrameAnimation {
            running: resizeBorder.edges !== 0 && engine.liveResize
            onTriggered: resizeBorder.applyPending()
        }

        // Hold the pointer still for a second and the window catches up without
        // letting go, so the result can be seen before committing to it.
        Timer {
            id: settle
            interval: 1000
            onTriggered: resizeBorder.applyPending()
        }

        function applyPending() {
            if (!pendingGeometry)
                return
            pendingGeometry = false
            if (pendingW === root.width && pendingH === root.height
                && pendingX === root.x && pendingY === root.y)
                return
            engine.setWindowGeometry(root, pendingX, pendingY, pendingW, pendingH)
            saveGeometry.restart()         // fixingAspect is held by onPressed
        }

        onReleased: (mouse) => {
            settle.stop()
            const g = mapToGlobal(mouse.x, mouse.y)
            aimAt(g.x, g.y)
            applyPending()
            edges = 0; root.fixingAspect = false
            outline.visible = false
        }
        onCanceled: {
            settle.stop()
            edges = 0; root.fixingAspect = false
            outline.visible = false
        }
    }

    // The rubber band. One window, created at the size of the screen and never
    // resized - only the rectangle drawn inside it moves - so the overlay itself
    // never triggers the geometry-change artefact it exists to avoid.
    //
    // Not a real XOR rectangle on the desktop DC, which is how Windows did this
    // in 1995: under DWM the compositor repaints over it at unpredictable
    // moments and leaves droppings behind. A transparent always-on-top window
    // is the modern equivalent and works the same way on X11.
    //
    // WindowTransparentForInput matters - without it the overlay swallows the
    // very drag it is drawing.
    Window {
        id: outline
        flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
               | Qt.WindowTransparentForInput | Qt.Tool
        color: "transparent"
        visible: false

        x: root.screen ? root.screen.virtualX : 0
        y: root.screen ? root.screen.virtualY : 0
        width:  root.screen ? root.screen.width  : 1920
        height: root.screen ? root.screen.height : 1080

        Rectangle {
            // Frame only. The fill has to stay transparent or the overlay hides
            // whatever is behind it, the calculator included.
            color: "transparent"
            border.color: "#e8e8ea"
            border.width: 2
            antialiasing: false

            x: resizeBorder.pendingX - outline.x
            y: resizeBorder.pendingY - outline.y
            width:  resizeBorder.pendingW
            height: resizeBorder.pendingH
        }
    }

    // Haptics and beeps. Both are platform calls, not Qt Multimedia - see
    // README "Sound and haptics" for why QSoundEffect is not an option here.
    // TODO: add src/bridge/Feedback.{h,cpp} as a QML_SINGLETON wrapping
    //   Android : Vibrator / VibrationEffect + AudioTrack, via QJniObject
    //   Linux   : libcanberra-less ALSA square wave, or nothing
    //   Windows : Beep() on a worker, or WASAPI for a real waveform
    QtObject {
        id: feedback
        function tap()        { if (engine.hapticsEnabled) { /* Feedback.vibrate(12) */ } }
        function beep(hz, ms) { if (engine.soundEnabled)   { /* Feedback.tone(hz, ms) */ } }
    }

    Component.onCompleted: {
        applyDefaultGeometry()
        engine.start()
    }

    // Saved as it changes rather than in onClosing, which did not fire on the
    // dogfood machine and left the size unrestored. The timer keeps a resize
    // drag from writing QSettings on every frame.
    Timer {
        id: saveGeometry
        interval: 400
        onTriggered: if (root.geometryApplied) {   // never store the pre-skin fallback
            geom.w = root.width
            geom.h = root.height
            geom.x = root.x
            geom.y = root.y
        }
    }

    onActiveChanged: {
        if (active) { engine.resumeFromBackground(); focusGuard.restart() }
        else engine.suspend()
    }
}
