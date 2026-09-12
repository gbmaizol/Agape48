import QtQuick
import QtQuick.Controls
import Agape48

// Tekstgrandoj, vivaj, kun la numero videbla.
//
// Gert, 2026sep03: "I'll find a size that works and make it default." Do la
// celo de ĉi tiu fenestro ne estas la ŝovbutonoj, ĝi estas la NUMERO apud ĉiu
// el ili - li ŝovas ĝis ĝi aspektas ĝusta, legas la ciferon, kaj tiu cifero
// fariĝas defaŭlto en TextSizes.qml. Ĉiu el ili aplikiĝas tuj al la fenestro
// sube, kaj tial ĉi tio estas aparta fenestro anstataŭ paĝo interne de Agordoj:
// Agordoj kovras la kalkulilon, kaj duono de tio kion oni agordas estas
// desegnita sur la kalkulilo.
Window {
    id: root

    // Transdonita al la rapidkalibrado. Vidu AdvancedContent.qml.
    property var engine: null

    readonly property bool opened: visible

    function open() {
        if (transientParent) {
            x = transientParent.x + transientParent.width + 12
            y = transientParent.y
        }
        show(); raise(); requestActivate()
    }
    function close() { hide() }

    // "Advanced" prefere ol "Text sizes" ekde 2026sep10, kiam la rapidkalibrado
    // enmoviĝis ĉi tien kaj faris mensogon el la malnova titolo. Ĝi estas ankaŭ
    // kion diras la butono kiu malfermas ĝin, kaj kiel Gert nomas ĝin: "the
    // advanced settings window".
    title: qsTr("Advanced")
    flags: Qt.Dialog
    color: "#1b1b1b"
    // Limigita al la ekrano; vidu SettingsWindow.qml por tio, kion telefono
    // faris al dialogo dimensiita por tekokomputilo.
    readonly property real fitW: Screen.desktopAvailableWidth  > 0
                                     ? Screen.desktopAvailableWidth  : 1e6
    readonly property real fitH: Screen.desktopAvailableHeight > 0
                                     ? Screen.desktopAvailableHeight : 1e6
    width: Math.min(460, fitW)
    height: Math.min(560, fitH)
    minimumWidth: Math.min(380, fitW)
    minimumHeight: Math.min(420, fitH)

    AdvancedContent {
        anchors.fill: parent
        engine: root.engine
        onCloseRequested: root.hide()
    }
}
