import QtQuick
import QtQuick.Controls
import Agape48

// "About Agape48" - la enhavo, sen kadro ĉirkaŭ ĝi, por ke labortablo povu meti
// fenestron ĉirkaŭ ĝi kaj telefono paĝon. Sama divido kiel Agordoj, la breto kaj
// la transdono; vidu PageShell.qml pri kial ĝi ne estas elektebla.
//
// Provo 17 linio 4, el la Vindoza instalilo: la menuo ricevas eron "About" kun
// la titolo, la versio, la konstrunumero kaj tempindiko kiun la konstruo mem
// faras, plus kelkajn alineojn pri Agape48. La pozicio estis lasita al ĉi tiu
// dosiero por elekti.
//
// LA POZICIO, ĉar li petis ke ni elektu: propra sekcio malsupre, super Fini.
// Tio estas kie ĉiu labortabla programo metas ĝin, kaj la menuo jam finiĝas per
// la du danĝeraj eroj - "Reset memory and quit" kaj "Quit" - kiujn separatoro
// jam apartigas de ĉio alia. About eniras super tiu separatoro anstataŭ sub ĝi,
// do la lasta grupo restas "la du kiuj fermas la programon" kaj nenio sendanĝera
// sidas inter ili.
//
// LA KAPTILO NE ESTAS HORLOĜA TEMPO, kaj tio estas elekto: vidu
// cmake/BuildStamp.cmake. Ĝi estas la enarbigo kaj ĝia dato, kio ŝanĝiĝas
// ekzakte kiam la fonto ŝanĝiĝas anstataŭ je ĉiu ligado - kaj tio respondas la
// demandon kiun kaptilo devas respondi, "ĉu ĉi tiu duumaĵo enhavas la riparon".
// `+` post la hash signifas ke la arbo portis neenarbigitajn ŝanĝojn.
Item {
    id: root
    required property Agape48Engine engine

    // La kadro fermas sin mem; la enhavo neniam faras tion.
    signal closeRequested()

    implicitWidth: 520
    implicitHeight: column.implicitHeight + 36

    Rectangle {
        anchors.fill: parent
        color: "#1b1b1b"

        // Rulebla, ĉar du alineoj sur telefono en horizontala pozicio ne kongruas.
        Flickable {
            anchors { fill: parent; margins: 18 }
            contentHeight: column.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: column
                width: parent.width
                spacing: 10

                Row {
                    spacing: 12
                    Image {
                        // La sama dosiero kiun setWindowIcon prenas, do la
                        // fenestro kiu diras kiu ĝi estas montras la ikonon kiun
                        // la taskostrio desegnas por ĝi.
                        source: "qrc:/qt/qml/Agape48/assets/icon.png"
                        sourceSize.width: 64
                        sourceSize.height: 64
                        width: 64
                        height: 64
                    }
                    Column {
                        spacing: 2
                        Text {
                            text: qsTr("Agape48")
                            color: "#f0f0f0"
                            font.pixelSize: TextSizes.dialogTitle + 6
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: qsTr("HP 48GX emulator")
                            color: "#d0d0d0"
                            font.pixelSize: TextSizes.dialogBody
                        }
                    }
                }

                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: "#d0d0d0"
                    font.pixelSize: TextSizes.dialogBody
                    text: qsTr("Agape48 runs the real HP 48GX ROM on the x48 core, so what is "
                             + "on the screen is the calculator's own software rather than an "
                             + "imitation of it - the same RPN stack, the same menus, the same "
                             + "68 kilobytes of Saturn code, and the same behaviour when you "
                             + "ask it something it does not like. The face is not a photograph: "
                             + "every key, legend and gradient is drawn from the real machine's "
                             + "measurements, which is why the lettering stays sharp at any size.")
                }

                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: "#d0d0d0"
                    font.pixelSize: TextSizes.dialogBody
                    text: qsTr("A calculator here is an ordinary folder, not a database. Point "
                             + "Agape48 at a folder your own sync client already keeps, and the "
                             + "same calculator can be picked up on another machine where you "
                             + "left it - one at a time, and it says so when somebody else has "
                             + "it. Nothing is uploaded anywhere, no account is needed, and on "
                             + "the phone the app asks for no permissions at all.")
                }

                Item { width: 1; height: 4 }

                // La kaptilo. Unu linio po fakto, do "kiun konstruon vi kuras"
                // estas kopiebla en raporton sen retajpi ĝin.
                Grid {
                    columns: 2
                    rowSpacing: 3
                    columnSpacing: 14

                    Text {
                        text: qsTr("Version")
                        color: "#7d7d7d"; font.pixelSize: TextSizes.dialogHint
                    }
                    Text {
                        text: Qt.application.version
                        color: "#d0d0d0"; font.pixelSize: TextSizes.dialogHint
                    }

                    Text {
                        text: qsTr("Build")
                        color: "#7d7d7d"; font.pixelSize: TextSizes.dialogHint
                        visible: root.engine.buildStamp !== ""
                    }
                    Text {
                        text: root.engine.buildStamp
                        color: "#d0d0d0"; font.pixelSize: TextSizes.dialogHint
                        visible: root.engine.buildStamp !== ""
                    }

                    Text {
                        text: qsTr("Qt")
                        color: "#7d7d7d"; font.pixelSize: TextSizes.dialogHint
                    }
                    Text {
                        text: root.engine.qtVersion + " · " + Qt.platform.os
                        color: "#d0d0d0"; font.pixelSize: TextSizes.dialogHint
                    }
                }

                Text {
                    width: parent.width
                    wrapMode: Text.WordWrap
                    color: "#7d7d7d"
                    font.pixelSize: TextSizes.dialogHint
                    text: qsTr("Free software under the GNU General Public License, version 3 "
                             + "or later, and it comes with no warranty. The ROM is not part "
                             + "of it and never has been: that is Hewlett-Packard's, and you "
                             + "bring your own.")
                }

                Item { width: 1; height: 4 }

                Button {
                    text: qsTr("Close")
                    onClicked: root.closeRequested()
                }
            }
        }
    }
}
