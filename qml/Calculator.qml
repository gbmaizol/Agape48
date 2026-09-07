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

    // ANDROID FILLS THE SCREEN, the desktops keep the face's proportions. Gert,
    // 2026sep07: "I prefer that only for Android it does the same as Droid48
    // does. Keep the desktop versions as they are."
    //
    // The face is 854x1438, which is 0.594 wide for its height; his phone is
    // 0.450. Fitted by the smaller of the two ratios - which is what every
    // other platform does - the calculator ends at 75% of the screen's height
    // and the rest is background. Droid48 stretches instead, and that is now
    // what happens here: the two axes get their own scale, so the keypad
    // reaches the bottom of the phone and the LCD's dots come out 32% taller
    // than they are wide on this particular screen.
    //
    // One transform for the whole face, so every key, annunciator and hit area
    // follows it without knowing about any of this - the same property the
    // uniform version relied on.
    readonly property bool fillScreen: Qt.platform.os === "android"
    readonly property real scaleX:
        faceSize.width > 0 ? (fillScreen ? width / faceSize.width : scaleFactor) : 1
    readonly property real scaleY:
        faceSize.height > 0 ? (fillScreen ? height / faceSize.height : scaleFactor) : 1

    // The LCD's vertical centre in THIS item's coordinates, so anything that has
    // to sit over the calculator's screen can be put there without knowing how
    // the face is scaled. Exposed rather than anchored to because QML anchors
    // only reach parents and siblings, and the LCD is neither from outside here.
    // The face is centred and scaled about its own centre, so a point maps
    // through that centre rather than through the origin.
    readonly property real lcdCenterY:
        face.y + face.height / 2
        + (engine.skin.lcdRect.y + engine.skin.lcdRect.height / 2
           - face.height / 2) * scaleY

    Item {
        id: face
        width: root.faceSize.width
        height: root.faceSize.height
        anchors.centerIn: parent
        // A Scale transform rather than the scale property, because scale is
        // one number and this needs two. Identical to the old behaviour
        // wherever scaleX and scaleY are the same, which is every desktop.
        transform: Scale {
            origin.x: face.width / 2
            origin.y: face.height / 2
            xScale: root.scaleX
            yScale: root.scaleY
        }

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

        // The open calculator's name, printed between the two words the skin
        // has already printed on itself. Gert, both-05 line 37: "I'd like the
        // first 20 letters of the name of the current calculator to be shown
        // between the 'HEWLETT-PACKARD' and the '48GX' at the top, same font,
        // center-aligned in the middle." Then both-06 line 20, having seen it:
        // "Looks great! Increate the limit to 30!"
        //
        // Everything about it - where, how big, what colour, how many letters,
        // which font - comes from the skin, because tools/makeface.py is what
        // lettered the two words either side of it and is the only thing that
        // knows what it did. A skin that omits the block gets no nameplate and
        // no error. Inside `face`, so it scales with everything else.
        //
        // Deliberately NOT hidden while the calculator is asleep: a blank green
        // screen is exactly when "which calculator is this window on" is worth
        // being able to read.
        Text {
            readonly property var plate: root.engine.skin.nameplate
            readonly property rect box: plate && plate.rect ? plate.rect
                                                            : Qt.rect(0, 0, 0, 0)
            x: box.x; y: box.y
            width: box.width; height: box.height
            visible: box.width > 0 && text.length > 0
            text: (root.engine.state.instance || "")
                      .slice(0, plate && plate.maxChars ? plate.maxChars : 30)
            color: plate && plate.color ? plate.color : "#c6aa60"
            // The skin names the face's font and then its nearest substitutes,
            // because the face is lettered in DejaVu Sans Condensed and a stock
            // Windows has none of it. QML's font value type only takes ONE
            // family - `families` is a C++ QFont property and does not exist
            // here - so the list is resolved against what is actually installed
            // and the first hit wins. Empty means "whatever Qt would have
            // chosen", which is the right answer when none of them is present.
            readonly property string faceFont: {
                const want = plate && plate.font ? plate.font : []
                const have = Qt.fontFamilies()
                for (let i = 0; i < want.length; ++i)
                    if (have.indexOf(want[i]) >= 0)
                        return want[i]
                return ""
            }
            font.family: faceFont
            font.bold: plate ? plate.bold === true : true
            font.pixelSize: plate && plate.pixelSize ? plate.pixelSize : 17
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            // Thirty letters, and all thirty of them shown. The band is 445 px
            // and thirty letters of a name anyone would type measure about 275,
            // but thirty capital Ws measure 506 - and on Windows, where none of
            // the condensed families exist and the fallback is wider, the
            // margin is thinner still. Fit only ever SHRINKS, so an ordinary
            // name is untouched at 17 and only a freakishly wide one gives up
            // any size. Cutting letters he asked to see would be the wrong way
            // round.
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: 12
            // Below 12 px it would be unreadable anyway, so the last resort is
            // still to cut. A skin with a narrower band than this one's is the
            // only thing that reaches it.
            elide: Text.ElideRight
        }

        LcdItem {
            engine: root.engine
            x: root.engine.skin.lcdRect.x
            y: root.engine.skin.lcdRect.y
            width: root.engine.skin.lcdRect.width
            height: root.engine.skin.lcdRect.height
            pixelColor: root.engine.skin.lcdPixelColor
            backgroundColor: root.engine.skin.lcdBackground

            // Free resizing puts a fractional number of device pixels on every
            // HP 48 dot, and nearest-neighbour then rounds each dot's two edges
            // on its own account: at two and a half pixels to the dot, one stem
            // of an H comes out two pixels wide and the other three. Gert,
            // 2026sep04: "this looks like very poor rendering." The dots being
            // small is not the complaint - the dots being DIFFERENT SIZES is.
            //
            // So enlarge in two steps instead of one. Draw the matrix into a
            // layer at a whole number of pixels per dot - the smallest whole
            // number that is still big enough - and let the final draw shrink
            // that by the fraction left over. Step one is exact, so no dot is
            // favoured over its neighbour; step two is never more than 2:1, so
            // it only ever softens a dot's edge and never reaches its middle.
            // Every stem then weighs the same, which is the thing the eye was
            // objecting to.
            //
            // The GPU does both from the same 8.4 KB upload the item already
            // sends: one extra pass the size of the glass, on frames that
            // changed. Nothing here scales with the window.
            readonly property int cols: root.engine.skin.lcdZoom > 0
                ? Math.round(width  / root.engine.skin.lcdZoom) : 0
            readonly property int rows: root.engine.skin.lcdZoom > 0
                ? Math.round(height / root.engine.skin.lcdZoom) : 0
            // The small tolerance keeps a scale that is already whole from
            // being rounded up to the next one and then shrunk back by 6/7 for
            // no reason: at exactly 6 device pixels to the dot this is 6, the
            // second step is 1:1, and the result is what it is today.
            // The larger of the two axes, so a stretched screen is drawn at
            // the finer of its two pitches and then squeezed, never blown up.
            readonly property int perDot: Math.max(1, Math.ceil(
                root.engine.skin.lcdZoom * Math.max(root.scaleX, root.scaleY)
                    * Screen.devicePixelRatio - 0.02))
            layer.enabled: cols > 0 && rows > 0
            layer.smooth: true
            layer.textureSize: Qt.size(cols * perDot, rows * perDot)
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
                // Only when somebody else really is holding the memory. The
                // rectangle above blanks the glass whenever this window is not
                // driving the machine, which is right; this sentence used to
                // follow it, which was not. Gert, 2026sep04: "it seems to be
                // showing even when no other calculator is even running to use
                // the memory" - and it was, because a calculator that was saved
                // switched off, or that you switched off yourself, releases the
                // lock and comes up detached with nobody else in the picture.
                // detached says WE do not hold it; this says SOMEBODY ELSE
                // does, and only the second one is what the words claim.
                //
                // A lock left behind by a machine that died stays on disk and
                // still counts, which is his instruction: "If another host died
                // and left the lock on, keep showing the locked screen as is
                // with the phrase on it. Otherwise screen blank as normal."
                visible: root.engine.memoryHeldElsewhere
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
                font.pixelSize: TextSizes.screenMessage
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
