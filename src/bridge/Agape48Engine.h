// ---------------------------------------------------------------------------
// Agape48Engine - the QML-facing facade over the x48 core.
//
// Owns the emulation tick, the key matrix translation, and the wiring to the
// state-file and skin objects. Holds no emulator state of its own: everything
// authoritative lives behind x48_shim.h.
// ---------------------------------------------------------------------------
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include "x48_shim.h"

// Included, not forward-declared: moc needs a complete type to build a
// QMetaType for a pointer parameter, the same rule that caught SkinModel and
// StateFileManager in the very first build. "Pointer Meta Types must either
// point to fully-defined types."
#include <QQuickWindow>

// Included rather than forward-declared: both appear as pointer Q_PROPERTYs
// below, and moc needs a complete type to build a QMetaType for a pointer
// property ("Pointer Meta Types must either point to fully-defined types...").
#include "SkinModel.h"
#include "StateFileManager.h"

class Agape48Engine : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(bool running        READ isRunning     NOTIFY runningChanged)
    Q_PROPERTY(bool ready          READ isReady       NOTIFY readyChanged)
    Q_PROPERTY(QUrl romSource      READ romSource     WRITE setRomSource  NOTIFY romSourceChanged)
    Q_PROPERTY(int  contrast       READ contrast      NOTIFY frameReady)
    Q_PROPERTY(int  annunciators   READ annunciators  NOTIFY annunciatorsChanged)
    Q_PROPERTY(QString lastError   READ lastError     NOTIFY lastErrorChanged)

    // Names of the keys currently held down, whatever pressed them - mouse,
    // finger or the physical keyboard. Keypad.qml lights its caps off this.
    // It has to live here rather than in QML: QML only ever saw the touch
    // points, so a key pressed from the keyboard lit nothing.
    Q_PROPERTY(QStringList pressedKeys READ pressedKeys NOTIFY pressedKeysChanged)

    // Aggregated sub-objects, so QML reaches everything through one root:
    //   engine.state.location, engine.skin.keys, ...
    Q_PROPERTY(StateFileManager *state READ state CONSTANT)
    Q_PROPERTY(SkinModel        *skin  READ skin  CONSTANT)

    // User preferences that live on the frontend side, not in the core.
    Q_PROPERTY(bool hapticsEnabled READ hapticsEnabled WRITE setHapticsEnabled NOTIFY hapticsEnabledChanged)
    Q_PROPERTY(bool soundEnabled   READ soundEnabled   WRITE setSoundEnabled   NOTIFY soundEnabledChanged)

    // Off by default. On, every qDebug/qWarning and the startup facts that
    // explain a failed start go to logPath, which is in app storage and NOT in
    // the state folder - a log has no business syncing between machines.
    Q_PROPERTY(bool    debugLogging READ debugLogging WRITE setDebugLogging NOTIFY debugLoggingChanged)

    // Off by default: a resize drag shows an outline and the window changes
    // shape once, when you let go. On, the window follows the pointer the whole
    // way. The outline is the default because a window that changes shape
    // mid-drag is repainted by the OS a frame ahead of us, and on the left and
    // top edges that lands the face at a shifted position - dogfood windows-02.
    // Machine-local, like the window geometry and the keymap.
    Q_PROPERTY(bool liveResize READ liveResize WRITE setLiveResize NOTIFY liveResizeChanged)
    Q_PROPERTY(QString logPath      READ logPath      CONSTANT)

