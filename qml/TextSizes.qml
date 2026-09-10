pragma Singleton
import QtQuick
import QtCore

// Ĉiu tekstgrando kiun la aplikaĵo desegnas je RULTEMPO, en unu loko, por ke
// oni povu agordi ilin rigardante ilin anstataŭ diveni kaj rekonstrui.
//
// Gert, 2026sep03: "expose the size of all kinds of text on banners messages,
// calculator buttons text, text over buttons, on a separate settings window
// that appears when clicking on 'Advanced' on the settings window. I'll find a
// size that works and make it default."
//
// KIO NE ESTAS ĈI TIE, KAJ NE POVAS ESTI. La klavetikedoj kaj la surskriboj
// presitaj super ili ne estas teksto je rultempo - ili estas pentritaj en
// face.png de tools/makeface.py el sep numeroj en tools/face.json, je
// konstrutempo. Tial ili restas akraj je ĉia fenestrogrando, kaj tial nenia
// ŝovbutono povas movi ilin. AdvancedWindow.qml listigas tiujn sep por ke ili
// estu almenaŭ videblaj kaj nomeblaj, kaj diras klare ke ili bezonas
// rekonstruon.
//
// La defaŭltoj estas la valoroj kiuj estis fikse enkoditaj antaŭ ol ĉi tio
// ekzistis, do freŝa profilo aspektas ekzakte kiel antaŭe. Fari konstanta unu
// el liaj numeroj signifas ŝanĝi la defaŭlton ĈI TIE; la konservita valoro
// registras nur superregon.
//
// Maŝin-loka, kiel la fenestra geometrio kaj la klavmapo: tekstgrando apartenas
// al ekrano kaj al paro da okuloj, ne al la kalkulila memoro kiu sinkroniĝas
// inter maŝinoj.
QtObject {
    id: root

    // Kromnomoj prefere ol ligoj. Ŝovbutono skribanta al ligita atributo rompas
    // la ligon je la unua ŝovo kaj la valoro ĉesas esti konservata - kromnomo
    // havas unu konservlokon kaj ne povas malakordiĝi kun si mem.
    property alias banner: store.banner                 // eraroj kaj avizoj super la vizaĝo
    property alias screenMessage: store.screenMessage   // pentrita sur malplena LCD
    property alias dialogTitle: store.dialogTitle       // duarangaj fenestroj
    property alias dialogBody: store.dialogBody
    property alias dialogHint: store.dialogHint         // la grizaj klarigaj linioj

    readonly property int defaultBanner: 12
    readonly property int defaultScreenMessage: 52
    readonly property int defaultDialogTitle: 16
    readonly property int defaultDialogBody: 13
    readonly property int defaultDialogHint: 11

    // La intervalo inda je ŝovado, ne validiga regulo.
    readonly property int minSize: 6
    readonly property int maxSize: 72

    // La klavaj ŝpruchelpiloj: DERIVITA, ne konservita, kaj tial ne sur
    // ŝovbutono. Gert, 2026sep10: "make the size of the tooltip text 1.5 times
    // as big as the main menu text" - kio estas proporcio, kaj proporcio meritas
    // resti proporcio, ĉar la menuo estas desegnita per kio ajn la sistemo
    // diras, kaj tio diferencas po maŝino. La eroj de la ⋮-menuo metas
    // nenian propran tiparon, do tio per kio ili desegniĝas estas ekzakte
    // Qt.application.font: mezurita ĉi tie je 2026sep10 kiel Segoe UI,
    // pixelSize 12, pointSize 9, do ĉi tio rezultas je 18.
    //
    // Ambaŭ branĉoj ĉar QFont respondas nur pri la grando per kiu ĝi estis
    // konstruita: pixelSize estas -1 kiam ĝi venis el punktogrando, kaj
    // pointSize estas -1 inverse. Vindozo plenigas ambaŭ, fontconfig sur
    // Linukso ne nepre. Meti ambaŭ sur unu tiparon estas rultempa averto, do
    // Qt.font() kun unu el ili anstataŭ du atribuoj.
    readonly property font keyTip: {
        const f = Qt.application.font
        return f.pixelSize > 0
             ? Qt.font({ family: f.family, pixelSize: Math.round(f.pixelSize * 1.5) })
             : Qt.font({ family: f.family, pointSize: f.pointSize * 1.5 })
    }

    function reset() {
        banner = defaultBanner
        screenMessage = defaultScreenMessage
        dialogTitle = defaultDialogTitle
        dialogBody = defaultDialogBody
        dialogHint = defaultDialogHint
    }

    readonly property bool changed: banner !== defaultBanner
                                    || screenMessage !== defaultScreenMessage
                                    || dialogTitle !== defaultDialogTitle
                                    || dialogBody !== defaultDialogBody
                                    || dialogHint !== defaultDialogHint

    property Settings store: Settings {
        id: store
        category: "textsize"
        property int banner: root.defaultBanner
        property int screenMessage: root.defaultScreenMessage
        property int dialogTitle: root.defaultDialogTitle
        property int dialogBody: root.defaultDialogBody
        property int dialogHint: root.defaultDialogHint
    }
}
