import QtQuick
import Agape48

// The calculator shelf on a phone: a page, not a window. Everything about why
// is in PageShell.qml - a second Window makes the platform plugin acquire a
// surface, and that call aborts the process. Measured on Gert's phone: Settings
// 0 opened out of 10 as a window and 10 out of 10 as a page, while this one -
// left as a window on purpose, as the control - stayed at 0 out of 5.
//
// So this is the same fix applied to the last thing the menu could still reach.
// It matters more than Settings did: the shelf is the ONLY way to a calculator
// that arrived from another machine, which makes it the whole point of putting
// the state folder in a synced folder in the first place.
PageShell {
    id: root
    required property Agape48Engine engine

    signal busyCalculator(string name, var holder)

    function openPicker() {
        content.refresh()
        open()
    }

    title: qsTr("Calculators")
    onBackRequested: root.close()

    CalculatorPickerContent {
        id: content
        anchors.fill: parent
        engine: root.engine
        onCloseRequested: root.close()
        onBusyCalculator: (name, holder) => root.busyCalculator(name, holder)
    }
}
