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

    // touch point id -> key index, so a release hits the key that was pressed
    // even if the finger has drifted off it in the meantime.
    property var held: ({})

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
            for (const p of points) {
                const i = root.keyIndexAt(p.x, p.y)
                if (i >= 0) { root.held[p.pointId] = i; root.pressIndex(i) }
            }
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

    // Pressed-key artwork, driven off the same held map.
    Repeater {
        model: root.engine.skin.keys
        delegate: Rectangle {
            required property var modelData
            required property int index
            x: modelData.rect.x; y: modelData.rect.y
            width: modelData.rect.width; height: modelData.rect.height
            radius: Math.min(width, height) * 0.12
            color: "#ffffff"
            opacity: {
                for (const id in root.held)
                    if (root.held[id] === index) return 0.18
                return 0
            }
            Behavior on opacity { NumberAnimation { duration: 60 } }
        }
    }

    // Physical keyboard, desktop and Android with a hardware keyboard.
    // TODO: full Qt.Key -> HP 48 name table; the shift/alpha modifiers need
    // care because the HP 48 has its own shift state.
    focus: true
    Keys.onPressed: (event) => {
        const name = Agape48Keymap.nameFor(event.key, event.modifiers)
        if (name) { root.engine.pressKey(name); event.accepted = true }
    }
    Keys.onReleased: (event) => {
        const name = Agape48Keymap.nameFor(event.key, event.modifiers)
        if (name) { root.engine.releaseKey(name); event.accepted = true }
    }
}
