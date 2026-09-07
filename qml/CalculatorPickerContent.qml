import QtQuick
import QtQuick.Controls
import Agape48

// The calculators inside the state folder, and which one you are using.
//
// The state folder is a shelf: one shared ROM and a subfolder per calculator.
// Opening Agape48 gives you the calculator you used last; if that one is
// already open somewhere else, this appears instead of a dead calculator,
// which is the whole difference between the design Gert took and the one he
// rejected. Nothing is chosen for you by a timestamp and nothing is cloned.
//
// Contents only. What frames them is CalculatorPickerWindow on a desktop and
// CalculatorPickerPage on a phone - see SettingsContent.qml, which was split
// the same way and for the same reason.
Item {
    id: root
    required property Agape48Engine engine

    property var    rows: []
    property string selected: ""

    signal closeRequested()
    // Picked one that is in use. Main.qml decides what to do about it.
    signal busyCalculator(string name, var holder)

    function refresh() {
        rows = engine.state.instances()
        if (selected === "" || !rows.some(r => r.name === selected))
            selected = engine.state.instance
        nameField.text = selected
    }

    function openSelected() {
        if (root.selected === "" || root.selected === root.engine.state.instance)
            return
        if (root.engine.openCalculator(root.selected)) {
            root.closeRequested()
            return
        }
        // Somebody has it. The shelf can only report that; the handover dialog
        // can do something about it - it offers to ask them for it, which is
        // the whole point of asking rather than taking.
        const name = root.selected
        if (root.engine.state.isHeldBySomebody(name)) {
            root.closeRequested()
            root.busyCalculator(name, root.engine.state.lockHolderOf(name))
        } else {
            root.refresh()      // something else went wrong; the list says what
        }
    }

    // A key handler rather than a Shortcut, for the reason in SettingsWindow:
    // a Shortcut in a secondary window goes on grabbing its sequence after the
    // window is hidden, which is what killed Esc-is-ON in dogfood #8.
    Item {
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: (event) => { root.closeRequested(); event.accepted = true }

    Column {
        anchors { fill: parent; margins: 18 }
        spacing: 10

        Label {
            id: titleLabel
            text: qsTr("Calculators in this state folder")
            color: "#f0f0f0"; font.pixelSize: TextSizes.dialogTitle; font.weight: Font.DemiBold
        }
        Label {
            id: pathLabel
            width: parent.width
            text: root.engine.state.displayName
            color: "#7d7d7d"; font.pixelSize: TextSizes.dialogHint
            elide: Text.ElideMiddle
        }

        Rectangle {
            width: parent.width
            // The list took a fixed 176 units, which is right in a window sized
            // to hold it and wrong on a page that is as tall as the phone. It
            // now takes what is left after the rows below it, so the list is
            // the part that grows - measured on the phone, where a fixed height
            // left two thirds of the screen empty under it.
            height: Math.max(120, parent.height - parent.spacing * 4
                                  - titleLabel.height - pathLabel.height
                                  - nameRow.height - buttonRow.height - gap.height)
            color: "#141414"
            border.color: "#3a3a3a"
            radius: 4

            ListView {
                id: list
                anchors { fill: parent; margins: 4 }
                clip: true
                model: root.rows
                delegate: Rectangle {
                    required property var modelData
                    width: list.width
                    height: 40
                    color: modelData.name === root.selected ? "#2d4b6b" : "transparent"
                    radius: 3
                    MouseArea {
                        anchors.fill: parent
                        onClicked: { root.selected = modelData.name; nameField.text = modelData.name }
                        onDoubleClicked: root.openSelected()
                    }
                    Column {
                        anchors { left: parent.left; verticalCenter: parent.verticalCenter; leftMargin: 8 }
                        spacing: 1
                        Label {
                            text: modelData.current ? qsTr("%1   — the one you are using").arg(modelData.name)
                                                    : modelData.name
                            color: "#e8e8e8"; font.pixelSize: TextSizes.dialogBody
                        }
                        Label {
                            // "in use" is a fact on this device, where the pid
                            // can be checked, and a guess anywhere else - so
                            // another device's lock says who and since when
                            // rather than pretending to know.
                            text: modelData.inUse
                                  ? qsTr("open somewhere else")
                                  : (modelData.heldBy !== "" && !modelData.current
                                     ? qsTr("left open on %1 since %2").arg(modelData.heldBy).arg(modelData.heldSince)
                                     : qsTr("last used %1").arg(Qt.formatDateTime(modelData.lastUsed, "yyyy-MM-dd HH:mm")))
                            color: modelData.inUse ? "#e8a55a" : "#7d7d7d"
                            font.pixelSize: TextSizes.dialogHint
                        }
                    }
                }
            }
        }

        Row {
            id: nameRow
            spacing: 8
            width: parent.width
            TextField {
                id: nameField
                width: parent.width - renameButton.width - parent.spacing
                placeholderText: qsTr("name for this calculator")
                onAccepted: renameButton.clicked()
            }
            Button {
                id: renameButton
                text: qsTr("Rename")
                onClicked: {
                    // The engine, not state.renameInstance(): renaming the one
                    // that is open has to put the C core down first, or it goes
                    // on saving into the old name and the folder comes back.
                    if (root.engine.renameCalculator(root.selected, nameField.text)) {
                        root.selected = nameField.text.trim()
                        root.refresh()
                    }
                }
            }
        }

        Item { id: gap; width: 1; height: 2 }

        Row {
            id: buttonRow
            spacing: 10
            Button {
                text: qsTr("Open")
                enabled: root.selected !== "" && root.selected !== root.engine.state.instance
                onClicked: root.openSelected()
            }
            Button {
                text: qsTr("New calculator")
                onClicked: {
                    const made = root.engine.newCalculator()
                    if (made !== "") { root.selected = made; root.closeRequested() }
                }
            }
            Button { text: qsTr("Close"); onClicked: root.closeRequested() }
        }
    }

    }
}
