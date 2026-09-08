import QtQuick
import QtQuick.Controls
import Agape48

// A full-screen page, for the platform that has only one window.
//
// Android does not have a second window: asking Qt for one makes the platform
// plugin acquire a native surface, and that call takes a process-wide lock the
// accessibility bridge holds, whereupon Qt aborts rather than deadlock.
// Measured on Gert's phone at 67b2f08: Settings 0 opened out of 10, "Open
// another calculator" 0 out of 3. Measured again at bc57de7, with the settings
// contents in a page like this one: Settings 10 out of 10, and the picker -
// still a Window, and therefore the control - still 0 out of 5.
//
// Gert named both the cause and the shape: "What could be crashing is we trying
// to use some desktop feature that doesn't exist in Android", and "What if we
// use android-settings style pages, instead of drawing windows-style? Maybe
// they are usually like this for a reason."
//
// A Popup is drawn inside the scene that already exists, so it never asks for a
// surface. IN-SCENE IS NOT A SHEET: this fills what it is given and paints an
// opaque background, so it is a page - not the bottom sheet the project left
// behind on 2026aug29, and the calculator does not show through it.
//
// Pages stack. One opened from another sits on top of it and the back arrow
// takes the top one off, which is what a phone's settings do and what Gert
// asked for: "the advanced wouldn't be a new window. It would be a new page
// inside settings, right?"
Popup {
    id: root

    // What the header says. The back arrow is not optional: a page with no way
    // back is the trap Main.qml warns about in the customize-mode banner.
    property string title: ""
    signal backRequested()

    // THE SYSTEM BACK IS NOT THE DRAWN ARROW, since dogfood android-08. The
    // arrow means "up one page" - it is what takes Text sizes back to Settings.
    // Gert wants the phone's own back to mean something stronger: "The back
    // button should take out of every internal configs or selections, stopping
    // at the calculator", and "make it go back to the calculator if swiping
    // back or clicking the back bottom-button." So the gesture and the
    // navigation bar emit this instead, and Main.qml - which is the only place
    // that knows how many pages are open - closes the lot.
    signal dismissRequested()

    // Everything declared inside a PageShell lands under the header.
    default property alias pageContent: body.data

    // Pinned rather than left to the default. A plain Popup already resolves to
    // Item - measured on Linux with qt.quick.controls.popup logging: 42 popup
    // lines and not one popup window, on a platform that would have made one if
    // that were the default - but the whole fix rests on this property, so it is
    // written down instead of inherited. Qt 6.8 added it, which is older than
    // the SafeArea the calculator already needs, so it costs no version.
    popupType: Popup.Item

    modal: true
    // Nothing to shade: it covers everything it is given, and a dim layer would
    // only cost a full-screen blend every frame.
    dim: false
    padding: 0
    // Nothing closes this page by itself: every exit goes through
    // backRequested() or dismissRequested(), so a page that has something to
    // ask before closing still gets to ask it.
    closePolicy: Popup.NoAutoClose
    focus: true

    background: Rectangle { color: "#1b1b1b" }

    Item {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 52

        Item {
            id: backButton
            width: 52; height: 52

            // Drawn, not a glyph. "←" is a hairline in most of the fonts a phone
            // actually has, which is the same reason the house in
            // SettingsContent is a Canvas rather than "⌂".
            Canvas {
                anchors.centerIn: parent
                width: 24; height: 24
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    ctx.strokeStyle = "#e8e8e8"
                    ctx.lineWidth = 2
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(15, 4); ctx.lineTo(7, 12); ctx.lineTo(15, 20)
                    ctx.stroke()
                }
            }
            TapHandler { onTapped: root.backRequested() }
        }

        Text {
            anchors { left: backButton.right; verticalCenter: parent.verticalCenter }
            text: root.title
            color: "#e8e8e8"
            font.pixelSize: TextSizes.dialogTitle
        }

        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1
            color: "#3a3a3a"
        }
    }

    // Android's back gesture arrives as a key press, and the manifest goes out
    // of its way to keep it that way: android:enableOnBackInvokedCallback is
    // false there on purpose, because opting in to Android 13's replacement
    // stops the key ever reaching this handler - see the manifest. Escape is handled inside each page's contents, where
    // it cannot leak out to the calculator - see SettingsContent.qml.
    //
    // ON THE ITEM THAT CONTAINS THE PAGE, and that is the whole fix. It was on
    // a Popup first, where `Keys` cannot attach at all ("Could not attach Keys
    // property to: PageShell ... is not an Item"), so it was dead code. Then it
    // was on a focus-holding Item drawn under the page - which worked exactly
    // until you touched anything. A key event goes to the focus item and then
    // up its PARENT chain; a catcher beside the content is a sibling, not an
    // ancestor, so the moment a text field or a button inside the page took the
    // focus the back key went past it and nothing happened. That is Gert's
    // dogfood android-08 line 8 precisely: "In some circumstances, like if I
    // just entered the settings, swiping back goes back to the calculator" -
    // the circumstance being that he had not touched the page yet.
    //
    // Everything declared inside a PageShell lands in here, so here the back
    // key is always upstream of whatever has the focus.
    Item {
        id: body
        anchors { fill: parent; topMargin: header.height }
        focus: true
        Keys.onPressed: (event) => {
            if (event.key === Qt.Key_Back) {
                root.dismissRequested()
                event.accepted = true
            }
        }
    }
}
