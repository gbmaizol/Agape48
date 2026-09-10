import QtQuick
import QtQuick.Controls

// A switch drawn to sit in a line of dialog text, and carrying its own label.
//
// Gert, 2026sep10: "making the toggles and the space between them smaller, more
// like the size and space of normal text". The Basic style's Switch is drawn
// for a thumb on a phone - about 40 px of indicator per row - and four of them
// in a column was most of what fitted in his settings window, which is half of
// why the speed controls below them were off the bottom and cost him an evening
// of measurements taken at a rate he could not see. See deferred items 1 and 2
// in docs/design-questions.md.
//
// THE LABEL IS PART OF THE CONTROL rather than a Label beside it, which every
// call site used to need for a reason worth keeping: the Basic style paints
// Switch.text in a dark ink meant for a light window, and on this background it
// was almost unreadable. Overriding contentItem fixes that once instead of four
// times, and it also means the words are clickable, which they were not before.
//
// Sized from TextSizes.dialogBody, so it tracks the text it sits beside when he
// moves that slider rather than needing its own number.
Switch {
    id: control

    padding: 0
    spacing: Math.round(TextSizes.dialogBody * 0.5)

    // Height of a line of body text, width a little under twice that. Rounded
    // to whole pixels: a radius on a fractional height leaves a soft edge.
    readonly property int trackHeight: Math.round(TextSizes.dialogBody * 1.05)
    readonly property int trackWidth:  Math.round(TextSizes.dialogBody * 1.85)

    implicitHeight: Math.max(trackHeight, contentItem.implicitHeight)

    indicator: Rectangle {
        implicitWidth:  control.trackWidth
        implicitHeight: control.trackHeight
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: height / 2
        color: control.checked ? "#3f6f3f" : "#343434"
        border.width: 1
        border.color: control.checked ? "#5fa85f" : "#5a5a5a"

        Rectangle {
            readonly property int inset: 2
            x: control.checked ? parent.width - width - inset : inset
            y: inset
            width: parent.height - 2 * inset
            height: width
            radius: width / 2
            color: control.enabled ? "#e8e8e8" : "#8a8a8a"
            Behavior on x { NumberAnimation { duration: 90 } }
        }
    }

    contentItem: Text {
        text: control.text
        color: control.enabled ? "#e8e8e8" : "#8a8a8a"
        font.pixelSize: TextSizes.dialogBody
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
