import QtQuick
import QtQuick.Controls

// Ŝovbutono en la grando kaj la koloroj de CompactSwitch, ekde 2026sep15: la
// tenilo estas tiel alta kiel la trako de la ŝaltilo, kaj la trako mem estas
// maldika linio, verda ĝis la tenilo. La Slider de la stilo Basic havas tenilon
// de 32 bilderoj, destinitan por dikfingro, kaj sub ĉiu tekstgrando en
// Altnivelaj ĝi okupis pli da alto ol la teksto kiun ĝi agordas.
//
// Dimensiita el TextSizes.dialogBody, kiel CompactSwitch.
Slider {
    id: control

    padding: 0
    readonly property int knob: Math.round(TextSizes.dialogBody * 1.05)

    implicitWidth: 200
    implicitHeight: knob

    background: Rectangle {
        x: control.leftPadding + control.knob / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth - control.knob
        height: 4
        radius: height / 2
        color: "#343434"
        border.width: 1
        border.color: "#5a5a5a"

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: height / 2
            color: control.enabled ? "#5fa85f" : "#5a5a5a"
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: control.knob
        implicitHeight: control.knob
        radius: width / 2
        color: control.enabled ? "#e8e8e8" : "#8a8a8a"
        border.width: 1
        border.color: control.pressed ? "#5fa85f" : "#5a5a5a"
    }
}
