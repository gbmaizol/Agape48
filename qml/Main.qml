import QtQuick
import Agape48

Window {
    id: root
    visible: true
    // Portrait, sized from the skin so the face is never scaled up past 1:1 on
    // desktop. On Android the Window fills the screen and Calculator letterboxes.
    width: Math.min(engine.skin.faceSize.width || 480, Screen.desktopAvailableWidth * 0.5)
    height: width * ((engine.skin.faceSize.height || 900) / (engine.skin.faceSize.width || 480))
    color: "#101010"
    title: qsTr("Agape48")

    Agape48Engine {
        id: engine
        onRomRequired: settings.open()
        onBeep: (hz, ms) => feedback.beep(hz, ms)
        onKeyFeedback: (keyId) => feedback.tap()
        onLastErrorChanged: if (lastError) banner.show(lastError)
    }

    Calculator {
        id: calculator
        anchors.fill: parent
        engine: engine
    }

    SettingsSheet {
        id: settings
        anchors.fill: parent
        engine: engine
    }

    // Error banner. Raw QtQuick - see README: Qt Quick Controls is deliberately
    // not a dependency, so every widget here is a Rectangle and a Text.
    Rectangle {
        id: banner
        function show(msg) { text.text = msg; opacity = 1; hideTimer.restart() }
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: text.implicitHeight + 24
        color: "#8c1d18"
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 180 } }
        Timer { id: hideTimer; interval: 6000; onTriggered: banner.opacity = 0 }
        Text {
            id: text
            anchors { fill: parent; margins: 12 }
            color: "white"; wrapMode: Text.WordWrap; font.pixelSize: 13
        }
        MouseArea { anchors.fill: parent; onClicked: banner.opacity = 0 }
    }

    // Haptics and beeps. Both are platform calls, not Qt Multimedia - see
    // README "Sound and haptics" for why QSoundEffect is not an option here.
    // TODO: add src/bridge/Feedback.{h,cpp} as a QML_SINGLETON wrapping
    //   Android : Vibrator / VibrationEffect + AudioTrack, via QJniObject
    //   Linux   : libcanberra-less ALSA square wave, or nothing
    //   Windows : Beep() on a worker, or WASAPI for a real waveform
    QtObject {
        id: feedback
        function tap()        { if (engine.hapticsEnabled) { /* Feedback.vibrate(12) */ } }
        function beep(hz, ms) { if (engine.soundEnabled)   { /* Feedback.tone(hz, ms) */ } }
    }

    Component.onCompleted: engine.start()

    onActiveChanged: {
        if (active) engine.resumeFromBackground()
        else engine.suspend()
    }
}
