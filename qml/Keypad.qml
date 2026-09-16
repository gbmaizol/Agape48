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

    // RIGHT-click a key to rebind it. The gesture was Ctrl+left first, then
    // Ctrl+right, then plain right on 2026sep10: a left button is what you
    // press all day on this face, so Ctrl+left was one stuck modifier away from
    // an accident, and then the Ctrl itself turned out to interfere with the
    // calculator, while nothing else uses the right button here at all. While
    // "Customize keyboard…" is on, a plain click does the same, which is the
    // discoverable half of the pair - see design item 10d.
    property bool customizing: false
    signal remapRequested(var keyModel)
    signal customizeCancelled()
    signal unassignedKey(string label)
    signal bodyPressed()

    // La fingro sur la korpo, kiu ankoraŭ ne movis la fenestron: ĝia pointId kaj
    // kie ĝi komencis, en scenaj koordinatoj.
    property int bodyPoint: -1
    property point bodyFrom: Qt.point(0, 0)

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
            // never reaches a MultiPointTouchArea, so right-click is handled by
            // the MouseArea below instead.
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
            //
            // LA FENESTRO MOVIĜAS NUR KIAM LA MONTRILO MOVIĜAS. Sur Vindozo
            // startSystemMove() transdonas la muson al la sistemo, kaj klako sen
            // movo tie perdas sian malpremon: Qt neniam ricevas ĝin, ĉi tiu areo
            // tenas la muson, kaj ĉiu posta premo venas ĉi tien anstataŭ al la
            // substrekita 48GX. Mezurite 2026sep15 per realaj klakoj: post klako
            // sur la korpo, ses klakoj sur 48GX dum ĉirkaŭ duona minuto malfermis
            // nenion, ĝis klako sur klavo, kaj la protokolo de Qt montris premon
            // sen malpremo. Tiro kun movo ricevas sian malpremon ĉe la fino de la
            // movo, kaj lasis 48GX funkcii.
            if (!hitSomething && root.bodyPoint < 0 && points.length > 0) {
                root.bodyPoint = points[0].pointId
                root.bodyFrom = Qt.point(points[0].sceneX, points[0].sceneY)
            }
        }
        onUpdated: (points) => {
            for (const p of points) {
                if (p.pointId !== root.bodyPoint)
                    continue
                const d = Qt.styleHints.startDragDistance
                if (Math.abs(p.sceneX - root.bodyFrom.x) > d || Math.abs(p.sceneY - root.bodyFrom.y) > d) {
                    root.bodyPoint = -1
                    root.bodyPressed()
                }
            }
        }
        onReleased: (points) => {
            for (const p of points) {
                const i = root.held[p.pointId]
                if (i !== undefined) { root.releaseIndex(i); delete root.held[p.pointId] }
                if (p.pointId === root.bodyPoint)
                    root.bodyPoint = -1
            }
            // KAJ LA ŜPRUCHELPILO FORIRAS KUN LA FINGRO. Sur labortablo la
            // montrilo restas post klako kaj la ŝvebo mem forprenas la
            // skatolon kiam ĝi moviĝas; sur tuŝekrano ne estas montrilo post
            // la levo, containsMouse de la rando neniam ŝanĝiĝas, kaj
            // hoverLeft() do neniam kuras - do skatolo kiu aperis restis sur
            // la ekrano senfine. Mezurite sur la telefono 2026sep12 kun
            // virtuala klavaro: teni la ŝovklavon montris <F7>/<Shift>, kaj ĝi
            // ankoraŭ pendis tie du premojn poste, super klavo kiun neniu
            // tuŝis.
            if (root.tipShown || tipDelay.running)
                root.hoverLeft()
        }
        onCanceled: (points) => {
            // Gesture stolen by the system (notification shade, call). Let go of
            // everything, or the calculator sits with a key wedged down.
            root.held = ({})
            root.bodyPoint = -1
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
        }
    }

    // --- what is this key on my keyboard? ------------------------------------
    //
    // 2026sep10, and this is the whole specification: when the pointer halts for
    // two seconds over a key on the face, that key's assigned keyboard keys
    // appear in a yellow box, each between angle brackets, one to a line.
    //
    // READ FROM Agape48Keymap, NEVER FROM A SECOND TABLE. bindingsFor() is the
    // same function KeyBindingWindow lists and it already folds the user's
    // overrides over the defaults, so a rebound key says the truth here the
    // moment it is rebound. A copy of the map would start lying the first time
    // a binding changed - the constraint written down with the request in
    // docs/design-questions.md.
    //
    // RAW QtQuick, not Controls' ToolTip: the usage rule of 2026aug28 keeps
    // Quick Controls off the calculator face, and this is on it. Which is no
    // loss, because the yellow the box is specified in is not what the Basic
    // style paints anyway.
    //
    // Desktop in effect rather than by a platform test: a finger has no hover
    // state, so nothing below ever fires on a phone and this costs nothing there.
    property var tipKey: null
    property point tipAnchor: Qt.point(-99, -99)

    // ĈU LA HORLOĜO FINIS. La flava skatolo mem ne plu estas ĉi tie: ĝi pendas
    // en Calculator.qml, ekster la vizaĝo, kaj legas ĉi tiujn tri proprecojn.
    // 2026sep10, en la konstruo de tiu tago: "It's cropped by the edge of the
    // calculator face, so the buttons at the edges are less than half-displayed.
    // Couldn't the scale problem and this problem be more elegantly solved by
    // making the tooltip arise from" - la frazo estas tranĉita, la respondo
    // estas jes, kaj la kialoj staras tie kie la skatolo nun vivas. Kio restas
    // ĉi tie estas la sensilo: kiu klavo, kie, kaj ĉu la montrilo haltis
    // sufiĉe longe.
    property bool tipShown: false

    // THERE IS NO HOVER SENSOR ON THIS FACE, AND THERE CANNOT BE ONE. Qt Quick
    // delivers hover front to back and stops at the first item whose subtree
    // accepts it, and Main.qml's resize border is a full-window MouseArea with
    // hoverEnabled sitting on top of everything - so no item on the calculator
    // has been able to receive a hover event since that border was added. Not
    // the sensor: the roof over it. Which is why a HoverHandler per key failed,
    // and then one HoverHandler on the keypad failed in exactly the same way,
    // and why two separate reports said the tooltips could not be seen.
    //
    // Measured in isolation with qml.exe rather than argued about: four pointer
    // positions over a window built like this one, four events at the border and
    // none at the item beneath it; take the border's hover away and all four
    // arrive. A non-blocking HoverHandler on the border instead of hoverEnabled
    // changes nothing, because a HoverHandler makes its own parent item accept
    // hover, and that is the thing that blocks.
    //
    // So the border feeds the pointer down here, the way it already hands
    // presses back when they are not on an edge. Scene coordinates in, so
    // nothing here depends on how deeply the face is nested in the window.
    //
    // "HALTS for 2 seconds", literally: any real movement restarts the timer,
    // so the tooltip appears two seconds after the pointer stops rather than
    // two seconds after it arrives. Three pixels of tolerance, because a hand
    // resting on a mouse is never quite still and a strict test would never
    // fire.
    function hoverAtScene(sx, sy) {
        if (root.customizing) {
            root.hoverLeft()
            return
        }
        const p = root.mapFromItem(null, sx, sy)
        const i = root.keyIndexAt(p.x, p.y)
        const k = i >= 0 ? root.engine.skin.keys[i] : null
        const moved = Math.abs(p.x - root.tipAnchor.x) > 3
                   || Math.abs(p.y - root.tipAnchor.y) > 3
        if (k !== root.tipKey) {
            root.tipKey = k
            root.tipShown = false
        }
        if (!k) {
            tipDelay.stop()
            root.tipShown = false
        } else if (moved) {
            root.tipAnchor = p
            root.tipShown = false
            tipDelay.restart()
        }
    }

    function hoverLeft() {
        tipDelay.stop()
        root.tipShown = false
        root.tipKey = null
        root.tipAnchor = Qt.point(-99, -99)
    }

    readonly property string tipText: {
        if (!tipKey)
            return ""
        const rows = Agape48Keymap.bindingsFor(tipKey.key)
        if (!rows.length)
            return ""
        return rows.map(function (r) { return "<" + r.label + ">" }).join("\n")
    }

    // NUR KIAM KLAVARO ESTAS KONEKTITA. Provo 17, la sola problemo kiun la
    // Androida duono trovis: klavo tenata longe en unu loko montris klavaran
    // fulmoklavon, kaj tio devas esti malŝaltita krom se klavaro estas
    // konektita. La regulo pravas dufoje - fingro kiu tenas klavon ne
    // demandas "kiu klavo de mia klavaro estas ĉi tiu", kaj sur telefono sen
    // klavaro la respondo estus nomo de klavo kiun neniu povas premi.
    //
    // ĈI TIE KAJ NE EN hoverAtScene(): keyboardAttached() estas JNI-voko sur
    // Androido, kaj hoverAtScene kuras dufoje po montrila movo. Ĉi tiu punkto
    // kuras maksimume unu fojon po du sekundoj kaj demandas ekzakte kiam la
    // respondo gravas, do klavaro alveninta post la lanĉo estas rimarkata.
    Timer {
        id: tipDelay
        interval: 2000
        onTriggered: root.tipShown = root.engine.keyboardAttached()
    }

    // Right-click, no modifier. A MultiPointTouchArea only ever sees the left
    // button, so the gesture needs a MouseArea of its own - and because it
    // accepts only the right button, a plain left click falls straight through
    // to the keypad underneath, exactly as it did before.
    //
    // The Ctrl came off on 2026sep10, because the Ctrl key interferes with the
    // calculator itself. Nothing else on this face uses the
    // right button, so the modifier was never earning anything.
    //
    // ANDROID REACHES THIS THE OTHER WAY. A touchscreen has no second button,
    // so the phone's route is the "Customize keyboard..." menu item and a plain
    // tap, handled in the MultiPointTouchArea above. A mouse attached to an
    // Android device does send Qt.RightButton and does land here.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        onClicked: (mouse) => {
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
