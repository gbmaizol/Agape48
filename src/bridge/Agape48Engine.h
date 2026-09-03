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
#include <QDateTime>
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

    // Counts down while a sleep request is outstanding; 0 when nothing is.
    Q_PROPERTY(int waitSeconds READ waitSeconds NOTIFY waitSecondsChanged)

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

    // --- asking another instance for a calculator ---------------------------
    // Writes the request, then waits for whoever has it to save and let go.
    // takeWhenFree false is "go to sleep, I do not want it now" - the other
    // machine stops holding it and nobody opens it.
    //
    // The wait has to be generous and it has to end: the request travels
    // through the user's own sync folder, so a busy Dropbox can take a minute,
    // and a machine that is switched off will never answer at all. When it
    // does not, the dialog falls back to taking it over, which is what that
    // button was always for.
    Q_INVOKABLE void askForCalculator(const QString &instance, bool takeWhenFree = true);
    Q_INVOKABLE void stopWaiting();
    Q_INVOKABLE bool waitingForCalculator() const { return m_wait.isActive(); }
    // Seconds left before the ask is written off, for the dialog to count down.
    // Gert asked for the number to be on screen: a wait with no end in sight is
    // the difference between "it is working" and "it has hung".
    int waitSeconds() const { return m_waitSeconds; }

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
    // Object interchange, the HPHP48- binary format. Import pushes onto stack
    // level 1; export writes whatever is on level 1, whatever type it is.
    // Asked before the export dialog opens: refusing an empty stack after the
    // user has already chosen a filename is the wrong order - dogfood #15 line 6.
    // Switching calculators. Order is the whole of it: the running one has to
    // be saved and torn down BEFORE the state manager points somewhere else,
    // or its memory is written into the folder of the calculator you asked for.
    // The handover. OFF hands the calculator back - it saves, lets go of the
    // lock, and the window sits detached until ON. ON claims it again and
    // reloads from disk, so whatever another machine did in between is what
    // you get. Gert's design, 2026aug31.
    Q_PROPERTY(bool detached READ isDetached NOTIFY detachedChanged)
    bool isDetached() const { return m_detached; }
    Q_INVOKABLE bool attach(bool takeOver = false);

    Q_INVOKABLE bool    openCalculator(const QString &name, bool takeOver = false);
    // The unanswered case, in one entry point rather than three in QML. Which
    // of the three it is depends on why this window does not have the
    // calculator, and the dialog has no business knowing that: never started
    // (it was busy when the window opened), started and handed over, or simply
    // somebody else's.
    Q_INVOKABLE bool    takeOverCalculator(const QString &name);
    Q_INVOKABLE QString newCalculator();

    Q_INVOKABLE bool hasStackObject() const;
    // The banner times out but lastError did not, so a failed import was still
    // sitting in the settings window minutes later - dogfood #15 line 20.
    Q_INVOKABLE void clearError();

    Q_INVOKABLE bool importFile(const QUrl &url);
    Q_INVOKABLE bool exportFile(const QUrl &url);

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
    void waitingChanged();
    void waitSecondsChanged();
    // Something worth saying that is not a fault. The banner has a quieter
    // style for these; lastError is red and stays up four times as long.
    void notice(const QString &text);
    void otherLetGo(const QString &instance);
    void sleepUnanswered(const QString &instance, const QString &host);
    void beep(int frequencyHz, int durationMs);
    void keyFeedback(const QString &keyId); // QML plays haptics/sound off this
    void detachedChanged();
    // Could not take the calculator back: QML shows who has it and what can be
    // done about it. The map is StateFileManager::lockHolder().
    void attachRefused(const QVariantMap &holder);

    void romRequired();                     // no ROM yet - QML shows the picker

private:
    void tick();
    void setError(const QString &what);
    void pollForRelease();
    bool lookupKey(const QString &keyId, int *row, int *mask) const;
    void markPressed(int row, int mask, bool down);
    void finishRelease(int row, int mask);
    void setTickRate(int ms);
    void queueTaps(const QStringList &keys);
    void shutdownCore();
    // Save, release, detach: the single way a calculator leaves this window,
    // whether the user switched it off or another machine asked for it.
    void handOver();

    // ...and the other direction. A calculator we ASKED for arrives switched
    // off, because handing one over is switching it off. Somebody has to press
    // ON, and it should not be the person who just waited for it.
    void wakeAcquired();
    void logStartupFacts() const;

    QTimer            m_tick;
    // Polled rather than watched: the calculator being waited for is often not
    // the one this window has open, so its folder is not the one the watcher is
    // pointed at. One stat a second for at most a minute and a half.
    QTimer            m_wait;
    QString           m_waitFor;
    QString           m_waitHost;
    bool              m_waitTake = true;
    QDateTime         m_waitUntil;
    int               m_waitSeconds = 0;
    x48_frame_t       m_frame {};
    quint64           m_frameSerial = 0;
    int               m_annunciators = 0;
    bool              m_ready = false;
    bool              m_haptics = true;
    bool              m_sound = true;
    bool              m_debugLogging = false;
    bool              m_liveResize    = false;
    bool              m_displayOff = false;
    bool              m_detached = false;
    bool              m_sawFirstFrame = false;
    // Set by start(), spent by the first frame after it. That frame shows the
    // calculator as it was SAVED, so it must never be read as the user having
    // just switched the machine off - see tick().
    bool              m_freshLoad = false;
    // Who asked for this calculator, held from the moment the request arrives
    // until the machine has actually switched itself off and been handed over.
    // Empty at every other time, so it doubles as "a hand-over is in progress".
    QString           m_sleepFor;
    qint64            m_sleepAskedAt = 0;
    // Digest of the RAM at the last save this process actually performed, so a
    // save that would write identical bytes can be skipped. Set only by a real
    // write; 0 means "unknown, write anyway". See saveState().
    quint64           m_savedRamDigest = 0;

    // A key has to stay down long enough for the ROM's keyboard scan to see
    // it. A tap shorter than that was simply lost - and worst of all when the
    // calculator was off, because then the scan only starts once the key is
    // already down. Releases are held back to a minimum time; see releaseCode.
    QElapsedTimer     m_clock;
    QHash<int, qint64> m_downAt;
    QSet<int>         m_releasePending;
    QStringList       m_tapQueue;   // keys Agape48 presses on the user's behalf
    QUrl              m_romSource;
    QString           m_lastError;
    QStringList       m_pressed;
    StateFileManager *m_state = nullptr;
    SkinModel        *m_skin  = nullptr;
};
