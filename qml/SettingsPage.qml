import QtQuick
import Agape48

// Agordoj sur telefono: la sama enhavo kiun la labortabla fenestro enkadrigas,
// en paĝo. Ĉio pri la kialo estas en PageShell.qml.
PageShell {
    id: root
    required property Agape48Engine engine

    // Kiu metis ĉi tiun paĝon, tiu metas ankaŭ la sekvan - vidu Main.qml. Paĝo
    // deklarita INTERNE de PageShell alvenas sub ĝian kapon, kio metis la
    // tekstgrandan paĝon sub la Agordan kapon sur la telefono: du reensagoj unu
    // super la alia, la malsupra paĝo pendanta trans la malsupra rando je la
    // alteco de la kapo kiu forpuŝis ĝin, kaj la supra sago morta ĉar la modala
    // paĝo super ĝi forglutis la frapeton. Mezurite ĉe b221c14.
    signal advancedRequested()

    title: qsTr("Settings")
    // La reensago demandas unue la enhavon, ĉar nur ĝi scias ĉu neaplikita
    // vojo sidas en la dosieruja kampo.
    onBackRequested: content.requestClose()

    // Fermo laŭ ordono de iu alia - la "reen" de la telefono, kiun Main.qml
    // konvertas al "fermu ĉion kaj montru al mi la kalkulilon". Ĝi pasas tra la
    // enhavo pro la sama kialo kiel la sago: vojo tajpita kaj ne aplikita
    // ankoraŭ meritas demandon, kaj la gesto estas, se io ajn, PLI verŝajne la
    // akcidento kiu perdas ĝin.
    function dismiss() { content.requestClose() }

    SettingsContent {
        id: content
        anchors.fill: parent
        engine: root.engine
        onCloseRequested: root.close()
        // Paĝo interne de ĉi tiu paĝo, kio estas kion la kion la
        // agordoj de telefono faras ĉie aliloke: "the advanced wouldn't be a new
        // window. It would be a new page inside settings, right?"
        onAdvancedRequested: root.advancedRequested()
    }
}
