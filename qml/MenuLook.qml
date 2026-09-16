pragma Singleton
import QtQuick
import QtCore

// LA ASPEKTO DE LA MENUO SUB 48GX, unu el tri, elektebla en Altnivelaj ekde
// 2026sep15 (provo 22 linio 23):
//
//   "screen"   la ekrano de la kalkulilo: verda vitro en nigra kadro, malhelaj
//              literoj, kaj la elektita menuero en inversa video. La defaŭlto.
//   "button"   la klavoj: la malhela supra koloro de klavo, kaj la gradiento de
//              klavo nur laŭ la dekstra kaj la malsupra randoj.
//   "classic"  la korpo: ĝia koloro kaj ĝia pli hela rando.
//
// La koloroj venas el tools/face.json, kies nomojn la komentoj apud ili donas.
// Main.qml desegnas la tri fonojn; ĉi tie estas la elekto kaj la koloroj de la
// menueroj.
//
// Maŝin-loka, kiel TextSizes.qml kaj la fenestra geometrio: aspekto apartenas al
// ekrano, ne al la kalkulila memoro kiu sinkroniĝas inter maŝinoj.
QtObject {
    id: root

    property alias look: store.look
    readonly property string defaultLook: "screen"

    // Nekonata valoro, ekzemple el pli nova versio, legiĝas kiel la defaŭlto.
    readonly property bool screen: look !== "button" && look !== "classic"
    readonly property bool button: look === "button"

    // La teksto de menuero, kaj ĝia teksto kiam ĝi estas elektita.
    readonly property color ink: screen ? "#0d1a0d" : "#f0f0f2"          // colour_lcd_pixel, colour_text
    readonly property color inkChosen: screen ? "#9fbf7a" : "#f0f0f2"    // colour_lcd_bg
    // La tavolo sub elektita kaj sub premita menuero. La klavoj uzas la blankan
    // tavolon de premita klavo en Keypad.qml; la korpo uzas sian randon.
    readonly property color chosen: screen ? "#0d1a0d" : button ? "#24ffffff" : "#3e3e46"   // colour_body_edge
    readonly property color pressed: screen ? "#0d1a0d" : button ? "#38ffffff" : "#4a4a54"
    // La linioj inter la grupoj de menueroj.
    readonly property color rule: screen ? "#0d1a0d" : button ? "#40ffffff" : "#3e3e46"

    property Settings store: Settings {
        id: store
        category: "menu"
        property string look: root.defaultLook
    }
}
