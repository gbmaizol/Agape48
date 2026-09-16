import QtQuick
import QtQuick.Controls
import Agape48

// Text sizes, with no shell around them - see SettingsContent.qml for why the
// contents of every dialog now live apart from the thing that frames them.
//
// 2026sep03, and it decides the shape of this window: the point is not the
// sliders, it is the NUMBER beside each one. A slider is dragged until the text
// looks right, the figure is read off, and that figure becomes a default in
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
    // worse than saying nothing - it sends the reader hunting for a button that
    // is not there, which is what "it mentioned an outdated menu" was about.
    // The calculator, for the speed calibration below. Not required: the
    // text-size half of this page works without one, and a page that refuses to
    // open because no calculator is loaded would be a poor trade.
    property var engine: null

    readonly property bool onPhone: Qt.platform.os === "android"
    readonly property string shellWord: onPhone ? qsTr("Page") : qsTr("Window")
    readonly property string shellHere: onPhone ? qsTr("page")  : qsTr("window")
    // "48GX" on both since 2026sep10: the three-dot button is gone from the
    // desktop too, so naming it in the help text would point at something that
    // is no longer drawn.
    readonly property string menuWord:  "48GX"

    // One row: label, slider, and the figure that becomes the new default.
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
                // during a drag, which makes it much harder to read.
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
            // Sur la telefono svingo komencita sur regilo ĉe la rando de la paĝo
            // alie atingas la regilon; vidu la saman linion en SettingsContent.qml.
            boundsBehavior: Qt.platform.os === "android" ? Flickable.DragOverBounds
                                                          : Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            Column {
                id: column
                // A GUTTER FOR THE SCROLLBAR. The bar is an overlay anchored to the
                // Flickable's right edge, and this content was parent.width, so every
                // full-width row - every wrapped paragraph and the two-column value
                // grid - ran underneath it with no clearance at all. 2026sep09:
                // every object in every dialog gets at least a little clearance
                // around it. Fourteen is the Basic style's bar plus air;
                // when there is nothing to scroll the bar is hidden and this is just a
                // slightly narrower column, which is invisible.
                width: parent.width - 14
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
                    // sentence pointed at a banner that never comes.
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
                // Not hidden, because "calculator buttons text, text over
                // buttons" was asked for by name and silence would read as an
                // oversight.
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

                // ------------------------------------------------------------
                // THE SPEED CALIBRATION, moved here on 2026sep10 from the
                // settings window, where it was still invisible.
                //
                // It was invisible in Settings because it sat below the fold of
                // a window the face size makes about 356x599, and an evening of
                // speed measurements went to a rate nobody could see. It also
                // belongs here on its own merits: a one-time per-machine
                // calibration, like the text sizes it now sits under.
                Rectangle {
                    width: parent.width
                    height: 1
                    color: "#333333"
                    visible: root.engine !== null
                }

                Label {
                    visible: root.engine !== null
                    text: qsTr("Real calculator speed")
                    color: "#f0f0f0"
                    font.pixelSize: TextSizes.dialogTitle
                    font.weight: Font.DemiBold
                }

                Label {
                    visible: root.engine !== null
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: "#9a9a9a"
                    font.pixelSize: TextSizes.dialogHint
                    text: qsTr("Turn \"Slow down to real calculator speed\" on in "
                             + "Settings first. The figure is instructions a second "
                             + "and belongs to this machine, not to the calculator, "
                             + "so it is not synced.")
                }

                // A SLIDER, AND 1.0 IS EXACTLY IN THE MIDDLE, from 2026sep10:
                // "Default is 1.0 at the middle, and the user and set it all the
                // way to the left at sluggish 0.1 to all the way to the right at
                // almost unregulated speed."
                //
                // TWO LOG HALVES rather than one scale, because those three
                // numbers cannot all sit on a single logarithmic axis: 0.1 on the
                // left and 1.0 at the centre would put 10 on the right, and the
                // right end is the free-running ceiling, which on this machine is
                // about 21x. So the left half runs 0.1 -> 1.0 and the right half
                // 1.0 -> speedFactorMax, each logarithmic in itself. Both halves
                // are proportional under the finger, which is what matters when
                // what is being judged is a ratio, and the middle is a real
                // detent rather than a number that happens to be near it.
                //
                // The slider does NOT bind to speedFactor. A control whose value
                // is bound to what it writes loses the binding on the first drag
                // and then silently stops tracking - the mistake TextSizes.qml
                // documents at the top of its aliases. It is seeded once and the
                // reset button moves both.
                function sliderToFactor(v) {
                    const max = root.engine ? root.engine.speedFactorMax : 10
                    return v < 0.5 ? 0.1 * Math.pow(10, 2 * v)
                                   : Math.pow(max, (v - 0.5) * 2)
                }
                function factorToSlider(f) {
                    const max = root.engine ? root.engine.speedFactorMax : 10
                    if (!(f > 0)) return 0.5
                    return f <= 1 ? 0.5 * (Math.log(f) / Math.LN10 + 1)
                                  : 0.5 + 0.5 * Math.log(f) / Math.log(max)
                }

                // ON TOP OF THE SLIDER, one decimal, and deliberately not large:
                // 2026sep10: the number shows as "X.X" on top of the slider and
                // stays small. Body size rather than the
                // title size the headings above use.
                Label {
                    visible: root.engine !== null
                    text: root.engine ? root.engine.speedFactor.toFixed(1) + "\u00d7" : ""
                    color: "#f0f0f0"
                    font.family: "monospace"
                    font.pixelSize: TextSizes.dialogBody
                }

                Slider {
                    id: speedSlider
                    width: parent.width
                    from: 0
                    to: 1
                    enabled: root.engine !== null
                    Component.onCompleted:
                        value = column.factorToSlider(root.engine ? root.engine.speedFactor : 1)
                    onMoved: root.engine.speedFactor = column.sliderToFactor(value)

                    // Seeded once and written on move, never bound - a control
                    // whose value is bound to what it writes loses the binding
                    // on the first drag. But the factor can now change from
                    // somewhere else entirely, because switching folders reads
                    // another calculator's, so the handle is moved by hand when
                    // that happens and the user is not holding it.
                    Connections {
                        target: root.engine
                        function onSpeedFactorChanged() {
                            if (!speedSlider.pressed)
                                speedSlider.value =
                                    column.factorToSlider(root.engine.speedFactor)
                        }
                    }
                }

                Row {
                    visible: root.engine !== null
                    width: parent.width
                    spacing: 8
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        // The effective figure, and beside it x48's own reading,
                        // taken against the host clock - the answer to "Don't you
                        // see the clock tiks?" They differ: on 2026sep10 the core
                        // read 215766 against a setting of 206660, the pacer
                        // overshooting by about 4%, and that gap is exactly why
                        // the measured number is worth showing next to the asked
                        // for one rather than hidden.
                        text: {
                            if (!root.engine) return ""
                            return root.engine.measuredRate > 0
                                   ? qsTr("%1/s, doing %2").arg(root.engine.effectiveRate)
                                                           .arg(root.engine.measuredRate)
                                   : qsTr("%1/s").arg(root.engine.effectiveRate)
                        }
                        color: root.engine && root.engine.realSpeed ? "#9a9a9a" : "#6a6a6a"
                        font.family: "monospace"
                        font.pixelSize: TextSizes.dialogHint
                    }
                }

                Row {
                    visible: root.engine !== null
                    spacing: 8
                    Button {
                        text: qsTr("Back to a real 48")
                        enabled: root.engine && Math.abs(root.engine.speedFactor - 1) > 0.001
                        onClicked: {
                            root.engine.speedFactor = 1.0
                            speedSlider.value = 0.5
                        }
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("left: a tenth of a real 48   \u00b7   "
                                 + "middle: authentic   \u00b7   right: unthrottled")
                        color: "#7d7d7d"
                        font.pixelSize: TextSizes.dialogHint
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
