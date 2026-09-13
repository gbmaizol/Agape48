import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import Agape48

// Agordoj, en propra fenestro.
//
// Ĉi tio estis malsupra folio kuŝanta super la kalkulila vizaĝo ĝis 2026aug29,
// kaj tri sinsekvaj raportoj atentigis pri tio antaŭ ol demandi rekte kial. La
// honesta respondo estas ke ĝi estis skribita telefon-unue - folio glitanta
// supren de la malsupra rando estas la Androida idiomaĵo - kaj poste neniam
// rerigardita kiam la labortabla konstruo montriĝis esti tiu kiu ekzistas. Sur
// fenestro larĝa je 451 ĝi ne enkadriĝis, ĝi kaŝis la kalkulilon, kaj ĝi estis
// la rekta kaŭzo de la morta klavaro en dogfood #5: ĝiaj tekstkampoj prenis la
// fokuson interne de la ĉefa fenestro kaj nenio redonis ĝin. Aparta fenestro ne
// povas fari tion, ĉar fermi ĝin reaktivigas la ĉefan fenestron, kaj la ĉefa
// fenestro redonas la klavaron al la klavaro de la kalkulilo survoje enen.
//
// Ĉio ĉi tie faras ion. Tri regiloj estis forigitaj prefere ol lasitaj aspekti
// veraj:
//
//   Copy stack / Paste / Save now - jam en la ⋮-menuo, kaj la regulo estas ke
//     unu komando apartenas al unu loko.
//   Haptics / Beep - du ŝaltiloj super Feedback-unuopaĵo kiu ankoraŭ estas
//     TODO, do ili baskuligis absolute nenion.
//   Pick folder… - ĝi vokis StateFileManager::requestLocation(), kiu sur
//     labortablo nur eligas pickerRequested() kaj atendas FolderDialog kiun
//     neniu montris. Dogfood #7 petis veran, do la "…"-butono apud la
//     stata dosierujo nun estas QtQuick.Dialogs-a FolderDialog - denaska sur ĉi
//     tiu labortablo, ĉar Qt liveras la platforman etoson gtk3. Tio estas unu
//     plia Qt-modulo kontraŭ la maldika-modula regulo, kaj la escepto estas
//     intenca.
// Androida noto por poste: dua supranivela Window estas labortabla idiomaĵo.
// Kiam la Androida konstruo okazos, ĉi tio volas fariĝi tutekrana paĝo aŭ
// denove folio, ĉar telefono havas nenian fenestroadministrilon kiu metus ĝin
// ien racian.
Window {
    id: root
    required property Agape48Engine engine

    // Main.qml demandas ĉi tion por decidi ĉu la klavaro de la kalkulilo rajtas
    // preni la klavaron.
    readonly property bool opened: visible

    function open() {
        if (transientParent) {
            x = transientParent.x + (transientParent.width - width) / 2
            y = transientParent.y + 48
        }
        show(); raise(); requestActivate()
    }
    // Ambaŭ elirvojoj demandas unue la enhavon, ĉar nur ĝi scias ĉu estas
    // neaplikita vojo en la dosieruja kampo.
    function close() { content.requestClose() }

    onActiveChanged: if (!active && content.statePending) content.warnUnsaved = true

    onClosing: (event) => {
        if (content.statePending) {
            event.accepted = false
            content.requestClose()
        }
    }

    title: qsTr("Agape48 settings")
    flags: Qt.Dialog
    color: "#1b1b1b"
    // Metita unufoje, ne ligita: ligo sur la grando de fenestro batalas kontraŭ
    // la propra ŝovo de la uzanto kaj resnapas ĝin, kio estas la kaptilo kiun
    // Main.qml dokumentas.
    // Limigita al la ekrano, ĉar la ekrano de telefono povas esti pli malgranda
    // ol dialogo skribita por tekokomputilo. Mezurite sur la telefono ĉe
    // ef76dd2: la ekrano estas 458 unuojn larĝa kaj ĉi tiu fenestro petis 520,
    // do ĝi estis centrita sur io pli larĝa ol la ekrano kaj detranĉita ĉe
    // AMBAŬ randoj - "P 48 ROM" anstataŭ "HP 48 ROM", la foliumbutonoj trans la
    // dekstra flanko. La minimumoj gravas tiom kiom la grandoj: minimumWidth pli
    // larĝa ol la ekrano puŝus ĝin tuj reeksteren. La gardilo estas por
    // desktopAvailable* revenanta kiel 0 antaŭ ol la fenestro estas mapita, kion
    // Main.qml dokumentas; Math.min kontraŭ 0 donus fenestron kun neniu grando.
    readonly property real fitW: Screen.desktopAvailableWidth  > 0
                                     ? Screen.desktopAvailableWidth  : 1e6
    readonly property real fitH: Screen.desktopAvailableHeight > 0
                                     ? Screen.desktopAvailableHeight : 1e6
    width: Math.min(520, fitW)
    height: Math.min(430, fitH)
    minimumWidth: Math.min(380, fitW)
    minimumHeight: Math.min(340, fitH)

    SettingsContent {
        id: content
        anchors.fill: parent
        engine: root.engine
        onCloseRequested: root.hide()
        onAdvancedRequested: advanced.open()
    }

    // Propra fenestro prefere ol paĝo ĉi tie, ĉar duono de tio kion ĝi agordas
    // estas desegnita sur la kalkulilo kaj ĉi tiu fenestro kovras ĝin.
    // Fenestro, kaj tial nur labortabla ĝis ĝi ricevos la saman traktadon kiel
    // ĉi tiu.
    AdvancedWindow {
        id: advanced
        engine: root.engine
        transientParent: root
    }
}