public:
    explicit Agape48Engine(QObject *parent = nullptr);
    ~Agape48Engine() override;

    bool isReady() const     { return m_ready; }
    bool isRunning() const   { return m_tick.isActive(); }
    QUrl romSource() const   { return m_romSource; }
    int  contrast() const    { return m_frame.contrast; }
    int  annunciators() const{ return m_annunciators; }
    QString lastError() const{ return m_lastError; }
    QStringList pressedKeys() const { return m_pressed; }
    StateFileManager *state() const { return m_state; }
    SkinModel        *skin()  const { return m_skin; }
    bool hapticsEnabled() const { return m_haptics; }
    bool soundEnabled() const   { return m_sound; }
    bool debugLogging() const   { return m_debugLogging; }
    bool liveResize() const     { return m_liveResize; }
    QString logPath() const;

    void setRomSource(const QUrl &url);
    void setHapticsEnabled(bool on);
    void setSoundEnabled(bool on);
    void setDebugLogging(bool on);
    void setLiveResize(bool on);

    // The live modifier state, not an event's cached copy. A mouse press on the
    // face arrives through MultiPointTouchArea, whose touch points carry no
    // modifiers at all, so Ctrl+click has to ask the platform directly.
    Q_INVOKABLE int keyboardModifiers() const;

    // A frameless window has no title bar for the window manager to drag, so
    // the app asks for the drag itself. This hands the interaction to the WM,
    // which is what makes it behave like a real title bar - snapping, workspace
    // edges, the lot - instead of a hand-rolled x/y chase.
    //
    // There is deliberately no startSystemResize twin. The WM's own resize is
    // interactive and ignores the height the aspect lock sets during the drag,
    // so the window came out the wrong shape; Main.qml drives the resize itself
    // and keeps the proportions exact on every frame.
    // QML cannot reach QUrl::fromLocalFile()/toLocalFile(), so SettingsWindow
    // hand-rolled both with string surgery. That breaks the moment there is a
    // drive letter: toString().mid(7) on file:///C:/x leaves "/C:/x", a leading
    // slash Windows does not want, and building a URL by pasting "file:///" in
    // front of C:\Users\… produces backslashes no URL may contain. Qt's own
    // conversions already know about drive letters, UNC paths and
    // percent-encoding. Flagged from the Windows laptop, 2026aug31.
    Q_INVOKABLE QUrl    pathToUrl(const QString &path) const;
    Q_INVOKABLE QString urlToPath(const QUrl &url) const;

    Q_INVOKABLE bool startSystemMove(QQuickWindow *window);

    // One XMoveResizeWindow for a resize drag. QML cannot reach QWindow's own
    // four-argument setGeometry, and Main.qml already has a helper of that name
    // with a different signature, so the call comes through here.
    Q_INVOKABLE void setWindowGeometry(QQuickWindow *window, int x, int y, int w, int h);

    // --- called by LcdItem, not exposed to QML -----------------------------
    // Valid until the next tick; LcdItem copies out of it inside
    // updatePaintNode(), which runs while the GUI thread is blocked.
    const x48_frame_t &frame() const { return m_frame; }
    quint64 frameSerial() const { return m_frameSerial; }

public slots:
    // --- lifecycle ---------------------------------------------------------
    bool start();          // init core (if needed) + begin ticking
    void stop();           // pause ticking; does NOT save
    void suspend();        // stop + save + release all keys (app going background)
    void resumeFromBackground();

    // --- keys --------------------------------------------------------------
    // Name form is what skins use ("ENTER", "SIN", "N7", "ON"); the numeric
    // form lets a skin address the matrix directly. Both land on the same write.
    void pressKey(const QString &keyId);
    void releaseKey(const QString &keyId);
    void pressCode(int row, int mask);
    void releaseCode(int row, int mask);
    void releaseAllKeys();


    // --- state -------------------------------------------------------------
    void reset(bool cold = false);
    bool saveState();
    bool reloadState();

    // --- clipboard ---------------------------------------------------------
    bool copyStackToClipboard();
    bool pasteClipboardToStack();

signals:
    void runningChanged();
    void readyChanged();
    void romSourceChanged();
    void annunciatorsChanged();
    void lastErrorChanged();
    void pressedKeysChanged();
    void hapticsEnabledChanged();
    void soundEnabledChanged();
    void debugLoggingChanged();
    void liveResizeChanged();

    void frameReady();                      // LcdItem listens; fires only on change
    void beep(int frequencyHz, int durationMs);
    void keyFeedback(const QString &keyId); // QML plays haptics/sound off this
    void romRequired();                     // no ROM yet - QML shows the picker
    void stateFolderBusy();                 // another instance has it - same

private:
    void tick();
    void setError(const QString &what);
    bool lookupKey(const QString &keyId, int *row, int *mask) const;
    void markPressed(int row, int mask, bool down);
    void finishRelease(int row, int mask);
    void setTickRate(int ms);
    void logStartupFacts() const;

    QTimer            m_tick;
    x48_frame_t       m_frame {};
    quint64           m_frameSerial = 0;
    int               m_annunciators = 0;
    bool              m_ready = false;
    bool              m_haptics = true;
    bool              m_sound = true;
    bool              m_debugLogging = false;
    bool              m_liveResize    = false;
    bool              m_displayOff = false;

    // A key has to stay down long enough for the ROM's keyboard scan to see
    // it. A tap shorter than that was simply lost - and worst of all when the
    // calculator was off, because then the scan only starts once the key is
    // already down. Releases are held back to a minimum time; see releaseCode.
    QElapsedTimer     m_clock;
    QHash<int, qint64> m_downAt;
    QSet<int>         m_releasePending;
    QUrl              m_romSource;
    QString           m_lastError;
    QStringList       m_pressed;
    StateFileManager *m_state = nullptr;
    SkinModel        *m_skin  = nullptr;
};
