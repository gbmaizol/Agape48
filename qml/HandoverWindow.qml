import QtQuick
import QtQuick.Controls
import Agape48

// "This calculator is somebody else's just now."
//
// Shown when ON could not take the calculator back. Nothing here decides for
// the user: a lock from another machine cannot be checked - a machine that is
// merely offline looks exactly like one that has died - so the age is reported
// and the choice is theirs. Cancel leaves the calculator asleep, which is where
// it already was.
Window {
    id: root
    required property Agape48Engine engine

    property var holder: ({})

    readonly property bool opened: visible

    function showFor(h) {
        holder = h || ({})
        if (transientParent) {
            x = transientParent.x + (transientParent.width - width) / 2
            y = transientParent.y + 90
        }
        show(); raise(); requestActivate()
    }

    readonly property int quiet: holder.quietMinutes !== undefined ? holder.quietMinutes : -1
    readonly property bool probablyGone: holder.probablyGone === true

    title: qsTr("Calculator in use")
    flags: Qt.Dialog
    color: "#1b1b1b"
    width: 460
    height: 260
    minimumWidth: 380
    minimumHeight: 220

    Item {
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: (event) => { root.close(); event.accepted = true }

        Column {
            anchors { fill: parent; margins: 18 }
            spacing: 12

            Label {
                text: qsTr("%1 is in use").arg(root.engine.state.instance)
                color: "#f0f0f0"; font.pixelSize: 16; font.weight: Font.DemiBold
            }

            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                color: "#d0d0d0"; font.pixelSize: 13
                text: {
                    if (root.holder.host === undefined)
                        return qsTr("Something else is holding it.")
                    if (root.holder.sameMachine && root.holder.alive)
                        return qsTr("Another Agape48 window on this computer has it open. Switch it off there — green shift, then ON — and it is yours.")
                    if (root.holder.sameMachine)
                        return qsTr("A window on this computer left it open and is no longer running.")
                    return qsTr("It was left open on %1, and has been quiet for %2 minutes.")
                           .arg(root.holder.host).arg(root.quiet)
                }
            }

            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                visible: root.holder.sameMachine === false
                color: root.probablyGone ? "#e8a55a" : "#7d7d7d"
                font.pixelSize: 11
                // The honest sentence. Taking it over while the other machine
                // is genuinely running is how both end up writing one folder,
                // which is the thing the lock exists to prevent.
                text: root.probablyGone
                      ? qsTr("Long enough that it has probably gone. If that machine is really still using it, taking it over means whichever quits last wins.")
                      : qsTr("Recent enough that it may still be in use. If it is, taking it over means whichever quits last wins.")
            }

            Item { width: 1; height: 4 }

            Row {
                spacing: 10
                Button {
                    text: qsTr("Try again")
                    onClicked: if (root.engine.attach()) root.close()
                }
                Button {
                    text: qsTr("Take it over")
                    onClicked: if (root.engine.attach(true)) root.close()
                }
                Button {
                    text: qsTr("Another calculator…")
                    onClicked: { root.close(); root.pickAnother() }
                }
                Button { text: qsTr("Cancel"); onClicked: root.close() }
            }
        }
    }

    signal pickAnother()
}
