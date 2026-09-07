import QtQuick
import QtQuick.Controls
import Agape48

// The settings as a PAGE, for the platform that has only one window.
//
// Android does not have windows in the sense a desktop does: an app gets one
// surface and everything that looks like a dialog is drawn inside it. Asking Qt
// for a second top-level Window makes the platform plugin go and acquire a
// native surface, and that call takes a process-wide lock which Qt's own
// accessibility bridge holds whenever a screen reader - or AnyDesk - is
// running. Qt then aborts rather than deadlock. Measured on Gert's phone,
// 2026sep07: Settings 0 opened out of 10, "Open another calculator" 0 out of 3,
// and the Vulkan build says vkSurface() where the OpenGL one says eglSurface(),
// so it is the surface and not the renderer.
//
// Gert, before the stack trace had said it: "What could be crashing is we
// trying to use some desktop feature that doesn't exist in Android" - and then
// "What if we use android-settings style pages, instead of drawing
// windows-style? Maybe they are usually like this for a reason." They are, and
// this is the reason.
//
// A Popup is drawn inside the scene that already exists, so it never asks for a
// surface and never touches that lock. The evidence was already on the phone
// before this file existed: the ⋮ menu is a Controls popup and has opened
// dozens of times without one abort, while every Window has aborted.
//
// IN-SCENE IS NOT THE SAME AS A SHEET. This fills the window and paints its own
// background, so it is a page - not the bottom sheet lying over the face that
// this project deliberately left behind on 2026aug29, and not something the
// calculator shows through.
Popup {
    id: root
    required property Agape48Engine engine

    // Pinned rather than left to the default. A plain Popup already resolves to
    // Item - measured on Linux with qt.quick.controls.popup logging: 42 popup
    // lines and not one popup window, on a platform that supports popup windows
    // and would have made one if that were the default - but the whole fix
    // depends on this one property, so it is written down instead of inherited.
    // Qt 6.8 added it, which is older than the SafeArea attached property the
    // calculator already needs, so it costs no supported version.
    popupType: Popup.Item

    modal: true
    // No shading behind it: it covers the whole window, so there is nothing
    // left to shade and the dim layer would only cost a blend of the full
    // screen every frame.
    dim: false
    padding: 0
    // Every exit goes through requestClose(), including the back gesture, so
    // the unsaved-path question cannot be walked around by leaving sideways.
    closePolicy: Popup.NoAutoClose
    focus: true

    background: Rectangle { color: "#1b1b1b" }

    // Android's back gesture arrives as a key, and the manifest already opts in
    // to the modern callback. Escape is handled inside the contents, where it
    // cannot leak out to the calculator - see SettingsContent.qml.
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Back) {
            content.requestClose()
            event.accepted = true
        }
    }

    // A phone page says where it is and how to leave, in the corner every
    // Android app puts it in.
    Item {
        id: header
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: 52

        Item {
            id: backButton
            width: 52; height: 52

            // Drawn, not a glyph. "←" is a thin hairline in most of the fonts a
            // phone actually has, and the same reasoning that gave the house
            // icon in SettingsContent a Canvas applies here.
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
            TapHandler { onTapped: content.requestClose() }
        }

        Text {
            anchors { left: backButton.right; verticalCenter: parent.verticalCenter }
            text: qsTr("Settings")
            color: "#e8e8e8"
            font.pixelSize: TextSizes.dialogTitle
        }

        Rectangle {
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 1
            color: "#3a3a3a"
        }
    }

    SettingsContent {
        id: content
        anchors { fill: parent; topMargin: header.height }
        engine: root.engine
        onCloseRequested: root.close()
        // Advanced is still a Window, so it is still fatal here. The button
        // that would have opened it is hidden on Android in SettingsContent
        // rather than left to abort, and this handler is what will be wired
        // when AdvancedWindow gets the same shell treatment as this one.
        onAdvancedRequested: {}
    }
}
