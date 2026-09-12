import QtQuick
import QtQuick.Controls
import Agape48

// Tutekrana paĝo, por la platformo kiu havas nur unu fenestron.
//
// Androido ne havas duan fenestron: peti unu de Qt igas la platforman kromaĵon
// akiri denaskan surfacon, kaj tiu voko prenas procez-vastan seruron kiun tenas
// la alirebleca ponto, post kio Qt abortas anstataŭ interblokiĝi. Mezurite sur
// la telefono ĉe 67b2f08: Agordoj malfermiĝis 0 el 10, "Open another
// calculator" 0 el 3. Mezurite denove ĉe bc57de7, kun la agorda enhavo en paĝo
// kiel ĉi tiu: Agordoj 10 el 10, kaj la elektilo - ankoraŭ Window, kaj tial la
// kontrolo - ankoraŭ 0 el 5.
//
// La kaŭzo kaj la formo estis nomitaj kune: la kraŝo estas labortabla trajto
// kiu ne ekzistas sur Androido, kaj la respondo estas paĝoj en la stilo de
// Androidaj agordoj anstataŭ desegnitaj fenestroj. "Maybe
// they are usually like this for a reason."
//
// Popup estas desegnita interne de la sceno kiu jam ekzistas, do ĝi neniam
// petas surfacon. EN-SCENE NE ESTAS FOLIO: ĉi tio plenigas kion oni donas al ĝi
// kaj pentras maldiafanan fonon, do ĝi estas paĝo - ne la malsupra folio kiun
// la projekto forlasis je 2026aug29, kaj la kalkulilo ne trabrilas.
//
// Paĝoj staplas. Unu malfermita el alia sidas super ĝi kaj la reensago forprenas
// la supran, kio estas kion faras la agordoj de telefono: Altnivela ne estas
// nova fenestro, ĝi estas nova paĝo interne de Agordoj.
Popup {
    id: root

    // Kion la kapo diras. La reensago ne estas laŭvola: paĝo sen elirvojo estas
    // la kaptilo pri kiu Main.qml avertas en la bandrolo de la agorda reĝimo.
    property string title: ""
    signal backRequested()

    // LA SISTEMA "REEN" NE ESTAS LA DESEGNITA SAGO, ekde dogfood android-08. La
    // sago signifas "unu paĝon supren" - ĝi estas kio reportas Tekstgrandojn al
    // Agordoj. La propra "reen" de la telefono signifas ion pli fortan:
    // "The back button should take out of every internal configs or
    // selections, stopping at the calculator", kaj "make it go back to the
    // calculator if swiping back or clicking the back bottom-button." Do la
    // gesto kaj la naviga strio eligas ĉi tion anstataŭe, kaj Main.qml - kiu
    // estas la sola loko kiu scias kiom da paĝoj estas malfermitaj - fermas
    // ĉiujn.
    signal dismissRequested()

    // Ĉio deklarita interne de PageShell alvenas sub la kapon.
    default property alias pageContent: body.data

    // Fiksita prefere ol lasita al la defaŭlto. Simpla Popup jam solviĝas al
    // Item - mezurite sur Linukso kun protokolado de qt.quick.controls.popup: 42
    // popup-linioj kaj neniu ŝprucfenestro, sur platformo kiu estus farinta unu
    // se tio estus la defaŭlto - sed la tuta korekto ripozas sur ĉi tiu
    // atributo, do ĝi estas skribita anstataŭ heredita. Qt 6.8 aldonis ĝin, kio
    // estas pli malnova ol la SafeArea kiun la kalkulilo jam bezonas, do ĝi
    // kostas nenian version.
    popupType: Popup.Item

    modal: true
    // Nenio por ombri: ĝi kovras ĉion kion oni donas al ĝi, kaj malluma tavolo
    // kostus nur tutekranan kunmiksadon ĉiun kadron.
    dim: false
    padding: 0
    // Nenio fermas ĉi tiun paĝon per si mem: ĉiu elirvojo pasas tra
    // backRequested() aŭ dismissRequested(), por ke paĝo kiu havas ion por
    // demandi antaŭ la fermo ankoraŭ povu demandi ĝin.
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

            // Desegnita, ne signo el tiparo. "←" estas harlinio en la plimulto
            // de la tiparoj kiujn telefono vere havas, kio estas la sama kialo
            // pro kiu la domo en SettingsContent estas Canvas anstataŭ "⌂".
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

    // La reen-gesto de Androido alvenas kiel klavpremo, kaj la manifesto faras
    // sian eblon por teni ĝin tia: android:enableOnBackInvokedCallback estas
    // false tie intence, ĉar aliĝi al la anstataŭaĵo de Androido 13 haltigas la
    // klavon antaŭ ol ĝi iam atingas ĉi tiun traktilon - vidu la manifeston. Esk
    // estas traktata interne de la enhavo de ĉiu paĝo, kie ĝi ne povas eskapi al
    // la kalkulilo - vidu SettingsContent.qml.
    //
    // SUR LA ERO KIU ENHAVAS LA PAĜON, kaj tio estas la tuta korekto. Ĝi estis
    // unue sur Popup, kie `Keys` tute ne povas alkroĉiĝi ("Could not attach Keys
    // property to: PageShell ... is not an Item"), do ĝi estis mortinta kodo.
    // Poste ĝi estis sur fokus-tenanta Item desegnita sub la paĝo - kio
    // funkciis ĝuste ĝis vi tuŝis ion ajn. Klavevento iras al la fokusa ero kaj
    // poste supren laŭ ĝia GEPATRA ĉeno; kaptilo apud la enhavo estas gefrato,
    // ne prapatro, do en la momento kiam tekstkampo aŭ butono interne de la paĝo
    // prenis la fokuson, la reen-klavo preterpasis ĝin kaj nenio okazis. Jen
    // precize dogfood android-08, linio 8: en iuj cirkonstancoj, ekzemple tuj
    // post eniro en la agordojn, reen-svingo iris rekte al la kalkulilo - la
    // cirkonstanco estante ke la paĝo ankoraŭ ne estis tuŝita.
    //
    // Ĉio deklarita interne de PageShell alvenas ĉi tien, do ĉi tie la
    // reen-klavo estas ĉiam kontraŭflue de kio ajn havas la fokuson.
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
