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
    // "48SX" kiam la ŝargita ROM estas de la S-serio, alie "48GX".
    Q_PROPERTY(QString model       READ model         NOTIFY readyChanged)

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

    // Keep the Saturn running while the window is not the active one. OFF by
    // default, which is the behaviour this app has always had: Main.qml calls
    // suspend() on deactivation, and suspend() stops the tick. 2026sep10 caught
    // it - the emulator ran ONLY while the calculator had focus - and the
    // default stays off anyway: "Although it's totally ok to have games
    // progress only when focused." So this is an escape hatch for a long
    // computation left to run, not a repair. Machine-local, like liveResize.
    Q_PROPERTY(bool runUnfocused READ runUnfocused WRITE setRunUnfocused NOTIFY runUnfocusedChanged)

    // A MULTIPLIER OVER THE CALIBRATED RATE, for playing with rather than for
    // getting right. 2026sep10: the speed calibration is there to be played
    // with. Default is 1.0 at the middle, and it can be set
    // all the way to the left at sluggish 0.1 to all the way to the right at
    // almost unregulated speed."
    //
    // SEPARATE FROM realSpeedRate ON PURPOSE. That one is the measurement - the
    // instructions a second at which this machine matches a real 48, arrived at
    // over three evenings and worth 0.8% - and dragging a slider must not be
    // able to destroy it. So the slider moves this instead, 1.0 means "what a
    // real 48 does", and coming back to the middle restores authentic speed
    // exactly. speedFactorMax is the far right: the factor at which the pacing
    // budget reaches the free-running ceiling, so the calculator is as fast as
    // it can be without the throttle being switched off.
    Q_PROPERTY(double speedFactor READ speedFactor WRITE setSpeedFactor NOTIFY speedFactorChanged)
    Q_PROPERTY(double speedFactorMax READ speedFactorMax NOTIFY realSpeedRateChanged)
    Q_PROPERTY(int effectiveRate READ effectiveRate NOTIFY speedFactorChanged)

    // Off by default, and off means the calculator runs as fast as the machine
    // allows, because normally a calculator should run as fast as it can. On,
    // the Saturn is paced to a real HP 48's instruction rate,
    // which is what makes a game written for one playable rather than five
    // times too fast. See kRealSpeedInstrPerSec for where the rate comes from.
    Q_PROPERTY(bool realSpeed READ realSpeed WRITE setRealSpeed NOTIFY realSpeedChanged)

    // AŬTOMATAJ →STR KAJ STR→, du ŝaltiloj en Agordoj, ambaŭ defaŭlte malŝaltitaj.
    // Kun autoToStr, Kopii rulas →STR per la ROM kaj kopias ties tekston, do ĉiu
    // tipo de objekto iĝas kopiebla. Kun autoStrTo, algluita aŭ demetita teksto
    // trapasas STR→, kiu kompilas kaj PLENUMAS ĝin ĝuste kiel komandlinio post
    // ENTER: "1 2 +" alvenas kiel 3. Loĝas en la dosierujo de la kalkulilo, kiel
    // la aliaj agordoj de konduto.
    Q_PROPERTY(bool autoToStr READ autoToStr WRITE setAutoToStr NOTIFY autoToStrChanged)
    Q_PROPERTY(bool autoStrTo READ autoStrTo WRITE setAutoStrTo NOTIFY autoStrToChanged)

    // THE RATE ITSELF, so calibrating it never needs a rebuild: a calibration
    // program on the calculator measures the rate, and the number it produces is
    // typed in here once. The default is derived rather than picked - see
    // kRealSpeedInstrPerSec - and it was still too fast for a real game, which is
    // the whole argument for the number being a setting instead of a constant.
    Q_PROPERTY(int realSpeedRate READ realSpeedRate WRITE setRealSpeedRate NOTIFY realSpeedRateChanged)

    // What the emulator is ACTUALLY executing, straight out of x48's own
    // continuous measurement against the host clock. This is the answer to "Don't
    // you see the clock tiks?" - yes, and this is what they say. Live while the
    // calculator is awake; it falls towards nothing in SHUTDN, where the 48
    // spends most of its life by design.
    Q_PROPERTY(int measuredRate READ measuredRate NOTIFY measuredRateChanged)
    Q_PROPERTY(QString logPath      READ logPath      CONSTANT)

    // Kion la fenestro "About" montras. La versio jam estas en
    // Qt.application.version; ĉi tiuj du ne havas QML-ekvivalenton, kaj
    // buildStamp estas skribita je konstruotempo de cmake/BuildStamp.cmake.
    Q_PROPERTY(QString buildStamp   READ buildStamp   CONSTANT)
    Q_PROPERTY(QString qtVersion    READ qtVersion    CONSTANT)

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
    QString model() const;
    QStringList pressedKeys() const { return m_pressed; }
    StateFileManager *state() const { return m_state; }
    SkinModel        *skin()  const { return m_skin; }
    bool hapticsEnabled() const { return m_haptics; }
    bool soundEnabled() const   { return m_sound; }
    bool debugLogging() const   { return m_debugLogging; }
    bool liveResize() const     { return m_liveResize; }
    bool runUnfocused() const   { return m_runUnfocused; }
    double speedFactor() const  { return m_speedFactor; }
    double speedFactorMax() const;
    int  effectiveRate() const;
    bool realSpeed() const      { return m_realSpeed; }
    bool autoToStr() const      { return m_autoToStr; }
    bool autoStrTo() const      { return m_autoStrTo; }
    int  realSpeedRate() const  { return m_realSpeedRate; }
    int  measuredRate() const   { return m_measuredRate; }
    QString logPath() const;
    QString buildStamp() const;
    QString qtVersion() const;

    // ĈU IU KLAVARO ESTAS KONEKTITA. Provo 17, la sola problemo kiun la Androida
    // duono trovis: "If I hold a button down for long in one place, it
    // shows a keyboard shortcut. This should be disabled unless some kind of
    // keyboard is connected."
    //
    // FUNKCIO KAJ NE PROPRECO, ĉar la respondo ŝanĝiĝas dum la programo kuras -
    // Bludenta klavaro povas alveni kaj foriri - kaj propreco kun NOTIFY kiu
    // neniam pafas estus kaŝmemorigita de la unua ligo por ĉiam. Ĝi estas vokata
    // maksimume unu fojon po du sekundoj, kiam klavo estas ŝvebita.
    Q_INVOKABLE bool keyboardAttached() const;

    void setRomSource(const QUrl &url);
    void setHapticsEnabled(bool on);
    void setSoundEnabled(bool on);
    void setDebugLogging(bool on);
    void setLiveResize(bool on);
    void setRunUnfocused(bool on);
    void setSpeedFactor(double factor);
    void setRealSpeed(bool on);
    void setRealSpeedRate(int instructionsPerSecond);
    void setAutoToStr(bool on);
    void setAutoStrTo(bool on);

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
    // The number is on screen because a wait with no end in sight is the
    // difference between "it is working" and "it has hung".
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
    // you get. The shape of it was settled on 2026aug31.
    Q_PROPERTY(bool detached READ isDetached NOTIFY detachedChanged)
    bool isDetached() const { return m_detached; }

    // Whether SOMEBODY ELSE is holding the memory this window is pointed at -
    // a live lock file that is not ours. A different question from detached,
    // which only says WE are not holding it, and the difference is the whole
    // of the 2026sep04 report: a calculator you left switched off comes up
    // detached with nobody else involved at all, and the screen said "in use by
    // another device" over an empty shelf. Polled while detached, because a
    // window that is not running the machine has no other way to notice the
    // other side letting go.
    Q_PROPERTY(bool memoryHeldElsewhere READ memoryHeldElsewhere
                                        NOTIFY memoryHeldElsewhereChanged)
    bool memoryHeldElsewhere() const { return m_heldElsewhere; }
    Q_INVOKABLE bool attach(bool takeOver = false);

    Q_INVOKABLE bool    openCalculator(const QString &name, bool takeOver = false);
    // The unanswered case, in one entry point rather than three in QML. Which
    // of the three it is depends on why this window does not have the
    // calculator, and the dialog has no business knowing that: never started
    // (it was busy when the window opened), started and handed over, or simply
    // somebody else's.
    Q_INVOKABLE bool    takeOverCalculator(const QString &name);
    Q_INVOKABLE QString newCalculator();
    // Renaming goes through the engine, not straight to the state file
    // manager, because the C core holds the folder's path from start()
    // and has to be put down while the folder moves. See the definition.
    Q_INVOKABLE bool    renameCalculator(const QString &name, const QString &to);

    Q_INVOKABLE bool hasStackObject() const;
    // The banner times out but lastError did not, so a failed import was still
    // sitting in the settings window minutes later - dogfood #15 line 20.
    Q_INVOKABLE void clearError();

    Q_INVOKABLE bool importFile(const QUrl &url);
    Q_INVOKABLE bool exportFile(const QUrl &url);

    // --- la ROM ---------------------------------------------------------------
    // inspectRom() juĝas dosieron sen ŝargi ĝin kaj redonas {ok, problem,
    // model}. Ĝi legas 42 bajtojn, do la kampo en Agordoj rajtas voki ĝin je ĉiu
    // tajpita signo. romLoadedPath() estas la loka vojo de la dosiero kiun la
    // kerno vere malfermis - ne ĉiam tiu kiun la uzanto elektis, ĉar dokumento
    // elektita sur Androido estas kopiita apud la staton. loadRom() estas la sola
    // vojo kiu ŝanĝas la ROM-on de funkcianta kalkulilo: start() sola revenas tuj
    // kiam la kerno jam staras, kaj tial elekto en Agordoj ŝanĝis nenion.
    Q_INVOKABLE QVariantMap inspectRom(const QString &text) const;
    Q_INVOKABLE QString romLoadedPath() const { return m_romLoadedPath; }
    Q_INVOKABLE bool loadRom(const QUrl &source);

    // --- tiri kaj demeti sur la kalkulilon -----------------------------------
    // dropKind() rigardas nur kio estas tirata: "rom", "object", "text", aŭ
    // "unknown" kiam la enhavo ankoraŭ ne estas legebla (Androido montras nur la
    // tipon ĝis la demeto). Malplena signifas nenion ŝarĝeblan. dropAllowed()
    // aldonas la staton de la kalkulilo en la momento de la voko - legitan rekte
    // el ĝia RAM, sen konservo - kaj kostas nenion, do ĝi respondas je ĉiu movo
    // de la muso. ROM estas akceptata nur kiam neniu ROM ekzistas; ĉio alia nur
    // kiam la kalkulilo atendas ĉe la stako.
    Q_INVOKABLE QString dropKind(const QList<QUrl> &urls, const QString &text);
    Q_INVOKABLE bool dropAllowed(const QString &kind) const;
    Q_INVOKABLE bool drop(const QList<QUrl> &urls, const QString &text);

    Q_INVOKABLE QUrl    pathToUrl(const QString &path) const;
    Q_INVOKABLE QString urlToPath(const QUrl &url) const;

    Q_INVOKABLE bool startSystemMove(QQuickWindow *window);

    // FENESTRO VOKITA PER BUTONO APERAS INTERNE DE LA EKRANO. Ĝia kadro estas
    // ŝovita en la liberan areon de la ekrano, kiu enhavas ĝian centron aŭ tiun
    // de ĝia gepatra fenestro. Poste la fenestro estas movebla kien ajn.
    Q_INVOKABLE void keepOnScreen(QQuickWindow *window) const;

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

    // "Reset memory and quit", kiu ekde 2026sep16 vere forigas la memoron.
    // Demetas la kernon SEN konservo kaj forigas la dosierojn kiuj estas la
    // kalkulilo, do la sekva starto konstruas la memoron el nenio.
    bool forgetMemory();
    bool saveState();
    bool reloadState();

    // --- clipboard ---------------------------------------------------------
    // THE TEXT, not a bool. Copy used to answer yes-or-no to a caller that
    // asked nothing and told nobody, which is how two menu items over a stub
    // in the core - "clipboard: not implemented" - went weeks without anyone
    // noticing they did nothing. Handing back what was copied lets the menu
    // say it out loud, and that sentence is also the only way to see, from
    // outside, that the number was read correctly. Empty means it failed and
    // lastError says why - krom kun la aŭtomata →STR, kiam la redono ĉiam estas
    // malplena kaj la teksto alvenas poste per clipboardCopied, post la ROM.
    QString copyStackToClipboard();
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
    void realSpeedChanged();
    void realSpeedRateChanged();
    void autoToStrChanged();
    void autoStrToChanged();
    // Kion Kopii metis en la tondujon. Ankaŭ la aŭtomata →STR alvenas ĉi tie,
    // kelkajn momentojn post la menuero, kiam la ROM finis.
    void clipboardCopied(const QString &text);
    void measuredRateChanged();
    void liveResizeChanged();
    void runUnfocusedChanged();
    void speedFactorChanged();

    void frameReady();                      // LcdItem listens; fires only on change
    void waitingChanged();
    void waitSecondsChanged();
    // Something worth saying that is not a fault. The banner has a quieter
    // style for these; lastError is red and stays up four times as long.
    void notice(const QString &text);
    void otherLetGo(const QString &instance);
    // The ask ran out of time, and WHY it did decides what the dialog may
    // offer. reason is one of:
    //   "held"      they still hold the lock and have said nothing
    //   "arriving"  they let go and their memory is still coming over
    //   "letgo"     they let go, but nothing addressed to us arrived
    // Emitted again, without raising the window a second time, whenever
    // the reason changes - because it does, and used not to.
    void sleepUnanswered(const QString &instance, const QString &host,
                         const QString &reason);
    void beep(int frequencyHz, int durationMs);
    void keyFeedback(const QString &keyId); // QML plays haptics/sound off this
    void detachedChanged();
    void memoryHeldElsewhereChanged();
    // Could not take the calculator back: QML shows who has it and what can be
    // done about it. The map is StateFileManager::lockHolder().
    void attachRefused(const QVariantMap &holder);

    void romRequired();                     // no ROM yet - QML shows the picker

