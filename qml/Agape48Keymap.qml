pragma Singleton
import QtQuick
import QtCore

// Physical keyboard -> HP 48 key name, with the user's own edits on top.
//
// WHAT IDENTIFIES A BINDING. Two kinds of key, because a keyboard has two kinds:
//
//   "c:*"        a character the keyboard produced.  Upper-cased, so a and A
//                are one binding, but * and ' are two.
//   "k:16777220:0"  everything with no character to it - Enter, arrows, F5,
//                   Shift - written as Qt.Key and modifiers.
//
// Characters are keyed on what came out, not on which key and which modifiers
// went in, and that is the whole point. Shift is not a modifier on a character
// key, it is a level selector: on a Danish layout * is Shift+' and / is Shift+7,
// on a US layout * is Shift+8. Treating "Shift" as part of the identity made
// both unreachable - dogfood #12 line 17, "the keyboard key * is not accessible
// from the keyboard". Now the calculator listens for the character you typed and
// does not care what it took to type it, which also makes the map the same on
// every layout.
//
// Ctrl and Meta still are modifiers, so Ctrl+A stays a key of its own.
//
// Gert's rule of 2026aug28 survives where it still applies: no fallback and no
// most-specific-match, one identity in, one calculator key out.
//
// WHERE THE USER MAP LIVES: settings.ini in the state folder, beside the ROM.
// Gert, 2026sep10, reversing design item 10b: "make this keyboard setting and
// all the settings that are like this live in the same folder as the ROM in a
// simple settings.ini text file, so they change and move together with the state
// folder." The folder is the calculator, and a calculator you carry to another
// machine should be the one you set up.
//
// The old argument for keeping it machine-local was that a "k:" binding is a
// Qt.Key and those move with the keyboard layout. Still true, and now the
// smaller risk of the two: a binding that lands on the wrong physical key on a
// different layout is one right-click away from being fixed, while a map that
// silently stays behind is a calculator that is not yours.
QtObject {
    id: root

    // --- storage -------------------------------------------------------------
    // QtObject has no default property, so the Settings objects below are held
    // by properties rather than declared as children.
    //
    // Set by Main.qml from engine.state.settingsFile, because a QML singleton
    // cannot see the engine object. Empty until then, and an empty location is
    // what QML's Settings reads as "use the default", so nothing is ever written
    // to a file nobody chose.
    property url storeUrl

    property Settings store: Settings {
        category: "keymap"
        location: root.storeUrl
        property string json: ""

        // A NEW FOLDER MEANS A NEW MAP. Settings re-reads its file synchronously
        // when the location changes, and emits the property change BEFORE
        // locationChanged, so this is the signal that arrives with the new
        // contents already in place - measured with qml.exe, not assumed.
        //
        // Declared in here rather than as a Connections block outside, because
        // QtObject has no default property and a bare child element of one does
        // not load at all: "Cannot assign to non-existent default property",
        // which is the same rule the comment above this object is about.
        onJsonChanged: root.load()
    }

    // The registry, where the map lived until 2026sep10. Read when the folder
    // has nothing to say, so an upgrade does not silently reset everybody's
    // bindings, and so a brand new folder starts from the map already in use.
    // Never written: the first edit after this goes to the folder.
    property Settings legacy: Settings {
        category: "keymap"
        property string json: ""
    }

    // id -> HP 48 key name, or "" meaning "this default is removed".
    property var overrides: ({})

    signal changed()

    function load() {
        const raw = root.store.json || root.legacy.json
        try {
            overrides = raw ? JSON.parse(raw) : ({})
        } catch (e) {
            overrides = ({})
        }
        root.changed()
    }

    function persist() {
        root.store.json = JSON.stringify(root.overrides)
        root.changed()
    }

    function resetToDefaults() {
        overrides = ({})
        persist()
    }

    Component.onCompleted: load()

    // --- identity ------------------------------------------------------------
    // KeypadModifier is dropped: the numeric keypad already sends its own key
    // codes where it differs (Key_Enter vs Key_Return), so keeping it would only
    // split the digits into two bindings nobody asked for.
    readonly property int modMask: Qt.ShiftModifier | Qt.ControlModifier
                                 | Qt.AltModifier   | Qt.MetaModifier

    // Did this key press put a character on the screen? Control characters do
    // not count: Esc carries "\x1b", Enter "\r", Backspace "\b", and those are
    // keys, not characters. Neither does anything held with Ctrl or Meta, which
    // are real modifiers - Ctrl+A must stay bindable as Ctrl+A. Alt is allowed
    // through because AltGr reaches the third level on most European layouts and
    // some X11 setups report it as Alt.
    function charOf(key, modifiers, text) {
        if (modifiers & (Qt.ControlModifier | Qt.MetaModifier))
            return ""
        if (!text || text.length !== 1)
            return ""
        const c = text.charCodeAt(0)
        if (c < 0x20 || c === 0x7f)
            return ""
        const up = text.toUpperCase()
        return up.length === 1 ? up : text      // ß upper-cases to SS; keep ß
    }

    function normalise(key, modifiers) {
        // Shift, Ctrl, Alt and Meta report themselves as modifiers while they
        // are held, which would make them unmatchable against their own
        // bindings. They are ordinary keys here - the HP 48's shifts are sticky
        // presses on SHL and SHR, not held modifiers.
        if (key === Qt.Key_Shift || key === Qt.Key_Control
                || key === Qt.Key_Alt || key === Qt.Key_Meta)
            return 0
        return modifiers & root.modMask
    }

    function idFor(key, modifiers, text) {
        const c = root.charOf(key, modifiers, text)
        if (c !== "")
            return "c:" + c
        return "k:" + key + ":" + root.normalise(key, modifiers)
    }

    // --- lookup --------------------------------------------------------------
    function nameForId(id) {
        if (id in root.overrides)
            return root.overrides[id]
        return root.defaults[id] || ""
    }

    function nameFor(key, modifiers, text) {
        // ESC is ON permanently and cannot be rebound or removed. It is the one
        // guarantee that the calculator can always be switched on, whatever the
        // user does to the rest of the map. Design item 10c.
        if (key === Qt.Key_Escape)
            return "ON"
        return root.nameForId(root.idFor(key, modifiers, text))
    }

    // Which calculator key owns this identity right now, "" if nobody.
    function ownerOfId(id) { return root.nameForId(id) }

    // Every physical key bound to one calculator key, defaults and edits both.
    // Each row is { id, label, locked }.
    function bindingsFor(name) {
        let rows = []
        if (name === "ON")
            rows.push({ id: "k:" + Qt.Key_Escape + ":0",
                        label: root.labelForId("k:" + Qt.Key_Escape + ":0"),
                        locked: true })
        for (const id in root.defaults)
            if (root.defaults[id] === name && !(id in root.overrides))
                rows.push({ id: id, label: root.labelForId(id), locked: false })
        for (const id in root.overrides)
            if (root.overrides[id] === name)
                rows.push({ id: id, label: root.labelForId(id), locked: false })
        rows.sort((a, b) => a.label.localeCompare(b.label))
        return rows
    }

    // --- editing -------------------------------------------------------------
    function bindId(id, name) {
        if (!root.isBindableId(id))
            return false
        let o = Object.assign({}, root.overrides)
        o[id] = name
        root.overrides = o
        root.persist()
        return true
    }

    function unbind(id) {
        let o = Object.assign({}, root.overrides)
        // Which row is this? One the user added, or one of ours?
        //
        // Removing a row the user ADDED drops the override, so whatever the
        // default said comes back. Gert stole 8 from the 8 key for the 7 key
        // and then removed it, and the old rule shadowed it instead - leaving
        // 8 pressing nothing at all, on either key. Taking a key and then
        // giving it back has to end where it started.
        //
        // Removing one of OUR defaults has to shadow it with an empty binding,
        // because a default cannot be deleted from the table.
        if (id in o)
            delete o[id]
        else
            o[id] = ""
        root.overrides = o
        root.persist()
    }

    // Everything this one calculator key ever had, back to the shipped map.
    function resetKey(name) {
        let o = {}
        for (const id in root.overrides) {
            const wasOurs = root.defaults[id]
            // Drop any override that either points at this key now or hid one
            // of this key's own defaults.
            if (root.overrides[id] === name || (root.overrides[id] === "" && wasOurs === name))
                continue
            o[id] = root.overrides[id]
        }
        root.overrides = o
        root.persist()
    }

    // --- display -------------------------------------------------------------
    readonly property var keyNames: ({
        [Qt.Key_Escape]: "Esc", [Qt.Key_Return]: "Enter",
        [Qt.Key_Enter]: "Enter (keypad)", [Qt.Key_Backspace]: "Backspace",
        [Qt.Key_Delete]: "Delete", [Qt.Key_Space]: "Space", [Qt.Key_Tab]: "Tab",
        [Qt.Key_Left]: "Left", [Qt.Key_Right]: "Right",
        [Qt.Key_Up]: "Up", [Qt.Key_Down]: "Down",
        [Qt.Key_PageUp]: "Page Up", [Qt.Key_PageDown]: "Page Down",
        [Qt.Key_Home]: "Home", [Qt.Key_End]: "End", [Qt.Key_Insert]: "Insert",
        [Qt.Key_Shift]: "Shift", [Qt.Key_Control]: "Ctrl",
        [Qt.Key_Alt]: "Alt", [Qt.Key_Meta]: "Meta"
    })

    function keyName(key) {
        if (key in root.keyNames)
            return root.keyNames[key]
        if (key >= Qt.Key_F1 && key <= Qt.Key_F35)
            return "F" + (key - Qt.Key_F1 + 1)
        // Latin-1, not just ASCII: a Danish keyboard has Æ, Ø and Å, and they
        // are ordinary letter keys that must be bindable and nameable.
        if (key >= 0x20 && key <= 0xff)
            return String.fromCharCode(key).toUpperCase()
        return "0x" + key.toString(16)
    }

    function labelForId(id) {
        if (id.substring(0, 2) === "c:") {
            const c = id.substring(2)
            return c === " " ? qsTr("Space") : c
        }
        const parts = id.split(":")
        const key = Number(parts[1])
        const m = Number(parts[2])
        let s = ""
        if (m & Qt.ControlModifier) s += "Ctrl+"
        if (m & Qt.AltModifier)     s += "Alt+"
        if (m & Qt.ShiftModifier)   s += "Shift+"
        if (m & Qt.MetaModifier)    s += "Meta+"
        return s + root.keyName(key)
    }

    function labelFor(key, modifiers, text) {
        return root.labelForId(root.idFor(key, modifiers, text))
    }

    // Identities we refuse to bind. Gert's ThinkPad needs Fn held to reach F12,
    // and Fn itself arrives as XF86WakeUp -> Qt.Key_WakeUp (0x010000b8), which
    // capture happily recorded as a binding named "0x10000b8" - a key the
    // calculator can never see again. The rule: if we cannot give it a name a
    // person would recognise, we have no business binding it. A character always
    // has a name - itself - so "c:" ids are always fine.
    function isBindableId(id) {
        if (id.substring(0, 2) === "c:")
            return true
        const key = Number(id.split(":")[1])
        if (key === Qt.Key_Escape)
            return false            // permanently ON, item 10c
        if (key < 0x01000000)
            return key >= 0x20
        return (key in root.keyNames) || (key >= Qt.Key_F1 && key <= Qt.Key_F35)
    }

    function isBindable(key, modifiers, text) {
        return root.isBindableId(root.idFor(key, modifiers, text))
    }

    // --- the default table ---------------------------------------------------
    // The alpha legends are the ones printed on the real keys: A-F are the menu
    // keys, G-L the MTH row, M-R the ' row, S-X the SIN row, and Y and Z sit on
    // +/- and EEX. That is what the face shows, so it is what the keyboard does.
    readonly property var defaults: ({
        "c:0": "N0", "c:1": "N1", "c:2": "N2", "c:3": "N3", "c:4": "N4",
        "c:5": "N5", "c:6": "N6", "c:7": "N7", "c:8": "N8", "c:9": "N9",
        "c:.": "PERIOD", "c:,": "PERIOD",
        "c:+": "PLUS", "c:-": "MINUS", "c:*": "MUL", "c:/": "DIV",
        "c:\u00d7": "MUL", "c:\u00f7": "DIV",      // × and ÷, third level on some layouts
        "c:'": "QUOTE", "c: ": "SPC",

        "c:A": "A", "c:B": "B", "c:C": "C", "c:D": "D", "c:E": "E", "c:F": "F",
        "c:G": "MTH", "c:H": "PRG",  "c:I": "CST",
        "c:J": "VAR", "c:K": "UP",   "c:L": "NXT",
        "c:M": "QUOTE", "c:N": "STO", "c:O": "EVAL",
        "c:P": "LEFT",  "c:Q": "DOWN", "c:R": "RIGHT",
        "c:S": "SIN", "c:T": "COS",   "c:U": "TAN",
        "c:V": "SQRT", "c:W": "POWER", "c:X": "INV",
        "c:Y": "NEG", "c:Z": "EEX",

        ["k:" + Qt.Key_Return + ":0"]: "ENTER",
        ["k:" + Qt.Key_Enter + ":0"]: "ENTER",
        ["k:" + Qt.Key_Backspace + ":0"]: "BS",
        ["k:" + Qt.Key_Delete + ":0"]: "DEL",
        ["k:" + Qt.Key_Left + ":0"]: "LEFT",
        ["k:" + Qt.Key_Right + ":0"]: "RIGHT",
        ["k:" + Qt.Key_Up + ":0"]: "UP",
        ["k:" + Qt.Key_Down + ":0"]: "DOWN",
        ["k:" + Qt.Key_PageUp + ":0"]: "NXT",
        ["k:" + Qt.Key_Tab + ":0"]: "NXT",

        // The two shifts and alpha, as ordinary keys - see normalise().
        ["k:" + Qt.Key_Shift + ":0"]: "SHL",
        ["k:" + Qt.Key_Control + ":0"]: "SHR",
        ["k:" + Qt.Key_Alt + ":0"]: "ALPHA",
        ["k:" + Qt.Key_F7 + ":0"]: "SHL",
        ["k:" + Qt.Key_F8 + ":0"]: "SHR",
        ["k:" + Qt.Key_F9 + ":0"]: "ALPHA",

        // Menu keys also on the function row.
        ["k:" + Qt.Key_F1 + ":0"]: "A", ["k:" + Qt.Key_F2 + ":0"]: "B",
        ["k:" + Qt.Key_F3 + ":0"]: "C", ["k:" + Qt.Key_F4 + ":0"]: "D",
        ["k:" + Qt.Key_F5 + ":0"]: "E", ["k:" + Qt.Key_F6 + ":0"]: "F"
    })
}
