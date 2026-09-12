import QtQuick
import QtQuick.Controls

// Ŝaltilo desegnita por sidi en linio de dialoga teksto, kaj portanta sian
// propran etikedon.
//
// 2026sep10: "making the toggles and the space between them smaller, more
// like the size and space of normal text". La Switch de la stilo Basic estas
// desegnita por dikfingro sur telefono - ĉirkaŭ 40 bilderoj da indikilo por
// vico - kaj kvar el ili en kolumno estis la plimulto de tio kio enkadriĝis en
// lian agordan fenestron, kio estas duono de la kialo pro kiu la rapidregiloj
// sub ili estis trans la malsupra rando kaj kostis al li vesperon da mezuroj
// prenitaj je rapido kiun li ne povis vidi. Vidu prokrastitajn erojn 1 kaj 2 en
// docs/design-questions.md.
//
// LA ETIKEDO ESTAS PARTO DE LA REGILO prefere ol Label apud ĝi, kion ĉiu
// vokloko antaŭe bezonis, pro kialo inda je konservo: la stilo Basic pentras
// Switch.text per malhela inko destinita por hela fenestro, kaj sur ĉi tiu fono
// ĝi estis preskaŭ nelegebla. Superregi contentItem korektas tion unufoje
// anstataŭ kvarfoje, kaj tio ankaŭ signifas ke la vortoj estas klakeblaj, kio
// ili antaŭe ne estis.
//
// Dimensiita el TextSizes.dialogBody, do ĝi sekvas la tekston apud kiu ĝi sidas
// kiam li movas tiun ŝovbutonon, anstataŭ bezoni propran numeron.
Switch {
    id: control

    padding: 0
    spacing: Math.round(TextSizes.dialogBody * 0.5)

    // Alteco de linio de korpa teksto, larĝo iom malpli ol duoblo de tiu.
    // Rondigita al plenaj bilderoj: radiuso sur frakcia alteco lasas molan
    // randon.
    readonly property int trackHeight: Math.round(TextSizes.dialogBody * 1.05)
    readonly property int trackWidth:  Math.round(TextSizes.dialogBody * 1.85)

    implicitHeight: Math.max(trackHeight, contentItem.implicitHeight)

    indicator: Rectangle {
        implicitWidth:  control.trackWidth
        implicitHeight: control.trackHeight
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: height / 2
        color: control.checked ? "#3f6f3f" : "#343434"
        border.width: 1
        border.color: control.checked ? "#5fa85f" : "#5a5a5a"

        Rectangle {
            readonly property int inset: 2
            x: control.checked ? parent.width - width - inset : inset
            y: inset
            width: parent.height - 2 * inset
            height: width
            radius: width / 2
            color: control.enabled ? "#e8e8e8" : "#8a8a8a"
            Behavior on x { NumberAnimation { duration: 90 } }
        }
    }

    contentItem: Text {
        text: control.text
        color: control.enabled ? "#e8e8e8" : "#8a8a8a"
        font.pixelSize: TextSizes.dialogBody
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + control.spacing
    }
}