private:
    void tick();
    void setError(const QString &what);
    void pollForRelease();
    void pollLockHolder();
    bool lookupKey(const QString &keyId, int *row, int *mask) const;
    void markPressed(int row, int mask, bool down);
    void finishRelease(int row, int mask);
    void setTickRate(int ms);
    // Re-read the settings that live in the state folder. Called at startup and
    // again every time the folder changes, because a different folder is a
    // different calculator with its own preferences.
    void loadCalcSettings();
    int  realSpeedBudget();
    void writeSpeedProbe();
    void queueTaps(const QStringList &keys);
    QStringList unlatchShifts() const;
    QString notReadyReason() const;
    bool readyForObject(bool pressesKeys);
    bool pasteText(const QString &text);
    void beginAuto(int step);
    void stepAuto();
    QString sniffDropFile(const QUrl &url) const;
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
    qint64            m_tickCount = 0;
    // Polled rather than watched: the calculator being waited for is often not
    // the one this window has open, so its folder is not the one the watcher is
    // pointed at. One stat a second for at most a minute and a half.
    QTimer            m_wait;
    QTimer            m_lockWatch;
    QString           m_waitFor;
    QString           m_waitHost;
    bool              m_waitTake = true;
    QDateTime         m_waitUntil;
    int               m_waitSeconds = 0;
    // Empty until the countdown runs out; then whichever of held /
    // arriving / letgo is true right now, re-tested on every poll.
    QString           m_waitWhy;
    x48_frame_t       m_frame {};
    quint64           m_frameSerial = 0;
    int               m_annunciators = 0;
    bool              m_ready = false;
    bool              m_haptics = true;
    bool              m_sound = true;
    bool              m_debugLogging = false;

    // Real-speed pacing. m_paceAt is the reading of m_clock at the last slice
    // and m_paceOwed the fraction of an instruction carried over, in
    // instruction-microseconds - without it the rate would be quietly rounded
    // down once per tick, which over a minute is a visible loss.
    bool              m_realSpeed = false;
    int               m_realSpeedRate = 0;   // set from settings in the ctor
    int               m_measuredRate  = 0;
    int               m_rateSample    = 0;   // ticks since the last reading
    qint64            m_paceAt    = 0;
    qint64            m_paceOwed  = 0;
    bool              m_liveResize    = false;
    bool              m_runUnfocused  = false;
    double            m_speedFactor   = 1.0;
    bool              m_displayOff = false;
    bool              m_detached = false;
    bool              m_heldElsewhere = false;
    bool              m_sawFirstFrame = false;
    // Set by start(), spent by the first frame after it. That frame shows the
    // calculator as it was SAVED, so it must never be read as the user having
    // just switched the machine off - see tick().
    bool              m_freshLoad = false;
    // Set by start() when the calculator has never been saved. A machine with
    // no state file on disk was not "left" anywhere, so its first frame says
    // nothing about how anybody put it down - see tick().
    bool              m_bornEmpty = false;
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

    // La aŭtomata →STR aŭ STR→ en progreso: 0 neniu, 1 →STR, 2 STR→. La objekto
    // estas la adreso de la programo puŝita por EVAL, kaj ĝia malapero de nivelo
    // 1 estas la signo, ke la ROM vere rulis ĝin. La teksto estas tiu de la ĉeno,
    // kiun STR→ ricevis: rifuzita STR→ remetas ĉenon kun ĝuste ĝi.
    bool              m_autoToStr = false;
    bool              m_autoStrTo = false;
    int               m_autoStep = 0;
    quint32           m_autoObject = 0;
    QString           m_autoPasted;
    // Demeto sur fenestron sen fokuso rekomencigis la haltigitan takton por siaj
    // klavoj. Kiam ĉio finiĝis, la kalkulilo reiras al la paŭzo.
    bool              m_dropWoke = false;
    qint64            m_autoSince = 0;
    bool              m_autoEvalQueued = false;
    QUrl              m_romSource;
    QString           m_romLoadedPath;
    QString           m_lastError;
    QStringList       m_pressed;
    StateFileManager *m_state = nullptr;
    SkinModel        *m_skin  = nullptr;
};
