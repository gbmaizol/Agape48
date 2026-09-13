pragma Singleton
import QtQuick
import QtCore

// Ĉiu tekstgrando kiun la aplikaĵo desegnas je RULTEMPO, en unu loko, por ke
// oni povu agordi ilin rigardante ilin anstataŭ diveni kaj rekonstrui.
//
// 2026sep03: "expose the size of all kinds of text on banners messages,
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
// ekzistis, do freŝa profilo aspektas ekzakte kiel antaŭe. Fari unu el la
// agorditaj numeroj konstanta signifas ŝanĝi la defaŭlton ĈI TIE; la konservita
// valoro registras nur superregon.
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
    // ŝovbutono. 2026sep10, unue: "make the size of the tooltip text 1.5
    // times as big as the main menu text" - proporcio, kiu naskis funkcion kiu
    // legis Qt.application.font kaj remultiplikis per la vizaĝskalo por elveni
    // konstanta. Poste, la saman tagon kaj vidinte ĝin: "Also, now the font
    // size it too big. Make it 12pt." Numero venkas proporcion, do la funkcio
    // malaperis kaj restas ĉi tiu linio.
    //
    // PUNKTOJ KAJ NE BILDEROJ, malkiel ĉio alia en ĉi tiu dosiero. La
    // ŝpruchelpilo nun pendas EKSTER la Scale-transformo de la vizaĝo - vidu
    // Calculator.qml - do ĝi estas fenestra ĉirkaŭaĵo kiel dialoga teksto, kaj
    // punkto sekvas la punktodenson de la ekrano dum bildero ne.
    readonly property int keyTip: 12

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
