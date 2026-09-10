import QtQuick
import Agape48

// Multi-touch keypad.
//
// One MultiPointTouchArea over the whole face rather than a MouseArea per key.
// That is not a style choice: ON+A+F is the HP 48 hard reset and needs three
// keys held down at once. Per-key MouseAreas serialise touches and would make
// the combination unreachable, which is exactly the bug users hit in ports that
// take the easy route.
Item {
    id: root
    required property Agape48Engine engine

    // Ctrl+RIGHT-click a key to rebind it. Gert asked for Ctrl+left first and
    // changed it after using it: a left button is what you press all day on
    // this face, so Ctrl+left is one stuck modifier away from an accident,
    // while nothing else uses the right button here at all. While "Customize
    // keyboard…" is on, a plain click does the same, which is the discoverable
    // half of the pair - see design item 10d.
    property bool customizing: false
    signal remapRequested(var keyModel)
    signal customizeCancelled()
    signal unassignedKey(string label)
    signal bodyPressed()

    // touch point id -> key index, so a release hits the key that was pressed
    // even if the finger has drifted off it in the meantime.
    property var held: ({})

    // Physical scan code -> the calculator key it pressed, so a release lets go
    // of the same one even if the modifiers changed in between. On a Danish
    // layout * is Shift+', and letting go of Shift first would otherwise
    // release QUOTE and leave MUL held down for good.
    property var downByScan: ({})

    // Shift is not pressed through to the calculator until it is let go, and
    // then only if nothing else was typed while it was down. See the comment on
    // the release handler.
    property bool shiftHeld: false
    property bool shiftUsed: false

    function scanOf(event) { return event.nativeScanCode || ("k" + event.key) }

    function keyIndexAt(px, py) {
        const keys = root.engine.skin.keys
        for (let i = 0; i < keys.length; ++i) {
            const r = keys[i].rect
            if (px >= r.x && px < r.x + r.width && py >= r.y && py < r.y + r.height)
                return i
        }
        return -1
    }

    function pressIndex(i) {
        const k = root.engine.skin.keys[i]
        if (k.key !== "") root.engine.pressKey(k.key)
        else              root.engine.pressCode(k.row, k.mask)
    }

    function releaseIndex(i) {
        const k = root.engine.skin.keys[i]
        if (k.key !== "") root.engine.releaseKey(k.key)
        else              root.engine.releaseCode(k.row, k.mask)
    }

    MultiPointTouchArea {
        anchors.fill: parent
        minimumTouchPoints: 1
        maximumTouchPoints: 5
        mouseEnabled: true          // desktop: the mouse acts as one touch point

        onPressed: (points) => {
            // Touching the calculator is the user's own way back if the
            // keyboard has been stolen - see the focus guard in Main.qml.
            root.forceActiveFocus()

            // Only the customize mode goes through here now: a right button
            // never reaches a MultiPointTouchArea, so Ctrl+right-click is
            // handled by the MouseArea below instead.
            if (root.customizing && points.length > 0) {
                const hit = root.keyIndexAt(points[0].x, points[0].y)
                if (hit >= 0)
                    root.remapRequested(root.engine.skin.keys[hit])
                return          // a remap click must not also press the key
            }

            let hitSomething = false
            for (const p of points) {
                const i = root.keyIndexAt(p.x, p.y)
                if (i >= 0) {
                    root.held[p.pointId] = i
                    root.shiftUsed = true       // a click counts as "Shift was for this"
                    root.pressIndex(i)
                    hitSomething = true
                }
            }
            // Nothing under the finger but the body or the display. With no
            // title bar that is the drag handle, which is how Emu48 behaves.
            if (!hitSomething)
                root.bodyPressed()
        }
        onReleased: (points) => {
            for (const p of points) {
                const i = root.held[p.pointId]
                if (i !== undefined) { root.releaseIndex(i); delete root.held[p.pointId] }
            }
        }
        onCanceled: (points) => {
            // Gesture stolen by the system (notification shade, call). Let go of
            // everything, or the calculator sits with a key wedged down.
            root.held = ({})
            root.engine.releaseAllKeys()
        }
    }

    // Pressed-key artwork.
    //
    // Driven off engine.pressedKeys, NOT off the held map above. Two reasons,
    // and the first one is why nothing lit up at all in dogfood #4: `held` is
    // a var holding a JS object, and writing held[id] = i mutates that object
    // without ever assigning to the property, so no change signal fires and a
    // binding that reads it is never re-evaluated. The second is that `held`
    // only ever knew about touch points, so a key pressed on the physical
    // keyboard could not have lit up even once the binding worked.
    //
    // Drawn over "cap", the key as painted, not over "rect", which is the hit
    // area and is deliberately a few pixels larger on every side. Lighting up
    // the hit area would put a halo around the key instead of lightening it.
    Repeater {
        model: root.engine.skin.keys
        delegate: Rectangle {
            required property var modelData
            x: modelData.cap.x; y: modelData.cap.y
            width: modelData.cap.width; height: modelData.cap.height
            // 0.16 is cap_radius_frac in tools/face.json, which is what the
            // cap under this rectangle was drawn with.
            radius: height * 0.16
            color: "#ffffff"
            opacity: root.engine.pressedKeys.indexOf(modelData.key) >= 0 ? 0.22 : 0
            Behavior on opacity { NumberAnimation { duration: 60 } }

            // Two seconds of a still pointer names the keyboard keys bound
            // here. See the tooltip below.
            HoverHandler {
                onHoveredChanged: {
                    if (hovered) {
                        root.tipKey = parent.modelData
                        tipBox.shown = false
                        tipDelay.restart()
                    } else if (root.tipKey === parent.modelData) {
                        tipDelay.stop()
                        tipBox.shown = false
                        root.tipKey = null
                    }
                }
            }
        }
    }

    // --- what is this key on my keyboard? ------------------------------------
    //
    // Gert, 2026sep10: "When the mouse pointer halts for 2 seconds over a button
    // on the calculator's face, each of it's assigned keyboard keys appears at a
    // yellow tooltip, between angle brackets <>, and when they are more than one,
    // separated by a newline."
    //
    // READ FROM Agape48Keymap, NEVER FROM A SECOND TABLE. bindingsFor() is the
    // same function KeyBindingWindow lists and it already folds the user's
    // overrides over the defaults, so a rebound key says the truth here the
    // moment it is rebound. A copy of the map would start lying the first time
    // he changed a binding - the constraint written down with the request in
    // docs/design-questions.md.
    //
    // RAW QtQuick, not Controls' ToolTip: the usage rule of 2026aug28 keeps
    // Quick Controls off the calculator face, and this is on it. Which is no
    // loss, because the yellow he asked for is not what the Basic style paints
    // anyway.
    //
    // Desktop in effect rather than by a platform test: a finger has no hover
    // state, so HoverHandler never fires on a phone and this costs nothing there.
    property var tipKey: null

    readonly property string tipText: {
        if (!tipKey)
            return ""
        const rows = Agape48Keymap.bindingsFor(tipKey.key)
        if (!rows.length)
            return ""
        return rows.map(function (r) { return "<" + r.label + ">" }).join("\n")
    }

    Timer {
        id: tipDelay
        interval: 2000
        onTriggered: tipBox.shown = true
    }

    Rectangle {
        id: tipBox
        property bool shown: false

        visible: shown && root.tipText.length > 0
        z: 100
        // Under the key rather than over it, so the pointer is never on top of
        // the words, and clamped to the face so a key at the right edge does
        // not push its tooltip off the window.
        x: root.tipKey ? Math.min(Math.max(0, root.tipKey.cap.x),
                                  Math.max(0, root.width - width))
                       : 0
        y: root.tipKey ? root.tipKey.cap.y + root.tipKey.cap.height + 4 : 0
        width:  tipLabel.implicitWidth + 10
        height: tipLabel.implicitHeight + 6
        radius: 3
        color: "#fdf3a8"
        border.width: 1
        border.color: "#8a7c1e"

        Text {
            id: tipLabel
            anchors.centerIn: parent
            text: root.tipText
            // Dark ink on the yellow, which is the one combination that does
            // not depend on the system palette - the mistake the unsaved-path
            // dialog made with #e8e8e8 on a palette background.
            color: "#1b1b1b"
            font.pixelSize: TextSizes.dialogHint
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // Ctrl+right-click. A MultiPointTouchArea only ever sees the left button,
    // so the gesture needs a MouseArea of its own - and because it accepts only
    // the right button, a plain left click falls straight through to the keypad
    // underneath, exactly as it did before.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: (mouse) => {
            if (!(mouse.modifiers & Qt.ControlModifier))
                return
            const hit = root.keyIndexAt(mouse.x, mouse.y)
            if (hit >= 0)
                root.remapRequested(root.engine.skin.keys[hit])
        }
    }

    // Physical keyboard, desktop and Android with a hardware keyboard.
    // TODO: full Qt.Key -> HP 48 name table; the shift/alpha modifiers need
    // care because the HP 48 has its own shift state.
    focus: true
    Keys.onPressed: (event) => {
        // Esc leaves the customize mode. Handled here rather than by a Shortcut
        // in Main.qml, because a Shortcut goes on grabbing its sequence in
        // every window of the app - which is how Esc stopped closing the
        // binding dialog once the mode was on, dogfood #8 line 18.
        if (root.customizing && event.key === Qt.Key_Escape) {
            root.customizeCancelled()
            event.accepted = true
            return
        }
        // Shift held down is usually somebody reaching for a character on the
        // second level of a key - * is Shift+' on a Danish keyboard and Shift+8
        // on a US one. Pressing the calculator's own shift the moment Shift
        // goes down would shift the calculator as well, so nothing happens yet:
        // the decision is made on release, below.
        if (event.key === Qt.Key_Shift) {
            if (!event.isAutoRepeat) { root.shiftHeld = true; root.shiftUsed = false }
            event.accepted = true
            return
        }
        if (root.shiftHeld)
            root.shiftUsed = true

        const name = Agape48Keymap.nameFor(event.key, event.modifiers, event.text)
        if (name) {
            if (!event.isAutoRepeat)
                root.downByScan[root.scanOf(event)] = name
            root.engine.pressKey(name)
            event.accepted = true
            return
        }

        // Nothing claimed it. Say so, rather than looking broken - a key that
        // does nothing and a calculator that has hung look identical from the
        // outside. Auto-repeat is skipped so holding a key says it once, and
        // keys the system keeps for itself stay quiet: Fn on a ThinkPad would
        // otherwise complain every time it is used to reach F12.
        if (!event.isAutoRepeat
                && Agape48Keymap.isBindable(event.key, event.modifiers, event.text))
            root.unassignedKey(Agape48Keymap.labelFor(event.key, event.modifiers, event.text))
    }
    Keys.onReleased: (event) => {
        // Shift on its own, with nothing typed while it was down: it was meant
        // as the calculator's shift after all, so tap it now. Held while
        // something else was typed, it was a level selector and the calculator
        // never hears about it.
        if (event.key === Qt.Key_Shift) {
            if (!event.isAutoRepeat) {
                root.shiftHeld = false
                if (!root.shiftUsed) {
                    const n = Agape48Keymap.nameFor(event.key, 0, "")
                    if (n) { root.engine.pressKey(n); root.engine.releaseKey(n) }
                }
            }
            event.accepted = true
            return
        }
        const scan = root.scanOf(event)
        const name = root.downByScan[scan]
                     || Agape48Keymap.nameFor(event.key, event.modifiers, event.text)
        if (name) {
            root.engine.releaseKey(name)
            if (!event.isAutoRepeat)
                delete root.downByScan[scan]
            event.accepted = true
        }
    }
}
