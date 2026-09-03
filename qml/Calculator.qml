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

    // The LCD's vertical centre in THIS item's coordinates, so anything that has
    // to sit over the calculator's screen can be put there without knowing how
    // the face is scaled. Exposed rather than anchored to because QML anchors
    // only reach parents and siblings, and the LCD is neither from outside here.
    // The face is centred and scaled about its own centre, so a point maps
    // through that centre rather than through the origin.
    readonly property real lcdCenterY:
        face.y + face.height / 2
        + (engine.skin.lcdRect.y + engine.skin.lcdRect.height / 2
           - face.height / 2) * scaleFactor

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

        // A window that no longer holds its calculator goes on showing the last
        // frame it drew, which reads as a freeze rather than as sleep. Dogfood
        // both-03 line 16: "the other screen should blank instead of freezing",
        // and line 26: "the 'sleeping' calculator failed to blank its screen, so
        // I doubted it was really sleeping. I tested the keyboard, and the keys
        // are dead, as they should be in a sleeping machine." The keys were
        // right and only the glass was lying. Wording is his.
        //
        // The LCD's own background rather than black: a blank HP 48 screen is
        // pale green, and a black hole in the bezel looks like a fault.
        Rectangle {
            x: root.engine.skin.lcdRect.x
            y: root.engine.skin.lcdRect.y
            width: root.engine.skin.lcdRect.width
            height: root.engine.skin.lcdRect.height
            visible: root.engine.detached
            color: root.engine.skin.lcdBackground

            Text {
                // In the UPPER part of the glass, not centred on it. Once the
                // text went to 4x it collided with the error banner, which
                // both-04's other note puts across the centre of the screen -
                // the banner covered the first of the two lines. An HP 48 puts
                // its own status messages at the top of the display, so this is
                // the machine's own idiom rather than a compromise, and a
                // transient banner underneath it now reads as a second line of
                // explanation instead of a lid.
                anchors {
                    left: parent.left; right: parent.right; top: parent.top
                    margins: 6
                }
                // 0.36, not 0.45: at 0.45 the banner's top edge clipped the
                // descenders of the second line. Measured off a capture.
                height: parent.height * 0.36
                verticalAlignment: Text.AlignTop
                // Broken across two lines deliberately, and four times the size
                // it was. Gert, both-04 line 7: "the text is way too small. It
                // should be 4x as large, divided in two lines." It was small
                // because Text.Fit only ever SHRINKS - pixelSize is its ceiling,
                // not its target - and the ceiling was 13. The default skin's
                // LCD is 786x384 face pixels, so 52 fits with room to spare.
                text: qsTr("The chosen memory is in use\nby another device.")
                color: root.engine.skin.lcdPixelColor
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                // Fit stays as the safety net for a skin whose glass is smaller
                // than this one's, rather than eliding the sentence that
                // explains why the machine is not responding.
                fontSizeMode: Text.Fit
                font.pixelSize: 52
                minimumPixelSize: 12
            }
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
                // …and dark while the machine is asleep, or the last frame's
                // annunciators go on burning over a blanked screen.
                visible: !root.engine.detached
                         && (root.engine.annunciators & modelData.bit) !== 0
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
