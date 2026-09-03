import QtQuick
import QtQuick.Controls
import Agape48

// One calculator key's keyboard bindings. Design item 10.
//
// Opened by Ctrl+clicking a key on the face, or by a plain click while the
// ⋮ menu's "Customize keyboard…" mode is on. Gert changed the gesture from
// Ctrl+right-click to Ctrl+click on 2026aug30, at the same time as making the
// ⋮ icon the only way to the menu.
//
// Edits apply as they are made and the list is the truth, so there is no Save
// to be out of step with it. The original sketch had one, but its job was to be
// greyed out during a conflict, and item 10a replaced blocking with stealing.
//
// Capture swallows every key except Esc, which is why Take it / Cancel stay
// mouse-reachable: Esc belongs to the dialog, so it can never be captured, and
// that is also the rule that keeps Esc permanently ON.
Window {
    id: root
    required property Agape48Engine engine

    property string keyId: ""
    property string keyLabel: ""
    property bool   capturing: false
    property string pendingId: ""
    property string pendingOwner: ""
    property string refused: ""
    property var    rows: []

    readonly property bool opened: visible

    function openFor(k) {
        keyId = k.key
        keyLabel = k.label && k.label !== k.key ? k.label : k.key
        capturing = false
        pendingId = ""
        refused = ""
        // Reopening used to leave a conflict row from last time on screen, and
        // its Take it then bound key 0 - a binding for a key that does not
        // exist. Found by driving the dialog twice in a row.
        pendingOwner = ""
        refresh()
        if (transientParent) {
            x = transientParent.x + (transientParent.width - width) / 2
            y = transientParent.y + 90
        }
        show(); raise(); requestActivate()
    }
    function refresh() { rows = Agape48Keymap.bindingsFor(keyId) }

    // The keymap speaks internal names - "N8", "SHL". The face speaks printed
    // labels - "8", the violet arrow. The user only ever saw the second, so the
    // conflict line has to translate rather than say "8 is currently N8".
    function labelOf(name) {
        const keys = root.engine.skin.keys
        for (let i = 0; i < keys.length; ++i)
            if (keys[i].key === name)
                return keys[i].label || name
        return name
    }

    function accept(key, mods, text) {
        // Fn on a ThinkPad arrives as Qt.Key_WakeUp and used to be recorded as
        // a binding the calculator could never see again - dogfood #8 line 8.
        const id = Agape48Keymap.idFor(key, mods, text)
        if (!Agape48Keymap.isBindableId(id)) {
            refused = (key === Qt.Key_Escape)
                      ? qsTr("Esc is always ON and cannot be given to another key.")
                      : qsTr("That key cannot be used. The system keeps it for itself.")
            return
        }
        refused = ""
        const owner = Agape48Keymap.ownerOfId(id)
        if (owner === root.keyId) {          // already ours, nothing to do
            capturing = false
            pendingId = ""
            return
        }
        pendingId = id
        if (owner === "") {                  // free: take it straight away
            Agape48Keymap.bindId(id, root.keyId)
            capturing = false
            pendingId = ""
            refresh()
            return
        }
        // Taken. Show who has it and offer the steal - item 10a chose stealing
        // over blocking, because blocking means three dialogs for one change.
        pendingOwner = owner
        capturing = false
    }

    function steal() {
        if (root.pendingId === "")
            return
        Agape48Keymap.bindId(root.pendingId, root.keyId)
        capturing = false
        pendingOwner = ""
        refresh()
    }

    title: qsTr("Keyboard for %1").arg(keyLabel)
    flags: Qt.Dialog
    color: "#1b1b1b"
    width: 440
    height: 360
    minimumWidth: 340
    minimumHeight: 260

    // Esc cancels a capture if one is running, otherwise closes the window.
    // A key handler rather than a Shortcut, for the reason in SettingsWindow.qml:
    // a Shortcut in a secondary window keeps grabbing after the window is gone.
    Item {
        anchors.fill: parent
        focus: !root.capturing
        Keys.onEscapePressed: (event) => {
            if (root.capturing) root.capturing = false
            else                root.close()
            event.accepted = true
        }

    Column {
        anchors { fill: parent; margins: 18 }
        spacing: 10

        Label {
            text: qsTr("Keys that press %1").arg(root.keyLabel)
            color: "#f0f0f0"; font.pixelSize: TextSizes.dialogTitle; font.weight: Font.DemiBold
        }

        // --- what is bound now ------------------------------------------------
        Column {
            width: parent.width
            spacing: 4
            Repeater {
                model: root.rows
                delegate: Row {
                    id: bindingRow
                    required property var modelData
                    spacing: 8
                    Button {
                        text: "−"
                        width: 30
                        enabled: !bindingRow.modelData.locked
                        onClicked: { Agape48Keymap.unbind(bindingRow.modelData.id); root.refresh() }
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: bindingRow.modelData.locked
                              ? qsTr("%1   (always ON, cannot be changed)").arg(bindingRow.modelData.label)
                              : bindingRow.modelData.label
                        color: bindingRow.modelData.locked ? "#7d7d7d" : "#e8e8e8"
                        font.pixelSize: TextSizes.dialogBody
                    }
                }
            }
            Label {
                visible: root.rows.length === 0
                text: qsTr("No key on the keyboard presses this one.")
                color: "#7d7d7d"; font.pixelSize: TextSizes.dialogHint
            }
        }

        // --- add one ----------------------------------------------------------
        Button {
            text: qsTr("+   Add a key")
            visible: !root.capturing && root.pendingOwner === ""
            onClicked: {
                root.capturing = true; root.pendingId = ""; root.refused = ""
                capture.forceActiveFocus()
            }
        }

        Rectangle {
            width: parent.width
            height: 62
            visible: root.capturing || root.pendingOwner !== ""
            color: "#262626"
            radius: 6
            border.color: (root.pendingOwner !== "" || root.refused !== "")
                          ? "#c86464" : "#3a3a3a"

            Column {
                anchors { fill: parent; margins: 10 }
                spacing: 6
                Label {
                    text: root.refused !== "" ? root.refused
                          : root.capturing
                          ? qsTr("Press the key you want. Esc cancels.")
                          : qsTr("%1 is currently assigned to the %2 key.")
                            .arg(Agape48Keymap.labelForId(root.pendingId))
                            .arg(root.labelOf(root.pendingOwner))
                    color: (root.pendingOwner !== "" || root.refused !== "")
                           ? "#e88a8a" : "#d0d0d0"
                    font.pixelSize: TextSizes.dialogBody
                    width: root.width - 56
                    wrapMode: Text.WordWrap
                }
                Row {
                    spacing: 8
                    visible: root.pendingOwner !== ""
                    Button { text: qsTr("Take it"); onClicked: root.steal() }
                    Button {
                        text: qsTr("Cancel")
                        onClicked: { root.pendingOwner = ""; root.pendingId = "" }
                    }
                }
            }
        }

        Item { width: 1; height: 4 }

        Row {
            spacing: 10
            Button {
                text: qsTr("Reset this key")
                onClicked: { Agape48Keymap.resetKey(root.keyId); root.refresh() }
            }
            Button {
                text: qsTr("Reset all keys")
                onClicked: { Agape48Keymap.resetToDefaults(); root.refresh() }
            }
            Button { text: qsTr("Close"); onClicked: root.close() }
        }
    }

    }

    // The capture sink. It has to swallow everything so that a captured chord
    // does not also drive the dialog's own buttons; Esc is handled by the
    // key handler above and never reaches here as a binding.
    Item {
        id: capture
        anchors.fill: parent
        enabled: root.capturing
        focus: root.capturing
        Keys.onPressed: (event) => {
            event.accepted = true
            if (event.key === Qt.Key_Escape) {
                root.capturing = false
                return
            }
            if (event.isAutoRepeat)
                return
            root.accept(event.key, event.modifiers, event.text)
        }
    }
}
