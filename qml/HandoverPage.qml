import QtQuick
import Agape48

// "This calculator is somebody else's just now.", kiel paĝo, sur telefono.
// Ĉio pri kial ĉi tio estas paĝo kaj ne fenestro estas en PageShell.qml; ĉio
// pri tio kion ĝi diras estas en HandoverContent.qml.
//
// Ĝi gravas pli ĉi tie ol la aliaj du paĝoj gravis. Sur labortablo ĉi tiu
// dialogo estas ĝentilaĵo - ambaŭ Agape48-oj kutime estas sur la sama maŝino,
// kaj la malgajninton oni povas fermi permane. Sur telefono la alia tenanto
// estas ALIA APARATO, ĉe la fino de sinkroniga kliento, kaj ĉi tiu paĝo estas
// la sola vojo peti de ĝi la kalkulilon, rigardi la retronombradon, aŭ decidi
// preni ĝin malgraŭe.
PageShell {
    id: root
    required property Agape48Engine engine
    property alias live: content.live

    readonly property string phase: content.phase

    signal pickAnother()
    signal changeFolder()

    function showFor(h, name) { content.showFor(h, name) }
    function showUnanswered(name, host, why) { content.showUnanswered(name, host, why) }

    title: qsTr("Calculator in use")
    // Ambaŭ elirvojoj signifas la samon ĉi tie - nenio tajpita estas
    // perdebla, kaj nenia paĝo sub ĉi tiu al kiu supreniri. Foriro haltigas la
    // atendon, alie la motoro plu nombrus por dialogo kiun neniu povas vidi.
    onBackRequested: root.leave()
    onDismissRequested: root.leave()

    function leave() {
        if (root.engine.waitingForCalculator())
            root.engine.stopWaiting()
        root.close()
    }

    HandoverContent {
        id: content
        anchors.fill: parent
        engine: root.engine
        onCloseRequested: root.leave()
        onRaiseRequested: root.open()
        onPickAnother: root.pickAnother()
        onChangeFolder: root.changeFolder()
    }
}
