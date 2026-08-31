import QtQuick
import Agape48

// The calculator face: one photographic skin image, the LCD drawn over it, and
// a multi-touch keypad on top. Everything is laid out in skin-image pixel
// coordinates and scaled as a unit, so a skin author works in one coordinate
// system and never thinks about device pixels.
Item {
    id: root
    required property Agape48Engine engine

    // Passed through to the keypad, which owns the gesture.
    property alias customizing: keypad.customizing
    signal remapRequested(var keyModel)
    signal customizeCancelled()
    signal unassignedKey(string label)
    signal bodyPressed()

    // Main.qml's focus guard asks and answers through these two.
    function hasKeyboardFocus()  { return keypad.activeFocus }
    function grabKeyboardFocus() { keypad.forceActiveFocus() }

    readonly property size faceSize: engine.skin.faceSize
    readonly property real scaleFactor:
        faceSize.width > 0 && faceSize.height > 0
            ? Math.min(width / faceSize.width, height / faceSize.height)
            : 1

    Item {
        id: face
        width: root.faceSize.width
        height: root.faceSize.height
        anchors.centerIn: parent
        scale: root.scaleFactor
        transformOrigin: Item.Center

        Image {
            anchors.fill: parent
            source: root.engine.skin.faceImage
            // The skin is a WebP photograph at its native size; let the GPU do
            // the scaling and keep only one decoded copy in memory.
            sourceSize: Qt.size(root.faceSize.width, root.faceSize.height)
            // The face is nearly always being scaled DOWN, and a downscaled
            // photograph aliases: the key legends sparkle and the whole face
            // shimmers when the window is small. Mipmaps are what fixes that -
            // smooth alone is bilinear on the full-size texture and does not.
            // Costs about a third more texture memory and nothing on disk.
            // (The LCD is the opposite case and stays nearest-neighbour; see
            // LcdItem.cpp.)
            smooth: true
            mipmap: true
            asynchronous: true
            cache: true
        }

        LcdItem {
            engine: root.engine
            x: root.engine.skin.lcdRect.x
            y: root.engine.skin.lcdRect.y
            width: root.engine.skin.lcdRect.width
            height: root.engine.skin.lcdRect.height
            pixelColor: root.engine.skin.lcdPixelColor
            backgroundColor: root.engine.skin.lcdBackground
        }

        // The annunciators, inside the glass and above the dot matrix, where
        // they are on the machine. They were a #101010 rectangle on a #121214
        // bezel until dogfood #13 line 12 - lighting up perfectly, invisibly.
        // The bits were never the problem.
        Repeater {
            model: root.engine.skin.annunciators
            delegate: Image {
                required property var modelData
                x: modelData.rect.x; y: modelData.rect.y
                width: modelData.rect.width; height: modelData.rect.height
                visible: (root.engine.annunciators & modelData.bit) !== 0
                source: modelData.image
                sourceSize: Qt.size(modelData.rect.width, modelData.rect.height)
                smooth: true
                mipmap: true
                cache: true
            }
        }

        Keypad {
            id: keypad
            anchors.fill: parent
            engine: root.engine
            onRemapRequested: (k) => root.remapRequested(k)
            onCustomizeCancelled: root.customizeCancelled()
            onUnassignedKey: (label) => root.unassignedKey(label)
            onBodyPressed: root.bodyPressed()
        }
    }
}
