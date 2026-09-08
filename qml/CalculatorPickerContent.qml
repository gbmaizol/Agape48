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

    // Deleting a calculator throws away its memory, its ports and its ROM
    // settings, and there is no undo anywhere in this program. So it asks -
    // which is also what makes the button's position on the row harmless.
    Dialog {
        id: confirmDelete
        anchors.centerIn: parent
        modal: true
        // PINNED, like every other popup in this program. See PageShell.qml:
        // asking Android for a second window makes the platform plugin acquire
        // a surface and the process aborts. A Dialog is a Popup, so it is the
        // same question and it gets the same answer.
        popupType: Popup.Item
        title: qsTr("Delete this calculator?")
        standardButtons: Dialog.Yes | Dialog.Cancel
        // Explicit, for the binding loop the settings dialog documents at
        // length: a Dialog with standardButtons and a wrapping label sizes
        // itself from a footer whose width depends on the width it is given.
        width: Math.min(420, root.width - 40)

        // WRAPPED IN A COLUMN, exactly like the settings dialog, and not for
        // tidiness. A single Item declared in a Dialog BECOMES its contentItem,
        // so a wrapping Label put here directly has its height decide the
        // dialog's height while its width comes from the dialog - and Qt
        // reports "Binding loop detected for property implicitHeight" twice at
        // startup. A Column in between is the contentItem instead; its height
        // is the sum of its children and nothing measures back the other way.
        Column {
            width: parent.width
            // NO COLOUR OF ITS OWN. A Dialog paints its own background from
            // the system palette - light on a laptop with no theme set, dark on
            // Gert's Windows - so ink fixed at either end is unreadable on the
            // other. Measured: on a bare X session this label was #e8e8e8 on the
            // Basic style's white and could not be read at all. The page behind
            // it is a different case and keeps its light ink, because that
            // background is ours and is always dark.
            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                font.pixelSize: TextSizes.dialogBody
                text: qsTr("%1 and everything in it - its memory, both ports and "
                           + "its settings - will be removed from the state folder. "
                           + "This cannot be undone.").arg(root.selected)
            }
        }

        onAccepted: {
            const doomed = root.selected
            if (root.engine.state.deleteInstance(doomed))
                root.selected = root.engine.state.instance
            // Either way: on success the list is one shorter, and on failure
            // lastError says why and the row is still there to try again.
            root.refresh()
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

        // ONE ROW OF FOUR, EQUAL WIDTHS. It was a Flow first, which wrapped
        // Close onto a second line on a narrow shelf; Gert, 2026sep08: "Keep
        // the four buttons on the same row. they can be smaller." So they
        // divide the width between them instead of asking for what their text
        // wants - four equal quarters, which also stops the row from
        // rearranging itself as the selection changes the labels' state.
        //
        // "New" rather than "New calculator" for the same reason: a quarter of
        // a phone's width does not hold two words, and a Button clips its label
        // rather than eliding it. On a page headed "Calculators" the noun is
        // not needed.
        Row {
            id: buttonRow
            width: parent.width
            spacing: 6
            readonly property real cell: (width - spacing * 3) / 4
            Button {
                width: buttonRow.cell
                text: qsTr("Open")
                enabled: root.selected !== "" && root.selected !== root.engine.state.instance
                onClicked: root.openSelected()
            }
            Button {
                width: buttonRow.cell
                text: qsTr("New")
                onClicked: {
                    const made = root.engine.newCalculator()
                    if (made !== "") { root.selected = made; root.closeRequested() }
                }
            }
            // Gert, dogfood android-08 line 6: "we need a 4th button to delete
            // a calculator! It should be disabled if the selected calculator is
            // the one that's loaded." Both halves of that are here: the enabled
            // condition is his, and deleteInstance() refuses the open one again
            // on its own account.
            //
            // KEPT BEFORE Close rather than after it. Close has been the last
            // button on this row since the shelf existed, and moving it to make
            // room for a destructive one would put Delete under the thumb that
            // has learned where Close is.
            Button {
                id: deleteButton
                width: buttonRow.cell
                text: qsTr("Delete")
                enabled: root.selected !== "" && root.selected !== root.engine.state.instance
                // Red, and only when it can actually do something - a disabled
                // button painted in warning colours reads as an error message.
                //
                // TWO LITERALS, and not palette.mid for the disabled half:
                // reading one member of the palette group inside a binding that
                // assigns another member is a loop, and Qt says so twice at
                // startup - "QML Palette: Binding loop detected for property
                // buttonText". Caught by running the program before shipping
                // it, which is the only way this class of mistake shows up.
                palette.buttonText: enabled ? "#ff8a80" : "#7d7d7d"
                onClicked: confirmDelete.open()
            }
            Button { width: buttonRow.cell; text: qsTr("Close"); onClicked: root.closeRequested() }
        }
    }

    }
}
