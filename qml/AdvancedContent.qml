import QtQuick
import QtQuick.Controls
import Agape48

// Text sizes, with no shell around them - see SettingsContent.qml for why the
// contents of every dialog now live apart from the thing that frames them.
//
// Gert, 2026sep03: "I'll find a size that works and make it default." So the
// point is not the sliders, it is the NUMBER beside each one - he drags until it
// looks right, reads the figure off, and that figure becomes a default in
// TextSizes.qml.
//
// On a desktop this is framed by a window BESIDE the settings window, because
// half of what it tunes is drawn on the calculator and Settings already covers
// it. On a phone there is nowhere beside anything, so it is a page over the
// settings page - which still previews most of what it changes, because the
// dialog sizes it tunes are what the page underneath is drawn in.
Item {
    id: root

    signal closeRequested()

    // The same dialog describes two shapes. On a desktop Settings is a window
    // and the menu is the ⋮ in the calculator's corner; on a phone Settings is
    // a page and the menu is the underlined "48GX" on the nameplate, because
    // the ⋮ was buried under the status bar and went. Naming the wrong one is
    // worse than saying nothing - it sends him hunting for a button that is not
    // there, which is what "it mentioned an outdated menu" was about.
    readonly property bool onPhone: Qt.platform.os === "android"
    readonly property string shellWord: onPhone ? qsTr("Page") : qsTr("Window")
    readonly property string shellHere: onPhone ? qsTr("page")  : qsTr("window")
    readonly property string menuWord:  onPhone ? "48GX" : "\u22ee"

    // One row: label, slider, and the figure he is going to tell us about.
    component SizeRow: Column {
        id: rowRoot
        required property string label
        required property string note
        required property int value
        required property int fallback
        signal moved(int v)

        width: parent ? parent.width : 0
        spacing: 2

        Row {
            width: parent.width
            spacing: 8
            Label {
                width: parent.width - readout.width - resetOne.width - 16
                text: rowRoot.label
                color: "#e8e8e8"
                font.pixelSize: TextSizes.dialogBody
                elide: Text.ElideRight
                anchors.verticalCenter: parent.verticalCenter
            }
            Label {
                id: readout
                width: 34
                // Monospaced so the row does not jiggle as the number changes
                // while he is dragging, which makes it much harder to read.
                font.family: "monospace"
                font.pixelSize: TextSizes.dialogBody
                horizontalAlignment: Text.AlignRight
                color: rowRoot.value === rowRoot.fallback ? "#7d7d7d" : "#8fc9ff"
                text: rowRoot.value
                anchors.verticalCenter: parent.verticalCenter
            }
            Button {
                id: resetOne
                width: 26
                text: "↺"
                enabled: rowRoot.value !== rowRoot.fallback
                onClicked: rowRoot.moved(rowRoot.fallback)
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Slider {
            width: parent.width
            from: TextSizes.minSize
            to: TextSizes.maxSize
            stepSize: 1
            snapMode: Slider.SnapAlways
            value: rowRoot.value
            // moved(), not valueChanged: valueChanged also fires when the value
            // is written back in, so the two would chase each other.
            onMoved: rowRoot.moved(Math.round(value))
        }
        Label {
            width: parent.width
            text: rowRoot.note
            color: "#7d7d7d"
            font.pixelSize: TextSizes.dialogHint
            wrapMode: Text.WordWrap
        }
    }

    Item {
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: (event) => { root.closeRequested(); event.accepted = true }

        Flickable {
            anchors { fill: parent; margins: 18; bottomMargin: 60 }
            contentWidth: width
            contentHeight: column.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            Column {
                id: column
                width: parent.width
                spacing: 14

                Label {
                    text: qsTr("Drag until it looks right, then tell me the number.")
                    color: "#f0f0f0"
                    font.pixelSize: TextSizes.dialogTitle
                    font.weight: Font.DemiBold
                }

                SizeRow {
                    label: qsTr("Messages over the calculator")
                    // Not "Save memory now": saving emits no notice, so that
                    // sentence sent him looking for a banner that never comes.
                    // These two do, and one of each colour.
                    note: qsTr("The red error strip and the blue notices. %1 → Import file to stack… shows a blue one; a folder that does not exist, typed into State folder, shows a red one.").arg(root.menuWord)
                    value: TextSizes.banner
                    fallback: TextSizes.defaultBanner
                    onMoved: (v) => TextSizes.banner = v
                }

                SizeRow {
                    label: qsTr("Message on the screen")
                    note: qsTr("\"The chosen memory is in use by another device.\", shown on a blank screen when another device has the calculator. It shrinks on its own if it will not fit the screen, so raising this past a certain point stops making a difference.")
                    value: TextSizes.screenMessage
                    fallback: TextSizes.defaultScreenMessage
                    onMoved: (v) => TextSizes.screenMessage = v
                }

                SizeRow {
                    label: qsTr("%1 headings").arg(root.shellWord)
                    note: qsTr("The bold line at the top of this %1, Settings, and the calculator shelf.").arg(root.shellHere)
                    value: TextSizes.dialogTitle
                    fallback: TextSizes.defaultDialogTitle
                    onMoved: (v) => TextSizes.dialogTitle = v
                }

                SizeRow {
                    label: qsTr("%1 text").arg(root.shellWord)
                    note: qsTr("Ordinary text in Settings, the shelf, and the in-use dialog.")
                    value: TextSizes.dialogBody
                    fallback: TextSizes.defaultDialogBody
                    onMoved: (v) => TextSizes.dialogBody = v
                }

                SizeRow {
                    label: qsTr("Explanatory lines")
                    note: qsTr("The grey sentences under a control, like this one.")
                    value: TextSizes.dialogHint
                    fallback: TextSizes.defaultDialogHint
                    onMoved: (v) => TextSizes.dialogHint = v
                }

                Rectangle { width: parent.width; height: 1; color: "#333" }

                // --- the ones a slider cannot reach ------------------------------
                //
                // Not hidden, because he asked for "calculator buttons text, text
                // over buttons" by name and silence would read as an oversight.
                Label {
                    width: parent.width
                    text: qsTr("The keys themselves")
                    color: "#f0f0f0"
                    font.pixelSize: TextSizes.dialogTitle
                    font.weight: Font.DemiBold
                }
                Label {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: "#e8a55a"
                    font.pixelSize: TextSizes.dialogHint
                    text: qsTr("The key labels and the coloured legends above them are not text while the program is running — they are drawn into the calculator's picture when the program is built, which is why they stay sharp at any size. Nothing here can move them. Tell me a number and I will rebuild the picture and send you a new copy to look at.")
                }
                Grid {
                    width: parent.width
                    columns: 2
                    columnSpacing: 12
                    rowSpacing: 3
                    Repeater {
                        model: [
                            { n: qsTr("Key label, 1–2 characters"), v: "28" },
                            { n: qsTr("Key label, longer"),         v: "25" },
                            { n: qsTr("Legend above a key"),        v: "18" },
                            { n: qsTr("Alpha letter, right of a key"), v: "17" },
                            { n: qsTr("CANCEL, under ON"),          v: "13" },
                            { n: qsTr("\"HEWLETT-PACKARD\""),       v: "17" },
                            { n: qsTr("\"48GX\""),                  v: "22" }
                        ]
                        delegate: Row {
                            required property var modelData
                            spacing: 8
                            Label {
                                text: modelData.n
                                color: "#9a9a9a"
                                font.pixelSize: TextSizes.dialogHint
                            }
                            Label {
                                text: modelData.v
                                color: "#7d7d7d"
                                font.family: "monospace"
                                font.pixelSize: TextSizes.dialogHint
                            }
                        }
                    }
                }
            }
        }

        Row {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 18 }
            spacing: 10
            Button {
                text: qsTr("Put everything back")
                enabled: TextSizes.changed
                onClicked: TextSizes.reset()
            }
            Item { width: parent.width - 260; height: 1 }
            Button { text: qsTr("Close"); onClicked: root.closeRequested() }
        }
    }
}
