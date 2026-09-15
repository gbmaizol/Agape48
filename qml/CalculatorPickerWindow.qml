import QtQuick
import QtQuick.Controls
import Agape48

// La kalkulilbreto, en propra fenestro. Nur labortabla - vidu
// CalculatorPickerPage.qml por la telefono, kaj CalculatorPickerContent.qml por
// la parto kiu estas sama sur ambaŭ.
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
            engine.keepOnScreen(root)
        }
        show(); raise(); requestActivate()
    }

    title: qsTr("Calculators")
    flags: Qt.Dialog
    color: "#1b1b1b"
    // Limigita al la ekrano; vidu SettingsWindow.qml por tio, kion telefono
    // faris al dialogo dimensiita por tekokomputilo.
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
