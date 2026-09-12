import QtQuick
import Agape48

// La kalkulilbreto sur telefono: paĝo, ne fenestro. Ĉio pri la kialo estas en
// PageShell.qml - dua Window igas la platforman kromaĵon akiri surfacon, kaj
// tiu voko abortas la procezon. Mezurite sur la telefono: Agordoj
// malfermiĝis 0 el 10 fojoj kiel fenestro kaj 10 el 10 kiel paĝo, dum ĉi tiu -
// intence lasita fenestro, kiel la kontrolo - restis ĉe 0 el 5.
//
// Do jen la sama korekto aplikita al la lasta afero kiun la menuo ankoraŭ povis
// atingi. Ĝi gravas pli ol Agordoj gravis: la breto estas la SOLA vojo al
// kalkulilo kiu alvenis de alia maŝino, kio faras ĝin la tuta celo de meti la
// statan dosierujon en sinkronigatan dosierujon unuavice.
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
