import QtQuick
import Agape48

// Settings, built from raw QtQuick primitives.
//
// Qt Quick Controls is not a dependency, on purpose: it would add roughly a
// megabyte of styles to a statically linked binary and its widgets look wrong
// over a photorealistic calculator face anyway. The cost is that a text field
// is a TextInput in a Rectangle, which is what this file is.
Item {
    id: root
    required property Agape48Engine engine
    property bool opened: false

    function open()  { opened = true }
    function close() { opened = false }

    visible: opacity > 0
    opacity: opened ? 1 : 0
    Behavior on opacity { NumberAnimation { duration: 150 } }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 0.6
        MouseArea { anchors.fill: parent; onClicked: root.close() }
    }

    Rectangle {
        anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
        anchors.margins: 12
        height: column.implicitHeight + 32
        radius: 14
        color: "#1b1b1b"

        Column {
            id: column
            anchors { fill: parent; margins: 16 }
            spacing: 14

            Text {
                text: qsTr("Agape48")
                color: "#f0f0f0"; font.pixelSize: 20; font.weight: Font.DemiBold
            }

            // --- ROM ------------------------------------------------------
            Text { text: qsTr("HP 48 ROM"); color: "#9a9a9a"; font.pixelSize: 12 }
            PathField {
                width: parent.width
                text: root.engine.romSource.toString()
                placeholder: qsTr("path to an HP 48 ROM image")
                onAccepted: (value) => { root.engine.romSource = value; root.engine.start() }
            }

            // --- state location -------------------------------------------
            Text {
                text: qsTr("State folder — put this inside your synced folder to "
                           + "carry the calculator between machines")
                color: "#9a9a9a"; font.pixelSize: 12; width: parent.width
                wrapMode: Text.WordWrap
            }
            PathField {
                width: parent.width
                text: root.engine.state.displayName
                placeholder: qsTr("e.g. ~/Dropbox/Agape48")
                onAccepted: (value) => root.engine.state.migrateTo(value)
            }
            Row {
                spacing: 10
                TextButton {
                    text: qsTr("Pick folder…")
                    onClicked: root.engine.state.requestLocation()
                }
                TextButton {
                    text: qsTr("Use app storage")
                    onClicked: root.engine.state.useDefaultLocation()
                }
            }

            // --- toggles --------------------------------------------------
            Row {
                spacing: 20
                Toggle {
                    label: qsTr("Haptics")
                    checked: root.engine.hapticsEnabled
                    onToggled: root.engine.hapticsEnabled = !root.engine.hapticsEnabled
                }
                Toggle {
                    label: qsTr("Beep")
                    checked: root.engine.soundEnabled
                    onToggled: root.engine.soundEnabled = !root.engine.soundEnabled
                }
            }

            Row {
                spacing: 10
                TextButton { text: qsTr("Copy stack");  onClicked: root.engine.copyStackToClipboard() }
                TextButton { text: qsTr("Paste");       onClicked: root.engine.pasteClipboardToStack() }
                TextButton { text: qsTr("Save now");    onClicked: root.engine.saveState() }
                TextButton { text: qsTr("Close");       onClicked: root.close() }
            }
        }
    }

    // A folder dropped on the window is the cheapest desktop folder picker
    // there is, and it needs no extra module at all.
    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls && drop.urls.length > 0)
                root.engine.state.migrateTo(drop.urls[0])
        }
    }

    // TODO: PathField.qml, TextButton.qml and Toggle.qml - three ~30-line files
    // of Rectangle + Text + TextInput + MouseArea. Kept out of this skeleton so
    // the interesting files stay readable.
}
