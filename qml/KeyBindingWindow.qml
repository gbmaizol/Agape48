import QtQuick
import QtQuick.Controls
import Agape48

// La klavarligoj de unu kalkulila klavo. Fasona ero 10.
//
// Malfermata per dekstra klako sur klavo de la vizaĝo, aŭ per simpla klako dum
// la reĝimo "Customize keyboard…" de la ⋮-menuo estas ŝaltita. La gesto ŝanĝiĝis
// de Stir+dekstra klako al Stir+klako je 2026aug30, samtempe kun fari la
// ⋮-ikonon la sola vojo al la menuo, kaj poste al simpla dekstra klako je
// 2026sep10, kiam raporto montris ke la Stir-klavo interbatalas kun la kalkulilo.
//
// La redaktoj aplikiĝas dum ili estas faritaj kaj la listo estas la vero, do
// ekzistas nenia Konservi kiu povus malakordi kun ĝi. La origina skizo havis
// tian, sed ĝia tasko estis esti grizigita dum konflikto, kaj ero 10a
// anstataŭigis blokadon per ŝtelado.
//
// La kapto forglutas ĉiun klavon krom Esk, kaj tial Take it / Cancel restas
// atingeblaj per la muso: Esk apartenas al la dialogo, do ĝi neniam povas esti
// kaptita, kaj tio estas ankaŭ la regulo kiu tenas Esk konstante ŜALTITA.
Window {
    id: root
    required property Agape48Engine engine

    property string keyId: ""
    property string keyLabel: ""
    property bool   capturing: false
    property string pendingId: ""
    property string pendingOwner: ""
    property string refused: ""
    property var    rows: []

    readonly property bool opened: visible

    function openFor(k) {
        keyId = k.key
        keyLabel = k.label && k.label !== k.key ? k.label : k.key
        capturing = false
        pendingId = ""
        refused = ""
        // Remalfermo antaŭe lasis konfliktvicon de la antaŭa fojo sur la
        // ekrano, kaj ĝia Take it tiam ligis klavon 0 - ligo por klavo kiu ne
        // ekzistas. Trovita per stiri la dialogon dufoje sinsekve.
        pendingOwner = ""
        refresh()
        if (transientParent) {
            x = transientParent.x + (transientParent.width - width) / 2
            y = transientParent.y + 90
            engine.keepOnScreen(root)
        }
        show(); raise(); requestActivate()
    }
    function refresh() { rows = Agape48Keymap.bindingsFor(keyId) }

    // La klavmapo parolas internajn nomojn - "N8", "SHL". La vizaĝo parolas
    // presitajn etikedojn - "8", la violan sagon. La uzanto vidis nur la duan,
    // do la konfliktlinio devas traduki anstataŭ diri "8 is currently N8".
    function labelOf(name) {
        const keys = root.engine.skin.keys
        for (let i = 0; i < keys.length; ++i)
            if (keys[i].key === name)
                return keys[i].label || name
        return name
    }

    function accept(key, mods, text) {
        // Fn sur ThinkPad alvenas kiel Qt.Key_WakeUp kaj antaŭe estis
        // registrata kiel ligo kiun la kalkulilo neniam povus revidi - dogfood
        // #8, linio 8.
        const id = Agape48Keymap.idFor(key, mods, text)
        if (!Agape48Keymap.isBindableId(id)) {
            refused = (key === Qt.Key_Escape)
                      ? qsTr("Esc is always ON and cannot be given to another key.")
                      : qsTr("That key cannot be used. The system keeps it for itself.")
            return
        }
        refused = ""
        const owner = Agape48Keymap.ownerOfId(id)
        if (owner === root.keyId) {          // jam nia, nenio por fari
            capturing = false
            pendingId = ""
            return
        }
        pendingId = id
        if (owner === "") {                  // libera: prenu ĝin tuj
            Agape48Keymap.bindId(id, root.keyId)
            capturing = false
            pendingId = ""
            refresh()
            return
        }
        // Prenita. Montru kiu havas ĝin kaj proponu la ŝtelon - ero 10a elektis
        // ŝteladon super blokado, ĉar blokado signifas tri dialogojn por unu
        // ŝanĝo.
        pendingOwner = owner
        capturing = false
    }

    function steal() {
        if (root.pendingId === "")
            return
        Agape48Keymap.bindId(root.pendingId, root.keyId)
        capturing = false
        pendingOwner = ""
        refresh()
    }

    title: qsTr("Keyboard for %1").arg(keyLabel)
    flags: Qt.Dialog
    color: "#1b1b1b"
    // Limigita al la ekrano; vidu SettingsWindow.qml por tio, kion telefono
    // faris al dialogo dimensiita por tekokomputilo.
    readonly property real fitW: Screen.desktopAvailableWidth  > 0
                                     ? Screen.desktopAvailableWidth  : 1e6
    readonly property real fitH: Screen.desktopAvailableHeight > 0
                                     ? Screen.desktopAvailableHeight : 1e6
    width: Math.min(440, fitW)
    height: Math.min(360, fitH)
    minimumWidth: Math.min(340, fitW)
    minimumHeight: Math.min(260, fitH)

    // Esk nuligas kapton se iu funkcias, alie fermas la fenestron. Klavtraktilo
    // prefere ol Shortcut, pro la kialo en SettingsWindow.qml: Shortcut en
    // duaranga fenestro plu kaptas post kiam la fenestro malaperis.
    Item {
        anchors.fill: parent
        focus: !root.capturing
        Keys.onEscapePressed: (event) => {
            if (root.capturing) root.capturing = false
            else                root.close()
            event.accepted = true
        }

    Column {
        anchors { fill: parent; margins: 18 }
        spacing: 10

        Label {
            text: qsTr("Keys that press %1").arg(root.keyLabel)
            color: "#f0f0f0"; font.pixelSize: TextSizes.dialogTitle; font.weight: Font.DemiBold
        }

        // --- kio estas ligita nun ---------------------------------------------
        Column {
            width: parent.width
            spacing: 4
            Repeater {
                model: root.rows
                delegate: Row {
                    id: bindingRow
                    required property var modelData
                    spacing: 8
                    Button {
                        text: "−"
                        width: 30
                        enabled: !bindingRow.modelData.locked
                        onClicked: { Agape48Keymap.unbind(bindingRow.modelData.id); root.refresh() }
                    }
                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        text: bindingRow.modelData.locked
                              ? qsTr("%1   (always ON, cannot be changed)").arg(bindingRow.modelData.label)
                              : bindingRow.modelData.label
                        color: bindingRow.modelData.locked ? "#7d7d7d" : "#e8e8e8"
                        font.pixelSize: TextSizes.dialogBody
                    }
                }
            }
            Label {
                visible: root.rows.length === 0
                text: qsTr("No key on the keyboard presses this one.")
                color: "#7d7d7d"; font.pixelSize: TextSizes.dialogHint
            }
        }

        // --- aldonu unu -------------------------------------------------------
        Button {
            text: qsTr("+   Add a key")
            visible: !root.capturing && root.pendingOwner === ""
            onClicked: {
                root.capturing = true; root.pendingId = ""; root.refused = ""
                capture.forceActiveFocus()
            }
        }

        Rectangle {
            width: parent.width
            height: 62
            visible: root.capturing || root.pendingOwner !== ""
            color: "#262626"
            radius: 6
            border.color: (root.pendingOwner !== "" || root.refused !== "")
                          ? "#c86464" : "#3a3a3a"

            Column {
                anchors { fill: parent; margins: 10 }
                spacing: 6
                Label {
                    text: root.refused !== "" ? root.refused
                          : root.capturing
                          ? qsTr("Press the key you want. Esc cancels.")
                          : qsTr("%1 is currently assigned to the %2 key.")
                            .arg(Agape48Keymap.labelForId(root.pendingId))
                            .arg(root.labelOf(root.pendingOwner))
                    color: (root.pendingOwner !== "" || root.refused !== "")
                           ? "#e88a8a" : "#d0d0d0"
                    font.pixelSize: TextSizes.dialogBody
                    width: root.width - 56
                    wrapMode: Text.WordWrap
                }
                Row {
                    spacing: 8
                    visible: root.pendingOwner !== ""
                    Button { text: qsTr("Take it"); onClicked: root.steal() }
                    Button {
                        text: qsTr("Cancel")
                        onClicked: { root.pendingOwner = ""; root.pendingId = "" }
                    }
                }
            }
        }

        Item { width: 1; height: 4 }

        Row {
            spacing: 10
            Button {
                text: qsTr("Reset this key")
                onClicked: { Agape48Keymap.resetKey(root.keyId); root.refresh() }
            }
            Button {
                text: qsTr("Reset all keys")
                onClicked: { Agape48Keymap.resetToDefaults(); root.refresh() }
            }
            Button { text: qsTr("Close"); onClicked: root.close() }
        }
    }

    }

    // La kaptilo. Ĝi devas forgluti ĉion por ke kaptita klavkombino ne ankaŭ
    // stiru la proprajn butonojn de la dialogo; Esk estas traktata de la
    // klavtraktilo supre kaj neniam atingas ĉi tien kiel ligo.
    Item {
        id: capture
        anchors.fill: parent
        enabled: root.capturing
        focus: root.capturing
        Keys.onPressed: (event) => {
            event.accepted = true
            if (event.key === Qt.Key_Escape) {
                root.capturing = false
                return
            }
            if (event.isAutoRepeat)
                return
            root.accept(event.key, event.modifiers, event.text)
        }
    }
}
