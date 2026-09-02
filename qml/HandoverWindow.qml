import QtQuick
import QtQuick.Controls
import Agape48

// "This calculator is somebody else's just now."
//
// Shown when a calculator cannot be opened because another instance holds it -
// ON refused, or one picked from the shelf.
//
// The first answer offered is to ASK, not to seize. Gert, 2026sep02: "send a
// sleep command to the other one and take it over?" That is better than taking
// it over rather than merely politer: the instance that is asked still holds
// the lock, so it saves before letting go, and the calculator that arrives here
// is everything that was done over there. A take-over cannot do that - by the
// time the loser notices, the folder is not its to write, so its last minutes
// are gone.
//
// Taking it over is still here, but only after the ask goes unanswered, which
// is the case it was always really for: a machine that is switched off.
Window {
    id: root
    required property Agape48Engine engine

    property var holder: ({})
    property string calculator: ""
    // asking -> waiting -> (closed | unanswered)
    property string phase: "asking"

    readonly property bool opened: visible

    function showFor(h, name) {
        holder = h || ({})
        // lockHolderOf() names the calculator it was asked about, so a dialog
        // raised from the shelf is about THAT one, not about whichever this
        // window happens to have open.
        calculator = name || holder.calculator || root.engine.state.instance
        phase = "asking"
        place(); show(); raise(); requestActivate()
    }

    function showUnanswered(name, host) {
        calculator = name
        if (host)
            holder = Object.assign({}, holder, { host: host })
        phase = "unanswered"
        place(); show(); raise(); requestActivate()
    }

    function place() {
        if (transientParent) {
            x = transientParent.x + (transientParent.width - width) / 2
            y = transientParent.y + 90
        }
    }

    // The engine drives the middle phase, so the dialog cannot get out of step
    // with it: whatever ends the wait ends this.
    Connections {
        target: root.engine
        function onWaitingChanged() {
            if (root.engine.waitingForCalculator())
                root.phase = "waiting"
        }
        function onOtherLetGo(name) { root.close() }
        function onSleepUnanswered(name, host) { root.showUnanswered(name, host) }
    }

    readonly property int quiet: holder.quietMinutes !== undefined ? holder.quietMinutes : -1
    readonly property bool probablyGone: holder.probablyGone === true
    readonly property string who: holder.host !== undefined && holder.host !== ""
                                  ? holder.host : qsTr("the other one")

    title: qsTr("Calculator in use")
    flags: Qt.Dialog
    color: "#1b1b1b"
    width: 560
    height: 340
    minimumWidth: 460
    minimumHeight: 300

    onVisibleChanged: if (!visible && root.engine.waitingForCalculator()) root.engine.stopWaiting()

    Item {
        anchors.fill: parent
        focus: true
        Keys.onEscapePressed: (event) => { root.close(); event.accepted = true }

        Column {
            anchors { fill: parent; margins: 18 }
            spacing: 12

            Label {
                text: qsTr("%1 is in use").arg(root.calculator)
                color: "#f0f0f0"; font.pixelSize: 16; font.weight: Font.DemiBold
            }

            // --- what is going on -------------------------------------------
            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                color: "#d0d0d0"; font.pixelSize: 13
                text: {
                    if (root.phase === "waiting")
                        return qsTr("Asked %1 to put it to sleep. Waiting for it to save and let go — %2 seconds. On one machine that takes about a second; across a synced folder it takes as long as the sync does, and a machine that is switched off or offline never answers at all.")
                               .arg(root.who).arg(root.engine.waitSeconds)
                    if (root.phase === "unanswered")
                        return qsTr("%1 has not answered. It may be switched off, or not syncing. Nothing has changed: the calculator is still theirs.")
                               .arg(root.who)
                    if (root.holder.host === undefined)
                        return qsTr("Something else is holding it. Send a sleep command and take it over?")
                    if (root.holder.sameMachine && root.holder.alive)
                        return qsTr("Another Agape48 window on this computer has it open. Send it a sleep command and take it over?")
                    if (root.holder.sameMachine)
                        return qsTr("A window on this computer left it open and is no longer running. Take it over?")
                    return qsTr("It is open on %1, last heard from %2 minutes ago. Send a sleep command to the other one and take it over?")
                           .arg(root.who).arg(root.quiet)
                }
            }

            // --- the honest footnote ----------------------------------------
            Label {
                width: parent.width
                wrapMode: Text.WordWrap
                visible: root.phase !== "waiting" && root.holder.sameMachine === false
                color: root.phase === "unanswered" || root.probablyGone ? "#e8a55a" : "#7d7d7d"
                font.pixelSize: 11
                text: root.phase === "unanswered"
                      ? qsTr("Taking it over now does not ask and does not wait. If that machine really is still using it, it loses whatever it has done since its last save, and whichever quits last wins.")
                      : qsTr("Asking is the safe one: it saves over there before it lets go, so you get everything they did.")
            }

            BusyIndicator {
                running: root.phase === "waiting"
                visible: running
                implicitWidth: 28; implicitHeight: 28
            }

            Item { width: 1; height: 4 }

            // --- asking ------------------------------------------------------
            Flow {
                width: parent.width
                spacing: 10
                visible: root.phase === "asking"
                Button {
                    text: qsTr("Yes")
                    // Ask, then take it when they let go. If nobody is really
                    // holding it any more this comes straight back.
                    onClicked: root.engine.askForCalculator(root.calculator, true)
                }
                Button {
                    text: qsTr("Sleep")
                    // Ask, and leave it. "Stop holding it" without "give it to
                    // me now" - the calculator ends up free for whoever wants it.
                    onClicked: { root.engine.askForCalculator(root.calculator, false); root.close() }
                }
                Button {
                    text: qsTr("Choose another…")
                    onClicked: { root.close(); root.pickAnother() }
                }
                Button {
                    text: qsTr("Another folder…")
                    onClicked: { root.close(); root.changeFolder() }
                }
                Button { text: qsTr("Cancel"); onClicked: root.close() }
            }

            // --- waiting -----------------------------------------------------
            // Gert, 2026sep02: a countdown, and a way out of it that is not
            // just cancelling - force it, pick another, or go somewhere else
            // entirely. Waiting is the polite answer, not the only one.
            Flow {
                width: parent.width
                spacing: 10
                visible: root.phase === "waiting"
                Button {
                    text: qsTr("Take it over now")
                    onClicked: {
                        root.engine.stopWaiting()
                        if (root.engine.takeOverCalculator(root.calculator))
                            root.close()
                    }
                }
                Button {
                    text: qsTr("Choose another…")
                    onClicked: { root.engine.stopWaiting(); root.close(); root.pickAnother() }
                }
                Button {
                    text: qsTr("Another folder…")
                    onClicked: { root.engine.stopWaiting(); root.close(); root.changeFolder() }
                }
                Button {
                    text: qsTr("Stop waiting")
                    onClicked: { root.engine.stopWaiting(); root.close() }
                }
            }

            // --- nobody answered ---------------------------------------------
            Flow {
                width: parent.width
                spacing: 10
                visible: root.phase === "unanswered"
                Button {
                    text: qsTr("Take it over")
                    onClicked: if (root.engine.takeOverCalculator(root.calculator)) root.close()
                }
                Button {
                    text: qsTr("Ask again")
                    onClicked: root.engine.askForCalculator(root.calculator, true)
                }
                Button {
                    text: qsTr("Choose another…")
                    onClicked: { root.close(); root.pickAnother() }
                }
                Button { text: qsTr("Cancel"); onClicked: root.close() }
            }
        }
    }

    signal pickAnother()
    // "None of these": the shelf itself is the wrong one. Opens Settings, where
    // the state folder lives.
    signal changeFolder()
}
