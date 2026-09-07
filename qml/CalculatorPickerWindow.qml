import QtQuick
import QtQuick.Controls
import Agape48

// The calculator shelf, in a window of its own. Desktop only - see
// CalculatorPickerPage.qml for the phone, and CalculatorPickerContent.qml for
// the part that is the same on both.
Window {
    id: root
    required property Agape48Engine engine

    readonly property bool opened: visible
    signal busyCalculator(string name, var holder)

    function openPicker() {
        content.refresh()
        if (transientParent) {
            x = transientParent.x + (transientParent.width - width) / 2
            y = transientParent.y + 70
        }
        show(); raise(); requestActivate()
    }

    title: qsTr("Calculators")
    flags: Qt.Dialog
    color: "#1b1b1b"
    // Clamped to the screen; see SettingsWindow.qml for what a phone did to a
    // dialog sized for a laptop.
    readonly property real fitW: Screen.desktopAvailableWidth  > 0
                                     ? Screen.desktopAvailableWidth  : 1e6
    readonly property real fitH: Screen.desktopAvailableHeight > 0
                                     ? Screen.desktopAvailableHeight : 1e6
    width: Math.min(460, fitW)
    height: Math.min(400, fitH)
    minimumWidth: Math.min(380, fitW)
    minimumHeight: Math.min(300, fitH)

    CalculatorPickerContent {
        id: content
        anchors.fill: parent
        engine: root.engine
        onCloseRequested: root.close()
        onBusyCalculator: (name, holder) => root.busyCalculator(name, holder)
    }
}
