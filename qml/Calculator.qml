import QtQuick
import Agape48

// The calculator face: one photographic skin image, the LCD drawn over it, and
// a multi-touch keypad on top. Everything is laid out in skin-image pixel
// coordinates and scaled as a unit, so a skin author works in one coordinate
// system and never thinks about device pixels.
Item {
    id: root
    required property Agape48Engine engine

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
            smooth: true
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

        Repeater {
            model: root.engine.skin.annunciators
            delegate: Item {
                required property var modelData
                x: modelData.rect.x; y: modelData.rect.y
                width: modelData.rect.width; height: modelData.rect.height
                visible: (root.engine.annunciators & modelData.bit) !== 0
                Rectangle { anchors.fill: parent; color: "#101010"; radius: 1 }
            }
        }

        Keypad {
            anchors.fill: parent
            engine: root.engine
        }
    }
}
