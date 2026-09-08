#include "Agape48Engine.h"

#include <QCoreApplication>

#include "SkinModel.h"
#include "StateFileManager.h"

#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QSettings>
#include <QHash>
#include <QStringView>

namespace {

// Emulation pacing. The HP 48 Saturn runs at ~4 MHz (48G) / ~2 MHz (48S), so a
// 60 Hz tick is roughly 70000 cycles. Ticking on the GUI thread is deliberate:
// a 4 MHz interpreter costs low single-digit percent of one modern core, and a
// worker thread would buy nothing but a frame-handoff race. If profiling ever
// says otherwise, this is the one place that has to move.
constexpr int  kTickIntervalMs = 16;

// How long a key is guaranteed to stay down. The HP 48's ROM polls the matrix
// on its own schedule - about 40 ms between scans - so a press and release
// inside one gap is never seen at all. Dogfood #11 line 2: pressing Esc did
// not switch the calculator back on, because from SHUTDN the scan only begins
// after the key arrives, and a tap was over before it got there.
constexpr int  kMinHoldMs      = 60;
constexpr int  kCyclesPerTick  = 70000;
constexpr int  kIdleIntervalMs = 100;

// Waiting for another instance to answer a sleep request. Long enough for the
// question and the answer to cross a sync folder - a busy Dropbox takes tens of
// seconds - and short enough that a machine which is simply switched off does
// not hold somebody at a dialog for ever.
constexpr int  kSleepWaitSecs = 90;
constexpr int  kSleepPollMs   = 1000;
// How long the calculator is given to switch ITSELF off when another machine
// asks for it. Three taps at one tick each is a fraction of a second, so this
// is not a budget - it is the point at which we conclude the ROM is never going
// to answer and hand the calculator over anyway.
constexpr int  kSleepOffGraceMs = 5000;

// The rate we fall back to while the Saturn is parked in SHUTDN, which is where
// it spends nearly all of its life. It must not be zero. The tick used to stop
// outright, and that broke waking: SHUTDN is not a halt, it is a wait, and the
// ROM leaves it on a TIMER tick as readily as on a key. With the tick stopped
// nothing ever delivered that timer, so a machine that parked mid-wake stayed
// parked until the next key press happened to shake it loose - Gert's "I have
// to tap Esc 3 times", and the same reason clicking the title bar appeared to
// help. Ten times a second costs a few dozen instructions and keeps the 48's
// clock and alarms honest, which a full stop also quietly broke.

// -----------------------------------------------------------------------------
// Key name -> (out row, in mask).
//
// The NAMES are the contract skins are written against, and they are stable.
//
// The CODES were transcribed on 2026aug28 from the vendored buttons[] table at
// src/core/x48/x48.c:233 - x48's original 1994 array, 49 entries. Its 4th field
// is a packed code that x48.c:386 decodes as row = code >> 4, mask = 1 << (code
// & 0xf). Checked: 49 entries, no two keys share a (row, mask), all nine rows
// used, masks 0x01..0x20. Rows 1, 2 and 3 carry a sixth key (SHR, SHL, ALPHA).
//
// Two names differ from x48's: its digits are "7", "8" where skins say "N7",
// "N8", and its "COLON" is the third row's first key, which skins call "QUOTE".
//
// lookupKey() still fails loudly for anything not here, and the engine reports
// which key by name rather than pressing something arbitrary.
// -----------------------------------------------------------------------------
// --- debug log ---------------------------------------------------------------
// One file, appended to, installed as Qt's message handler only while the
// option is on. Gert asked for this after dogfood #8, where the only record of
// why the app would not start was a red banner hidden behind a window.
QFile *g_logFile = nullptr;
QtMessageHandler g_previousHandler = nullptr;

void agape48LogHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    if (g_logFile && g_logFile->isOpen()) {
        static const char *kLevel[] = { "debug", "warning", "critical", "fatal", "info" };
        QTextStream out(g_logFile);
        out << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
            << "  " << kLevel[type <= QtInfoMsg ? type : 0] << "  " << msg << "\n";
        out.flush();
    }
    if (g_previousHandler)
        g_previousHandler(type, ctx, msg);
}

struct KeyDef { const char *id; int row; int mask; };

constexpr int kUnmapped = -1;

const KeyDef kKeys[] = {
    // menu row
    { "A", 1, 0x10 }, { "B", 8, 0x10 }, { "C", 8, 0x08 },
    { "D", 8, 0x04 }, { "E", 8, 0x02 }, { "F", 8, 0x01 },
    // second row
    { "MTH", 2, 0x10 }, { "PRG", 7, 0x10 }, { "CST", 7, 0x08 },
    { "VAR", 7, 0x04 }, { "UP", 7, 0x02 }, { "NXT", 7, 0x01 },
    // third row
    { "QUOTE", 0, 0x10 }, { "STO", 6, 0x10 }, { "EVAL", 6, 0x08 },
    { "LEFT", 6, 0x04 }, { "DOWN", 6, 0x02 }, { "RIGHT", 6, 0x01 },
    // fourth row
    { "SIN", 3, 0x10 }, { "COS", 5, 0x10 }, { "TAN", 5, 0x08 },
    { "SQRT", 5, 0x04 }, { "POWER", 5, 0x02 },
    // fifth row
    { "INV", 5, 0x01 }, { "EEX", 4, 0x04 }, { "NEG", 4, 0x08 },
    { "DEL", 4, 0x02 }, { "BS", 4, 0x01 },
    // numeric block
    { "ALPHA", 3, 0x20 }, { "N7", 3, 0x08 }, { "N8", 3, 0x04 },
    { "N9", 3, 0x02 }, { "DIV", 3, 0x01 },
    { "SHL", 2, 0x20 }, { "N4", 2, 0x08 }, { "N5", 2, 0x04 },
    { "N6", 2, 0x02 }, { "MUL", 2, 0x01 },
    { "SHR", 1, 0x20 }, { "N1", 1, 0x08 }, { "N2", 1, 0x04 },
    { "N3", 1, 0x02 }, { "MINUS", 1, 0x01 },
    { "N0", 0, 0x08 }, { "PERIOD", 0, 0x04 }, { "SPC", 0, 0x02 },
    { "PLUS", 0, 0x01 }, { "ENTER", 4, 0x10 },
    // ON is not in the matrix: X48_KB_MASK_ON sets bit 15 in all nine rows,
    // which is x48.c:381. The row here is ignored. Part of the ON+A+F reset.
    { "ON", 0, X48_KB_MASK_ON },
};

// The same table read backwards, so a press addressed by matrix code still
// knows which key it lit. pressCode() is the one place both the name path and
// the direct-matrix path meet, so the bookkeeping goes there and neither
// caller has to remember to do it.
const QHash<int, QString> &codeTable()
{
    static const QHash<int, QString> table = [] {
        QHash<int, QString> t;
        t.reserve(std::size(kKeys));
        for (const KeyDef &k : kKeys)
            if (k.row >= 0)
                t.insert((k.row << 16) | k.mask, QString::fromLatin1(k.id));
        return t;
    }();
    return table;
}

const QHash<QString, QPair<int, int>> &keyTable()
{
    static const QHash<QString, QPair<int, int>> table = [] {
        QHash<QString, QPair<int, int>> t;
        t.reserve(std::size(kKeys));
        for (const KeyDef &k : kKeys)
            t.insert(QString::fromLatin1(k.id), { k.row, k.mask });
        return t;
    }();
    return table;
}


// --- documents that are not files -------------------------------------------
//
// Android's picker hands back a content:// DOCUMENT, not a file. It has no
// POSIX path at all, so toLocalFile() is empty and the C core - which fopen()s
// what it is given - has nothing to work with. Until tonight both directions
// simply refused, and on a phone the picker is the only way to name a file, so
// that refusal covered every file there was. Gert, on the phone: "when I try to
// put a file on the stack it doesn't work, saying something that 'it can only
// read a file in this computer'."
//
// Qt's own QFile does understand content://, so the bytes make the trip through
// a scratch file in the app's cache and the core still only ever sees a real
// path. start() already does exactly this for a picked ROM; this is that,
// generalised and used in both directions.
//
// One scratch name, not a unique one: an import and an export cannot be in
// flight at the same time, since both run from the same menu on the same
// thread, and a fixed name means a crash leaves one stale file rather than a
// growing pile.
QString transferScratchPath()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(dir);
    return dir + QLatin1String("/agape48-transfer");
}

// Either end may be a content:// document or a plain path; QFile takes both.
bool copyBytes(const QString &from, const QString &to)
{
    QFile in(from);
    if (!in.open(QIODevice::ReadOnly))
        return false;
    QFile out(to);
    // Truncate matters on the way OUT: the document Android just created is
    // empty, but one the user picked to overwrite is not, and a shorter object
    // written over a longer one would otherwise keep the old tail.
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        // Not every document provider honours "wt". "w" alone is worth one
        // retry before telling the user it cannot be written.
        if (!out.open(QIODevice::WriteOnly))
            return false;
    }
    const QByteArray bytes = in.readAll();
    if (out.write(bytes) != bytes.size())
        return false;
    // close() rather than trusting the destructor: a content:// write is only
    // handed to the provider on close, and that is where it can still fail.
    out.close();
    return out.error() == QFileDevice::NoError;
}

} // namespace

Agape48Engine::Agape48Engine(QObject *parent)
    : QObject(parent)
    , m_state(new StateFileManager(this))
    , m_skin(new SkinModel(this))
{
    if (QSettings().value(QLatin1String("debug/logging"), false).toBool())
        setDebugLogging(true);

    m_liveResize = QSettings().value(QLatin1String("window/liveResize"), false).toBool();

    m_clock.start();
    m_tick.setInterval(kTickIntervalMs);
    m_tick.setTimerType(Qt::PreciseTimer);
    connect(&m_tick, &QTimer::timeout, this, &Agape48Engine::tick);

    // The state manager's own failures - a folder that does not exist, a copy
    // that would not copy - had nowhere to go: nothing read its lastError
    // except start(). So typing a bad folder into Settings looked like nothing
    // happening at all. Now they surface exactly like any other error.
    connect(m_state, &StateFileManager::lastErrorChanged, this, [this] {
        // Both directions: a cleared state error clears ours, or the message
        // outlives the problem and sits there after it has been fixed.
        setError(m_state->lastError());
    });

    // Refused at startup - no ROM, or another instance holding the state
    // folder - and then given a folder that works: start now, rather than
    // making the user quit and reopen to get a calculator. migrateTo() and the
    // house button both land here.
    connect(m_state, &StateFileManager::locationChanged, this, [this] {
        if (!m_ready) {
            start();
            return;
        }
        // The folder changed under a RUNNING calculator - Settings, the picker,
        // or a folder dropped on the window. The core keeps the path it was
        // initialised with in files_path, so without this it would carry on
        // saving into the folder we just left, and the calculator in the new
        // one would never be read at all. shutdownCore() saves first, into the
        // old folder, which is where that calculator still lives.
        shutdownCore();
        start();
    });

    // Everything the state object had to say went nowhere. The banner and the
    // settings strip both read the ENGINE's lastError, and StateFileManager
    // has its own; start() and attach() copied it across by hand and nothing
    // else did. So a refused folder move - "that calculator is already open",
    // "there is no folder called X" - was silent, and the user was left with a
    // path in the box and no calculator moved. One connection rather than a
    // copy at every call site.
    connect(m_state, &StateFileManager::lastErrorChanged, this, [this] {
        if (!m_state->lastError().isEmpty())
            setError(m_state->lastError());
    });

    // An external sync client rewriting the state file under us is the normal
    // case, not the exceptional one - that is the whole point of BYO-sync.
    // Somebody took the calculator over. Treat it exactly like OFF: stop
    // touching the files, and wait for ON.
    connect(m_state, &StateFileManager::lockLost, this, [this] {
        if (m_detached)
            return;
        m_detached = true;
        stop();
        emit detachedChanged();
        setError(tr("This calculator was taken over somewhere else. "
                    "Press ON to take it back."));
    });

    // Somebody has asked for this calculator. This is the polite handover and
    // it is better than being taken over, not just nicer: we still hold the
    // lock here, so we can SAVE first, and what they pick up is everything
    // that was done in this window. A take-over cannot do that - by the time
    // the loser notices, the folder is not its to write.
    // Gert, 2026sep03: "make sure that for handing over, the calculator's
    // auto-sleep function has the same effect as pressing the green shift
    // followed by ON."
    //
    // He is right, and it is not only tidiness. Until now this saved and let go
    // WITHOUT switching the Saturn off, so the machine written to disk was one
    // frozen mid-instruction rather than one parked in SHUTDN - a state no HP 48
    // ever reaches by itself, and a state nothing else in this program produces.
    // Two things follow from that. The screen did not blank, which is report
    // both-03 lines 16 and 26: "the 'sleeping' calculator failed to blank its
    // screen, so I doubted it was really sleeping". And Android, which is next,
    // suspends and kills the process whenever it likes - so the on-disk state
    // has to be one that loads cleanly from cold, which is exactly the OFF
    // state and exactly what attach() and the first-frame path already assume.
    //
    // So press the key rather than emulate its consequences. The screen going
    // off is ALREADY the hand-over path - see tick() - so this does not repeat
    // that logic, it joins it. One way to hand a calculator over, whether the
    // person or the other machine asked for it.
    connect(m_state, &StateFileManager::sleepRequested, this, [this](const QString &host) {
        if (m_detached)
            return;
        m_sleepFor = host;
        m_sleepAskedAt = m_clock.elapsed();
        if (!m_ready || m_displayOff) {
            // Already off, or never started: there is no key to press, and the
            // screen will not change to tell us it worked.
            handOver();
            return;
        }
        // ON with a shift latched is not ON, which is the whole point here: OFF
        // is printed on the key in the right shift's colour. A latched LEFT
        // shift would give CONT instead, so cancel that first - pressing a
        // shift while it is active is what cancels it - and only latch the
        // right one if the user has not already done it themselves.
        QStringList seq;
        if (m_annunciators & X48_ANN_LEFT)     seq << QStringLiteral("SHL");
        if (!(m_annunciators & X48_ANN_RIGHT)) seq << QStringLiteral("SHR");
        seq << QStringLiteral("ON");
        queueTaps(seq);
    });

    // The files changed underneath us. Gert's rule, and it is the right one:
    // going to sleep is the only move that cannot lose anybody's work. The
    // state on disk and the state in memory are two different calculators now,
    // and there is no way to reconcile them that is not a guess - so stop,
    // write NOTHING over what just arrived, and let ON read whatever is
    // actually there.
    connect(m_state, &StateFileManager::externalChangeDetected, this, [this] {
        if (m_detached)
            return;                 // not ours until somebody presses ON
        if (!isRunning()) {
            // Asleep in the window's own sense - suspended, or between ticks -
            // but still ours. Take the newer version rather than clobbering it
            // on the next save.
            reloadState();
            return;
        }
        stop();                     // no saveState(): theirs is the one on disk
        m_state->release();
        m_detached = true;
        emit detachedChanged();
        setError(tr("These files were changed by something else - a sync client, "
                    "or another machine. The calculator went to sleep without "
                    "saving. Press ON to open the version now on disk."));
    });

    m_wait.setInterval(kSleepPollMs);
    m_wait.setTimerType(Qt::CoarseTimer);
    connect(&m_wait, &QTimer::timeout, this, &Agape48Engine::pollForRelease);

    // The same cadence the countdown already uses, for the same reason: a
    // detached window has stopped the core and has nothing else that would tell
    // it the other machine let go. One small file read a second, and only while
    // detached - reading never writes, so this cannot restart the Dropbox churn
    // that both-03 line 8 is about.
    m_lockWatch.setInterval(kSleepPollMs);
    m_lockWatch.setTimerType(Qt::CoarseTimer);
    connect(&m_lockWatch, &QTimer::timeout, this, &Agape48Engine::pollLockHolder);
    connect(this, &Agape48Engine::detachedChanged, this, [this] {
        if (m_detached) {
            pollLockHolder();           // answer now, not in a second's time
            m_lockWatch.start();
        } else {
            m_lockWatch.stop();
            if (m_heldElsewhere) {      // ours again; nobody else can hold it
                m_heldElsewhere = false;
                emit memoryHeldElsewhereChanged();
            }
        }
    });

    m_skin->load(QUrl(QStringLiteral("qrc:/qt/qml/Agape48/assets/skins/default/layout.json")));

    // Let go of the state folder on the way out. aboutToQuit rather than the
    // destructor alone: Qt.quit() from the menu tears the QML engine down in
    // an order this object does not control, and a lock left behind is a lock
    // the next start has to reason about. A crash still leaves one, which is
    // exactly what the dead-pid check in claim() is for.
    //
    // SAVE BEFORE RELEASING, which is new on 2026sep03 and is an ordering fix
    // rather than a tidy-up. The save on quit used to happen in the destructor,
    // which runs AFTER this - so every ordinary quit wrote the calculator into a
    // folder it had already unlocked. On one machine that is invisible. On a
    // shared shelf it is a race: the other machine can claim the calculator in
    // that window and our save then lands on top of its work.
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        saveState();                    // while it is still ours to save
        m_state->release();
    });

#ifdef Q_OS_ANDROID
    // THE PLATFORM'S OWN SIGNAL, because the window's is never delivered here.
    // On a desktop the save is hung on Window.onActiveChanged -> suspend(), and
    // that is enough: a window always loses focus before it goes away. Android
    // does not work like that. The back gesture finishes the Activity and the
    // process is gone; aboutToQuit does not run, the QML Window never reports
    // itself inactive, and everything since the last explicit save is lost.
    //
    // MEASURED on Gert's phone, 2026sep07: cold-boot a new calculator, answer
    // the recovery prompt, leave with the back gesture, come back - and it asks
    // "Try To Recover Memory?" all over again, because ram and hp48 on disk
    // were still the ones written when the calculator was last switched by
    // hand, twenty minutes earlier.
    //
    // applicationStateChanged is what Qt raises from the Activity's own
    // onPause/onStop, which Android guarantees before it may kill the process.
    // Inactive rather than Suspended: Suspended is not reached on every device,
    // and this is the point at which the machine must already be on disk.
    connect(qApp, &QGuiApplication::applicationStateChanged, this,
            [this](Qt::ApplicationState state) {
                if (state == Qt::ApplicationInactive
                    || state == Qt::ApplicationSuspended)
                    suspend();
                else if (state == Qt::ApplicationActive)
                    resumeFromBackground();
            });
#endif
}

Agape48Engine::~Agape48Engine()
{
    if (m_ready) {
        // saveState(), not x48_save_state(): this is the second of the two sites
        // that went straight to the core and so skipped the "never write a
        // calculator we do not hold" guard. MEASURED: hand a calculator to the
        // other machine, let it work, then close the window you handed it from,
        // and this line wrote the stale memory back over the new owner's work.
        // The marker planted on disk, sha f42e0e7c..., came back as this
        // process's sha 89e83ca8... the moment the window closed.
        //
        // By the time we get here aboutToQuit has usually saved and released
        // already, so on an ordinary quit this is a no-op twice over - not held,
        // and the digest matches. It still matters for the paths that reach the
        // destructor without aboutToQuit.
        saveState();
        x48_shutdown();
    }
    m_state->release();
}

// --- lifecycle --------------------------------------------------------------

bool Agape48Engine::start()
{
    if (m_ready) {
        if (!m_tick.isActive()) {
            if (m_debugLogging)
                qWarning("emulation resumed");
            m_tick.start();
            emit runningChanged();
        }
        return true;
    }

    // Before giving up, look for a "rom" beside the state. That is the layout
    // x48 and Droid48 both use and the one x48_init() falls back to when
    // rom_path is null, so dropping a ROM in the state folder just works.
    // Decision 7 will bundle the ROM and set this properly.
    // Remembered from last time. Without this a ROM typed into Settings worked
    // for one session and was forgotten on restart, so the only thing that
    // could ever find a ROM again was the "beside the state" fallback below -
    // which is exactly what failed in dogfood #8 once migration had left the
    // ROM behind in the old folder.
    if (m_romSource.isEmpty()) {
        const QString saved = QSettings().value(QLatin1String("rom/source")).toString();
        if (!saved.isEmpty() && QFileInfo::exists(QUrl(saved).toLocalFile())) {
            m_romSource = QUrl(saved);
            emit romSourceChanged();
        }
    }

    if (m_romSource.isEmpty()) {
        const QString beside =
            m_state->location().toLocalFile() + QLatin1String("/rom");
        if (QFileInfo::exists(beside)) {
            m_romSource = QUrl::fromLocalFile(beside);
            // Not through setRomSource(): a fallback should not be written to
            // QSettings and frozen. But QML has to hear about it, or the ROM
            // field in Settings sits empty while a ROM is plainly loaded.
            emit romSourceChanged();
        }
    }

    if (m_romSource.isEmpty()) {
        emit romRequired();
        // Naming the folder is the whole difference between "something is
        // wrong" and "put a file called rom in here".
        setError(tr("No HP 48 ROM. There is no file named \"rom\" in %1, and "
                    "none has been chosen in Settings.")
                     .arg(m_state->location().toLocalFile()));
        return false;
    }

    // One calculator, one instance. Before anything is read or written: a
    // second copy pointed at this folder would save the whole state on quit
    // and wipe whatever the first had done. Dogfood #10 raised it, #13 settled
    // the shape of the answer.
    if (!m_state->claim()) {
        // The dialog rather than the shelf: it names who has it and offers to
        // ask them for it, and "Choose another…" opens the shelf from there.
        // Before 2026sep02 this went straight to the shelf, which could only
        // report the problem.
        emit attachRefused(m_state->lockHolder());
        setError(m_state->lastError());
        return false;
    }

    logStartupFacts();

    x48_config_t cfg {};
    cfg.fd_ram = cfg.fd_port1 = cfg.fd_port2 = cfg.fd_state = -1;
    cfg.throttle = true;

    // A ROM chosen on Android does not arrive as a file. Android's document
    // picker hands back a content:// document, which has no POSIX path at all:
    // toLocalFile() is empty, and the C core has nothing to fopen(). Qt's own
    // QFile does understand content://, so copy the bytes once into the state
    // folder under the name x48 already looks for, and from the next line down
    // this is the ordinary "a ROM sits beside the state" case that every
    // platform takes. Costs 512 KB in the app's own folder and turns the one
    // gesture an Android user has - pick a file from Downloads or Dropbox -
    // into a ROM the emulator can start from.
    if (!m_romSource.isEmpty() && !m_romSource.isLocalFile()
        && m_state->location().isLocalFile()) {
        const QString dest =
            m_state->location().toLocalFile() + QLatin1String("/rom");
        if (!QFileInfo::exists(dest)) {
            QFile in(m_romSource.toString());
            QFile out(dest);
            if (!in.open(QIODevice::ReadOnly)
                || !out.open(QIODevice::WriteOnly | QIODevice::Truncate)
                || out.write(in.readAll()) <= 0) {
                out.remove();
                setError(tr("Could not copy the chosen ROM into %1.")
                             .arg(m_state->location().toLocalFile()));
                return false;
            }
        }
        // Deliberately not through setRomSource(): what gets remembered should
        // be the document the user picked, not a copy of it we made.
        m_romSource = QUrl::fromLocalFile(dest);
        emit romSourceChanged();
    }

    const QByteArray romPath = m_romSource.toLocalFile().toUtf8();
    cfg.rom_path = romPath.constData();

    // Desktop fills cfg.state_dir; Android fills cfg.fd_* from SAF, because a
    // content:// tree URI has no POSIX path to hand to the C core.
    QByteArray stateDirUtf8;
    if (!m_state->populate(&cfg, &stateDirUtf8)) {
        setError(m_state->lastError());
        return false;
    }

    // BORN, NOT PUT DOWN. A calculator whose state file does not exist yet has
    // never been saved by anybody, so the first frame it draws is the ROM
    // booting - not a record of how it was left. tick() needs that difference;
    // see the m_freshLoad branch there for what reading it wrong costs.
    m_bornEmpty = !stateDirUtf8.isEmpty()
               && !QFileInfo::exists(QString::fromUtf8(stateDirUtf8)
                                     + QLatin1String("/hp48"));

    if (!x48_init(&cfg)) {
        setError(QString::fromUtf8(x48_last_error()));
        return false;
    }

    m_ready = true;
    // Whatever the next frame shows is the calculator as it was saved, not as
    // anybody just left it. Spent by the first frame in tick().
    m_freshLoad = true;

    // A NEW CALCULATOR MUST NOT WEAR THE OLD ONE'S SCREEN. A machine with
    // nothing saved boots into SHUTDN and stays there until ON, and a parked
    // machine produces no frames at all - so the LCD went on showing whatever
    // was last drawn. Measured on the phone: "New calculator" opened Calculator
    // 2 with Calculator 1's stack still on the glass, name and all. Blank it
    // here, where we already know there is nothing to draw.
    if (m_bornEmpty) {
        m_frame = {};
        ++m_frameSerial;
        emit frameReady();
    }
    setError(QString());
    emit readyChanged();
    m_tick.start();
    emit runningChanged();
    return true;
}

void Agape48Engine::stop()
{
    if (!m_tick.isActive())
        return;
    if (m_debugLogging)
        qWarning("emulation stopped (window inactive, or stop() called)");
    m_tick.stop();
    releaseAllKeys();
    emit runningChanged();
}

void Agape48Engine::suspend()
{
    stop();
    saveState();
}

void Agape48Engine::resumeFromBackground()
{
    // No calculator at all - it was in use when this window opened, or there is
    // no ROM. Do NOT retry the claim here: getting a calculator is a deliberate
    // act, and start() emits attachRefused when it cannot, so retrying on every
    // window activation threw the in-use dialog on top of whatever the user had
    // opened to deal with it. "Choose another…" opened the shelf and the dialog
    // landed straight back on top of it.
    if (!m_ready)
        return;
    // Cheap fingerprint check first: if a sync client touched the file while we
    // were away, take its version rather than clobbering it on next save.
    if (m_state->hasExternalChange())
        reloadState();
    start();
}

// --- tick -------------------------------------------------------------------

void Agape48Engine::tick()
{
    x48_run_slice(kCyclesPerTick);

    if (x48_take_frame(&m_frame)) {
        ++m_frameSerial;
        // What the user sees, not what a register says. OFF does not clear
        // display.on - it blanks the buffer and drops into SHUTDN - so testing
        // height alone would never have fired. Dogfood #10 line 13 asked what
        // had made the screen go blank and the log could not say, because it
        // only ever spoke at startup.
        // The display's own on/off bit, not a scan of the pixels. Scanning was
        // the first version and it lied: the ROM clears the whole screen before
        // it draws a full-screen form, and for that one frame an ordinary menu
        // looks exactly like a calculator that has been switched off. It also
        // read as "never fires" at first, which is what sent me to the pixels -
        // the real reason was x48_take_frame() skipping a frame in which only
        // display.on had changed. That is fixed at the source now.
        const bool off = m_frame.height == 0;
        // THE FIRST FRAME AFTER A LOAD SAYS HOW THE CALCULATOR WAS LEFT. It
        // never says what the person at the keyboard just did, and the two were
        // being confused in two different places.
        //
        // This used to key off m_sawFirstFrame, which is set once per PROCESS.
        // That was true enough while a calculator was only ever loaded at
        // startup. It stopped being true when the state folder could be changed
        // under a running window, and it became actively destructive once a
        // hand-over started switching the Saturn genuinely off (69c6345),
        // because from then on every calculator picked up from another machine
        // loads with a blank screen:
        //
        //   at startup      "loaded off" fell into the first branch, which
        //                   released the lock. Right for a calculator found
        //                   lying switched off; wrong for one just handed over.
        //   mid-session     m_sawFirstFrame was ALREADY true, so the load fell
        //                   through to the transition branch below and looked
        //                   exactly like the user pressing OFF - so handOver()
        //                   fired and SAVED and released a calculator we had
        //                   only just opened. That is both-04 line 8, "ram
        //                   keeps syncing forever": his log shows the state
        //                   folder change at 21:31:22.276 and "screen off"
        //                   106 ms later, with nobody having touched a key.
        //
        // So the question is not "have we ever drawn a frame", it is "is this
        // the frame that came with the load". One flag, set by start(), spent
        // here.
        if (m_freshLoad) {
            m_freshLoad = false;
            m_sawFirstFrame = true;
            m_displayOff = off;
            // A NEW CALCULATOR IS NOT A CALCULATOR SOMEBODY SWITCHED OFF.
            // Its first frame is blank because the ROM has not lit the display
            // yet, which on a cold boot takes longer than the first slice - and
            // reading that as "found off" detached the machine, released the
            // lock, and left the keyboard dead to everything except ON, which
            // then tried to reload a state file that had never been written.
            // A brand-new calculator could not be started at all: measured on
            // Gert's phone, 2026sep07, a fresh install sat on a blank green
            // screen through every key and two restarts, at zero CPU.
            //
            // Linux was winning the same race rather than avoiding it - by the
            // time it took its first frame the ROM had already switched the
            // display on. Nothing about the desktop made it safe.
            if (off && !m_bornEmpty) {
                // Found switched off, and nobody asked us to wake it: hold no
                // lock on a calculator we are not using. wakeAcquired() is what
                // clears m_freshLoad ahead of us when somebody DID ask.
                m_detached = true;
                m_state->release();
                emit detachedChanged();
            }
        } else if (off != m_displayOff) {
            m_displayOff = off;
            qWarning(off ? "screen off - the calculator has been switched off "
                           "(OFF is green-shift ON, and Ctrl is the green "
                           "shift). Press ON to switch it back on."
                         : "screen on");
            // Switching the calculator off is how you hand it to the other
            // machine: save it, then let go of the lock. Gert dismissed the
            // worry about hitting Ctrl+Esc by accident, and he is right - an
            // accidental release only costs anything if somebody also takes it
            // over by accident at the same moment, and Esc puts it straight
            // back.
            //
            // A load can no longer reach this branch at all - see m_freshLoad
            // above - so this is a real transition: the machine was on and now
            // it is not. Still not while we are part-way through switching it
            // on for the user, or attach() would hand back what it just took.
            if (off && !m_detached && m_tapQueue.isEmpty())
                handOver();
        }
        const int ann = m_frame.annunciators;
        if (ann != m_annunciators) {
            m_annunciators = ann;
            emit annunciatorsChanged();
        }
        emit frameReady();
    }

    if (m_sound) {
        quint32 hz = 0, ms = 0;
        if (x48_take_beep(&hz, &ms))
            emit beep(int(hz), int(ms));
    }

    // Keys held back by releaseCode let go here, once they have been down
    // long enough for the ROM to have scanned them.
    if (!m_releasePending.isEmpty()) {
        const qint64 now = m_clock.elapsed();
        for (auto it = m_releasePending.begin(); it != m_releasePending.end(); ) {
            const int id = *it;
            if (now - m_downAt.value(id, 0) >= kMinHoldMs) {
                finishRelease(id >> 16, id & 0xffff);
                it = m_releasePending.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Idle: drop to the slow tick while the Saturn is in SHUTDN, so a phone is
    // not spinning at 60 Hz to watch a sleeping calculator. Not while a release
    // is still owed, or the key would take an idle period to come up.
    //
    // This deliberately does not log. SHUTDN is where the 48 waits between one
    // keystroke and the next, so a line per transition was a line per key -
    // dogfood #12 line 14, "you can see the mess". What the log still records
    // is the screen going blank and coming back, which is a thing that happens
    // to the user rather than a thing the CPU does all day.
    // Keys Agape48 presses for the user - see queueTaps(). One at a time, and
    // only once the previous one has actually been let go, or the ROM's scan
    // sees them as a chord instead of a sequence.
    if (!m_tapQueue.isEmpty() && m_releasePending.isEmpty() && m_downAt.isEmpty()) {
        const QString k = m_tapQueue.takeFirst();
        pressKey(k);
        releaseKey(k);          // the 60 ms latch holds it down long enough
    }

    // A ROM that does not answer OFF must not leave the other machine waiting
    // out its ninety seconds while this window sits here still holding the lock.
    // It is not hypothetical: a brand-new calculator stops on "Try To Recover
    // Memory?" and no key gets past it, which is the one defect this program
    // still has. Give the keystroke a few seconds to do it properly, then hand
    // the calculator over the blunt way rather than not at all.
    if (!m_sleepFor.isEmpty() && !m_detached && m_sleepAskedAt != 0
        && m_clock.elapsed() - m_sleepAskedAt > kSleepOffGraceMs) {
        qWarning("the calculator did not switch itself off when %s asked for "
                 "it, so it was handed over without doing so",
                 qUtf8Printable(m_sleepFor));
        handOver();
    }

    const bool asleep = x48_is_asleep() && m_releasePending.isEmpty()
                                        && m_tapQueue.isEmpty();
    setTickRate(asleep ? kIdleIntervalMs : kTickIntervalMs);
}

// The timer never stops while the calculator is on; only its rate changes.
// The opposite of handOver(), and it exists because handOver() changed.
//
// Since a hand-over switches the Saturn OFF, every calculator that arrives by
// being asked for arrives with a blank screen - and two separate pieces of
// machinery read that blank frame as something it is not. The first-frame rule
// in tick() reads "loaded off" as "nobody is using this, let go of the lock",
// which is right for a calculator found lying switched off and exactly wrong
// for one we have just been handed: it released the lock it had spent ninety
// seconds waiting for. And switching calculators mid-session does not hit that
// rule at all, so it simply left a dead screen with nobody pressing ON.
//
// Gert, both-04 line 3: "The screen starts off, with 'The chosen memory is in
// use by another device.' ... It should turn on instead, right after taking
// over."
//
// So: the blank first frame is expected here rather than evidence of anything.
// Say so, and press ON. Same three flags attach() sets, for the same reason.
void Agape48Engine::wakeAcquired()
{
    m_freshLoad = false;        // the blank frame is expected, not a verdict
    m_detached = false;
    m_displayOff = true;
    m_sawFirstFrame = true;
    setError(QString());
    emit detachedChanged();
    queueTaps({ QStringLiteral("ON") });
}

// Save it, let go of it, and stop pretending it is ours. The one place that
// happens, whether the user switched the calculator off or another machine
// asked for it.
void Agape48Engine::handOver()
{
    saveState();
    m_state->release();
    m_detached = true;
    m_sleepAskedAt = 0;
    emit detachedChanged();
    if (!m_sleepFor.isEmpty()) {
        setError(tr("%1 asked for this calculator, so it was saved and handed "
                    "over. Press ON to ask for it back.").arg(m_sleepFor));
        m_sleepFor.clear();
    }
}

void Agape48Engine::setTickRate(int ms)
{
    if (m_tick.interval() != ms)
        m_tick.setInterval(ms);
    if (m_ready && !m_tick.isActive()) {
        m_tick.start();
        emit runningChanged();
    }
}

// --- keys -------------------------------------------------------------------

QString Agape48Engine::logPath() const
{
    // App storage, deliberately not the state folder: the state folder is the
    // one the user syncs, and a log is machine-local noise.
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
           + QLatin1String("/agape48.log");
}

void Agape48Engine::setLiveResize(bool on)
{
    if (m_liveResize == on)
        return;
    m_liveResize = on;
    QSettings().setValue(QLatin1String("window/liveResize"), on);
    emit liveResizeChanged();
}

void Agape48Engine::setDebugLogging(bool on)
{
    if (m_debugLogging == on)
        return;
    m_debugLogging = on;
    QSettings().setValue(QLatin1String("debug/logging"), on);

    if (on) {
        const QString path = logPath();
        QDir().mkpath(QFileInfo(path).absolutePath());
        auto *f = new QFile(path);
        if (f->open(QIODevice::Append | QIODevice::Text)) {
            g_logFile = f;
            g_previousHandler = qInstallMessageHandler(agape48LogHandler);
            qWarning("--- Agape48 %s log opened ---", AGAPE48_VERSION);
            logStartupFacts();
        } else {
            delete f;
            setError(tr("Cannot write the log at %1.").arg(path));
            m_debugLogging = false;
        }
    } else if (g_logFile) {
        qInstallMessageHandler(g_previousHandler);
        g_previousHandler = nullptr;
        g_logFile->close();
        delete g_logFile;
        g_logFile = nullptr;
    }
    emit debugLoggingChanged();
}

// The four facts that would have answered "why did it not start" on their own.
void Agape48Engine::logStartupFacts() const
{
    if (!m_debugLogging)
        return;
    const QString dir = m_state->location().toLocalFile();
    qWarning().noquote() << "state folder :" << dir;
    // The calculator, not just the shelf. With several on one shelf, "which
    // folder" stopped being enough to answer "which calculator did it open".
    qWarning().noquote() << "calculator   :" << m_state->instance();
    qWarning().noquote() << "its folder   :" << m_state->instanceDir();
    qWarning().noquote() << "rom source   :" << (m_romSource.isEmpty()
                                                 ? QStringLiteral("(none chosen)")
                                                 : m_romSource.toLocalFile());
    qWarning().noquote() << "rom beside   :"
                         << (QFileInfo::exists(dir + QLatin1String("/rom"))
                             ? QStringLiteral("present") : QStringLiteral("MISSING"));
    QStringList found;
    for (const QString &n : { QStringLiteral("rom"), QStringLiteral("ram"),
                              QStringLiteral("hp48"), QStringLiteral("port1"),
                              QStringLiteral("port2") })
        if (QFileInfo::exists(dir + QLatin1Char('/') + n))
            found << n;
    qWarning().noquote() << "files there  :"
                         << (found.isEmpty() ? QStringLiteral("(none)") : found.join(QLatin1String(", ")));
}

// Press keys on the user's behalf, one per tick-with-nothing-held.
//
// The ROM owns the screen and will not repaint the stack until it runs again,
// so an imported object sat there invisibly until the next keypress. Gert chose
// to have Agape48 press ON itself (dogfood #16): on the 48 that is also CANCEL,
// so a half-typed command line is lost, and he accepted that.
//
// His condition is the interesting half. ON with a shift active is not ON - it
// is OFF, printed right there on the key - so a latched shift has to be
// cancelled first. Pressing a shift key while it is active is what cancels it,
// and the annunciators are how we know one is: they come straight off the
// display register, so they are what the calculator itself thinks.
void Agape48Engine::queueTaps(const QStringList &keys)
{
    m_tapQueue += keys;
    setTickRate(kTickIntervalMs);
}

void Agape48Engine::shutdownCore()
{
    if (!m_ready)
        return;
    // saveState() rather than x48_save_state() + commit() directly, which is
    // what this did until 2026sep03. Going straight to the core skipped
    // saveState()'s own guard - "never write a calculator we do not hold" - and
    // a clean quit is the most ordinary action there is.
    //
    // MEASURED, on a shared shelf with the other machine holding the calculator:
    // a window that had already handed it over wrote its stale RAM back over the
    // new owner's work on close. The marker planted on disk, sha 03ee02fb..., was
    // replaced by this process's in-memory copy, sha 219c3880..., at 17:11:13.
    // Hand a calculator to the other machine, let it work, then close the window
    // you handed it from, and the other machine's work is gone. That is precisely
    // the clobber the lock exists to prevent, arrived at by the one path that did
    // not consult it.
    //
    // Two things follow for free: the RAM digest applies here too, so a quit that
    // would write identical bytes writes nothing at all; and commit() runs, so
    // the contents record beside the files describes the files rather than
    // trailing them.
    saveState();
    x48_shutdown();
    m_ready = false;
    m_tapQueue.clear();
    releaseAllKeys();
    m_tick.stop();
    emit readyChanged();
    emit runningChanged();
}

// --- asking another instance for a calculator -------------------------------

void Agape48Engine::askForCalculator(const QString &instance, bool takeWhenFree)
{
    const QString name = instance.isEmpty() ? m_state->instance() : instance;
    const QVariantMap holder = m_state->lockHolderOf(name);
    if (!m_state->requestSleep(name)) {
        setError(m_state->lastError());
        return;
    }
    const QString host = holder.value(QStringLiteral("host")).toString();
    if (!takeWhenFree) {
        // "Stop holding it" without "and give it to me": nothing to wait for,
        // and no request to withdraw when this window's dialog closes.
        emit notice(tr("Asked %1 to put %2 to sleep.")
                        .arg(host.isEmpty() ? tr("the other one") : host, name));
        return;
    }
    m_waitFor   = name;
    m_waitHost  = host;
    m_waitTake  = takeWhenFree;
    m_waitUntil = QDateTime::currentDateTimeUtc().addSecs(kSleepWaitSecs);
    m_waitSeconds = kSleepWaitSecs;
    m_waitWhy.clear();
    m_wait.start();
    emit waitSecondsChanged();
    emit waitingChanged();
}

bool Agape48Engine::takeOverCalculator(const QString &name)
{
    // Never read a folder we can SEE is half delivered, whichever button was
    // pressed to get here. Two of them offer this, and both-05 line 15 is what
    // pressing one of them costs: Gert took a calculator whose contents record
    // named a ram that had not arrived, and got the memory from before the
    // handover instead of the one he had waited a minute and a half for.
    //
    // Only bites while a request of OURS is outstanding for this calculator -
    // that is the one window in which a delivery is known to be in flight, and
    // handoverState() says Complete outside it. So it can never wedge somebody
    // out of a folder in the ordinary case, and "Stop waiting" is the way out
    // of the extraordinary one.
    const QString which = name.isEmpty() ? m_state->instance() : name;
    if (m_state->handoverState(which) == StateFileManager::Arriving) {
        setError(tr("%1's memory is still on its way. Taking it now would read "
                    "half of it.").arg(which));
        return false;
    }
    // Never started: there is nothing to attach TO. claim(true) first, because
    // start()'s own claim does not take over, and then start reads the files.
    if (!m_ready)
        return m_state->claim(true) && start();
    if (name.isEmpty() || name == m_state->instance())
        return attach(true);
    return openCalculator(name, true);
}

void Agape48Engine::stopWaiting()
{
    if (!m_wait.isActive())
        return;
    m_wait.stop();
    m_state->withdrawSleepRequest(m_waitFor);
    m_waitFor.clear();
    m_waitSeconds = 0;
    m_waitWhy.clear();
    emit waitSecondsChanged();
    emit waitingChanged();
}

// Is anyone else holding the memory we are pointed at? isHeldBySomebody() is
// the same test the shelf and the take-over dialog make: a lock file that is
// present, is not ours, and either names another machine or names a process on
// this one that is still alive.
//
// A lock left behind by a machine that was switched off stays on disk for ever
// and still reads as held. That is deliberate here - Gert, 2026sep04: "If
// another host died and left the lock on, keep showing the locked screen as is
// with the phrase on it." An offline machine and a dead one are the same file,
// so the screen tells you what is written rather than guessing which it was.
void Agape48Engine::pollLockHolder()
{
    const bool held = m_state->isHeldBySomebody(m_state->instance());
    if (held == m_heldElsewhere)
        return;
    m_heldElsewhere = held;
    emit memoryHeldElsewhereChanged();
}

void Agape48Engine::pollForRelease()
{
    if (m_waitFor.isEmpty()) {
        m_wait.stop();
        return;
    }
    const int left = int(QDateTime::currentDateTimeUtc().secsTo(m_waitUntil));
    if (left != m_waitSeconds) {
        m_waitSeconds = left < 0 ? 0 : left;
        emit waitSecondsChanged();
    }
    // Not "has it been let go of" - "has it ARRIVED". The lock is 96 bytes and
    // the memory beside it is 131,072, so through a sync client the lock's
    // deletion lands first and the folder is still half the previous
    // calculator. That is dogfood both-03 line 19, and the "External" object on
    // Gert's stack in line 8 is what reading it looked like.
    const bool held = m_state->isHeldBySomebody(m_waitFor);
    const StateFileManager::HandoverState arrival = m_state->handoverState(m_waitFor);
    if (!held && arrival == StateFileManager::Complete) {
        const QString name = m_waitFor;
        m_wait.stop();
        m_state->withdrawSleepRequest(name);   // answered; the question can go
        m_waitFor.clear();
        m_waitSeconds = 0;
        m_waitWhy.clear();
        emit waitSecondsChanged();
        emit waitingChanged();
        if (m_waitTake) {
            // Three ways in, depending on why we did not have it. Never
            // started (the calculator was busy when this window opened): just
            // start. Ours but handed over: ON. Somebody else's: open it.
            bool got;
            if (!m_ready) {
                // Never started - the calculator was busy when this window
                // opened, which is the commonest way in and the one both-04
                // line 3 came through. start() alone would load the machine
                // the other side just switched off and leave it dark.
                got = start();
                if (got)
                    wakeAcquired();
            } else if (name == m_state->instance()) {
                got = attach();                 // wakes it itself
            } else {
                got = openCalculator(name);     // wakes it itself
            }
            if (!got) {
                // Somebody else was waiting too and was quicker. Back to the
                // dialog with whoever holds it now, rather than a silent close.
                emit attachRefused(m_state->lockHolderOf(name));
                return;
            }
        }
        emit otherLetGo(name);
        return;
    }
    if (QDateTime::currentDateTimeUtc() < m_waitUntil)
        return;

    // Past the deadline, and the wait does NOT end here. That it used to is the
    // whole of both-05 line 15.
    //
    // MEASURED, from the two machines' own timestamps. The countdown ran out at
    // 21:32:2x with the other machine still holding the lock, so "has not
    // answered" was true when it was painted. At 21:33:59 that machine saved
    // and let go, at 21:34:07 the release landed here - and the dialog said the
    // same thing at 21:56, twenty-four minutes later, because nothing was left
    // running to repaint it. "Take it over" was still the first button, for a
    // folder whose ram was by then a whole session out of date.
    //
    // So the timer keeps going and the reason is re-tested every second. The
    // request is not withdrawn either: we ARE still asking, and withdrawing it
    // would also clear the expectation that lets handoverState() tell a
    // half-delivered folder from a whole one. Both end together, in
    // stopWaiting(), which is what every way out of the dialog calls.
    //
    // This does not introduce a wait with no end. The dialog was already up for
    // ever with no end; it is merely alive now instead of dead.
    const QString why = held ? QStringLiteral("held")
                      : arrival == StateFileManager::Arriving
                            ? QStringLiteral("arriving")
                            : QStringLiteral("letgo");
    if (why == m_waitWhy)
        return;
    m_waitWhy = why;
    emit sleepUnanswered(m_waitFor, m_waitHost, why);
}

// Take the calculator back. Claim first, then read what is on disk - the whole
// point of handing it over is that somebody else may have used it since, and
// theirs is the version that counts.
bool Agape48Engine::attach(bool takeOver)
{
    if (!m_detached)
        return true;
    if (!m_state->claim(takeOver)) {
        emit attachRefused(m_state->lockHolder());
        setError(m_state->lastError());
        return false;
    }
    if (!reloadState()) {
        // The files are there and unreadable, which is worth saying plainly
        // rather than leaving a blank calculator and no reason.
        m_state->release();
        emit attachRefused(m_state->lockHolder());
        return false;
    }
    // The calculator we just read is the one somebody switched off, so it is
    // still off - reloading does not turn it on. Press ON for real: it wakes
    // the machine and makes the ROM repaint, which is the same thing an import
    // needs and the same queue does it.
    wakeAcquired();
    return true;
}

bool Agape48Engine::openCalculator(const QString &name, bool takeOver)
{
    if (name.isEmpty() || name == m_state->instance())
        return true;
    shutdownCore();                       // saves into the folder we are leaving
    if (!m_state->openInstance(name, takeOver)) {
        setError(m_state->lastError());
        start();                          // openInstance put the old one back
        return false;
    }
    if (!start())
        return false;
    // Picking a calculator off the shelf is asking for it, so wake it. Every
    // calculator on a shared shelf is now saved switched off, because that is
    // what handing one over does - without this the shelf hands back a dead
    // screen and waits for a keypress nobody knows to make.
    wakeAcquired();
    return true;
}

QString Agape48Engine::newCalculator()
{
    shutdownCore();
    const QString made = m_state->createInstance();
    if (made.isEmpty())
        setError(m_state->lastError());
    start();
    return made;
}

// Renaming the calculator we have OPEN is not a folder operation, whatever it
// looks like from the shelf.
//
// cfg.state_dir is filled once, by populate() inside start(), and the C core
// keeps that path for the life of the session. Move the folder underneath it
// and the next save - a lost focus, a quit, opening another calculator - writes
// ram and hp48 back to the path the core was born with, MAKING THE FOLDER AGAIN
// under its old name. Gert, both-05 line 37: "there were not two, but 3:
// 'Windows Box', 'Linux Box' and 'Calculator 1' ... It seems the rename erased
// the files from the folder upon renaming it, but not every time."
//
// Nothing was erased. The renamed folder held whatever was on disk at the
// moment it moved, and the memory went on being saved beside it under the dead
// name. "Not every time" is simply whether a save happened afterwards.
//
// So the core goes down before the folder moves and comes back up after, which
// is what openCalculator() already does for the same reason. Renaming one we do
// not have open never involved the core at all.
bool Agape48Engine::renameCalculator(const QString &name, const QString &to)
{
    if (name != m_state->instance()) {
        if (m_state->renameInstance(name, to))
            return true;
        setError(m_state->lastError());
        return false;
    }
    const bool wasRunning = m_ready;
    shutdownCore();                       // saves into the folder that is moving
    const bool ok = m_state->renameInstance(name, to);
    if (!ok)
        setError(m_state->lastError());
    if (wasRunning)
        start();                          // reads from wherever it is now
    return ok;
}

bool Agape48Engine::hasStackObject() const
{
    return m_ready && x48_stack_has_object();
}

void Agape48Engine::clearError()
{
    setError(QString());
}

bool Agape48Engine::importFile(const QUrl &url)
{
    if (!m_ready) {
        setError(tr("The calculator is not running."));
        return false;
    }
    QString path = url.toLocalFile();
    QString scratch;
    if (path.isEmpty()) {
        scratch = transferScratchPath();
        if (!copyBytes(url.toString(), scratch)) {
            QFile::remove(scratch);
            setError(tr("Could not read that file."));
            return false;
        }
        path = scratch;
    }
    // Safe to reach into the Saturn's memory from here: emulation runs on this
    // thread from a timer, so a menu handler is always between two slices and
    // never inside one.
    const bool imported = x48_import_file(path.toUtf8().constData());
    if (!scratch.isEmpty())
        QFile::remove(scratch);
    if (!imported) {
        setError(QString::fromUtf8(x48_last_error()));
        return false;
    }
    setError(QString());

    QStringList seq;
    if (m_annunciators & X48_ANN_RIGHT) seq << QStringLiteral("SHR");
    if (m_annunciators & X48_ANN_LEFT)  seq << QStringLiteral("SHL");
    seq << QStringLiteral("ON");
    queueTaps(seq);
    return true;
}

bool Agape48Engine::exportFile(const QUrl &url)
{
    if (!m_ready) {
        setError(tr("The calculator is not running."));
        return false;
    }
    QString path = url.toLocalFile();
    QString scratch;
    if (path.isEmpty()) {
        scratch = transferScratchPath();
        path = scratch;
    }
    if (!x48_export_file(path.toUtf8().constData())) {
        if (!scratch.isEmpty())
            QFile::remove(scratch);
        setError(QString::fromUtf8(x48_last_error()));
        return false;
    }
    if (!scratch.isEmpty()) {
        // The document already EXISTS by now - Android creates it when the user
        // names it, before we are asked to write anything - so failing here
        // leaves a real, empty file behind with the user's chosen name on it.
        // Measured on Gert's phone before the fix: "test123.hpp, 0 B".
        const bool copied = copyBytes(scratch, url.toString());
        QFile::remove(scratch);
        if (!copied) {
            setError(tr("Could not write to that file."));
            return false;
        }
    }
    setError(QString());
    return true;
}

QUrl Agape48Engine::pathToUrl(const QString &path) const
{
    const QString t = path.trimmed();
    if (t.isEmpty())
        return QUrl();

    // Already a URL? A content:// tree from Android's file picker, or a
    // file:// somebody pasted. The length test is the whole point: on Windows
    // QUrl("C:/Users/gert").scheme() is "c", so asking "does it have a scheme"
    // calls every drive-lettered path a URL and hands it straight back
    // unconverted. No real scheme is one character.
    const QUrl asUrl(t);
    if (asUrl.isValid() && asUrl.scheme().size() > 1)
        return asUrl;

    return QUrl::fromLocalFile(QDir::fromNativeSeparators(t));
}

QString Agape48Engine::urlToPath(const QUrl &url) const
{
    if (url.isEmpty())
        return QString();
    if (url.isLocalFile())
        return QDir::toNativeSeparators(url.toLocalFile());
    return url.toString();      // content:// has no path to show
}

bool Agape48Engine::startSystemMove(QQuickWindow *window)
{
    return window && window->startSystemMove();
}

void Agape48Engine::setWindowGeometry(QQuickWindow *window, int x, int y, int w, int h)
{
    if (window)
        window->setGeometry(x, y, w, h);
}

int Agape48Engine::keyboardModifiers() const
{
    return int(QGuiApplication::queryKeyboardModifiers());
}

bool Agape48Engine::lookupKey(const QString &keyId, int *row, int *mask) const
{
    const auto it = keyTable().constFind(keyId);
    if (it == keyTable().cend())
        return false;
    if (it->first == kUnmapped)
        return false;
    *row = it->first;
    *mask = it->second;
    return true;
}

void Agape48Engine::pressKey(const QString &keyId)
{
    // Detached: the calculator is off and belongs to nobody, so the keyboard
    // does nothing until ON takes it back. That is also what a real 48 does -
    // when it is off, only ON is listened to.
    if (m_detached) {
        if (keyId == QLatin1String("ON"))
            attach();
        return;
    }

    int row = 0, mask = 0;
    if (!lookupKey(keyId, &row, &mask)) {
        setError(tr("Key \"%1\" has no matrix code yet "
                    "(see kKeys in Agape48Engine.cpp).").arg(keyId));
        return;
    }
    pressCode(row, mask);
    emit keyFeedback(keyId);
}

void Agape48Engine::releaseKey(const QString &keyId)
{
    int row = 0, mask = 0;
    if (lookupKey(keyId, &row, &mask))
        releaseCode(row, mask);
}

void Agape48Engine::markPressed(int row, int mask, bool down)
{
    const QString id = codeTable().value((row << 16) | mask);
    if (id.isEmpty() || m_pressed.contains(id) == down)
        return;
    if (down)
        m_pressed.append(id);
    else
        m_pressed.removeAll(id);
    emit pressedKeysChanged();
}

void Agape48Engine::pressCode(int row, int mask)
{
    if (row < 0 || row >= X48_KB_ROWS)
        return;
    const int id = (row << 16) | (mask & 0xffff);
    m_downAt[id] = m_clock.elapsed();
    m_releasePending.remove(id);

    x48_key_down(row, quint16(mask));
    markPressed(row, mask, true);
    setTickRate(kTickIntervalMs);          // straight back to full speed
}

void Agape48Engine::finishRelease(int row, int mask)
{
    x48_key_up(row, quint16(mask));
    markPressed(row, mask, false);
    m_downAt.remove((row << 16) | (mask & 0xffff));
}

void Agape48Engine::releaseCode(int row, int mask)
{
    if (row < 0 || row >= X48_KB_ROWS)
        return;
    const int id = (row << 16) | (mask & 0xffff);
    if (m_downAt.contains(id) && m_clock.elapsed() - m_downAt.value(id) < kMinHoldMs) {
        // Too soon. Hold it down and let tick() let go once the ROM has had
        // its chance to notice.
        m_releasePending.insert(id);
        setTickRate(kTickIntervalMs);
        return;
    }
    finishRelease(row, mask);
}

void Agape48Engine::releaseAllKeys()
{
    m_downAt.clear();
    m_releasePending.clear();
    x48_key_release_all();
    if (!m_pressed.isEmpty()) {
        m_pressed.clear();
        emit pressedKeysChanged();
    }
}

// --- state ------------------------------------------------------------------

void Agape48Engine::reset(bool cold)
{
    x48_reset(cold);
    setTickRate(kTickIntervalMs);
}

bool Agape48Engine::saveState()
{
    if (!m_ready)
        return false;
    // Never write a calculator we do not hold. Losing focus saves, and a window
    // whose calculator was taken over is still a running window - without this
    // it would write its stale memory over the top of whoever now owns the
    // folder, which is the one thing the lock exists to prevent. Detaching is
    // deliberately silent about it: the save that matters already happened when
    // the calculator was switched off.
    if (!m_state->isHeld() && m_state->location().isLocalFile())
        return false;
    // Main.qml's onActiveChanged calls suspend(), which is stop() + this, so
    // every alt-tab writes 131,072 bytes of RAM into what is usually a synced
    // folder. Dogfood both-03 line 8: "the Dropbox update is taking forever, as
    // if the other calculator in Linux keeps touching the memory files ...
    // Should Agape48 refrain from changing the RAM files while nothing changes
    // on the calculator's stack?" It should. Measured on his machine: the log's
    // "emulation stopped" at 15:23:05.022, ram's mtime 15:23:05.025.
    //
    // Nothing is committed on the skip, deliberately: we did not write, so the
    // stamp the watcher already holds still describes what is on disk. Setting
    // it here would be claiming a write that did not happen.
    const quint64 digest = x48_ram_digest();
    if (digest != 0 && digest == m_savedRamDigest)
        return true;
    if (!x48_save_state()) {
        setError(QString::fromUtf8(x48_last_error()));
        return false;
    }
    m_savedRamDigest = digest;
    return m_state->commit(x48_state_fingerprint());
}

bool Agape48Engine::reloadState()
{
    if (!m_ready)
        return false;
    if (!x48_reload_state()) {
        setError(QString::fromUtf8(x48_last_error()));
        return false;
    }
    // NOT m_frameSerial = 0. LcdItem skips a frame whose serial it has already
    // drawn, so a counter that restarts hands it a number it has seen: after a
    // reload the first frame of the NEW calculator was dropped as a duplicate,
    // and since a machine loaded in SHUTDN draws nothing more, the window went
    // on showing the PREVIOUS calculator's screen until a key was pressed.
    // Somebody else's stack on your screen, with your keys underneath it.
    // The counter is monotonic for the life of the process now.
    // What we just read IS the disk, so it is the new baseline. Without this
    // the watcher still holds the pre-reload stamps and reports the change we
    // have already acted on, which would put the calculator straight back to
    // sleep after every ON.
    m_state->noteStateOnDisk();
    // Forget what we last wrote. Only a save we actually performed may set this,
    // never a load: baselining it from freshly-read memory would let a
    // calculator whose "ram" file does not exist yet - a new one - skip its
    // first save and never create the file at all. One redundant write after a
    // reload is the cheap side of that trade.
    m_savedRamDigest = 0;
    return true;
}

// --- clipboard --------------------------------------------------------------

QString Agape48Engine::copyStackToClipboard()
{
    QByteArray buf(512, Qt::Uninitialized);
    size_t need = x48_stack_to_text(buf.data(), size_t(buf.size()));
    if (need == 0) {
        setError(QString::fromUtf8(x48_last_error()));
        return {};
    }
    if (need > size_t(buf.size())) {          // retry once with the real size
        buf.resize(int(need));
        need = x48_stack_to_text(buf.data(), size_t(buf.size()));
        if (need == 0 || need > size_t(buf.size())) {
            setError(QString::fromUtf8(x48_last_error()));
            return {};
        }
    }
    // need counts the terminator the core writes; the string does not want it.
    buf.truncate(int(need) - 1);
    const QString text = QString::fromUtf8(buf);
    QGuiApplication::clipboard()->setText(text);
    return text;
}

bool Agape48Engine::pasteClipboardToStack()
{
    const QString text = QGuiApplication::clipboard()->text();
    if (text.isEmpty())
        return false;
    if (!x48_text_to_stack(text.toUtf8().constData())) {
        // The core's own sentence, not a summary of it: it knows whether the
        // text was too long, held a character it cannot translate, or would
        // not fit in the calculator's memory.
        setError(QString::fromUtf8(x48_last_error()));
        return false;
    }
    setError(QString());
    setTickRate(kTickIntervalMs);
    // THE SAME NUDGE IMPORT NEEDS, and leaving it out is why Paste looked like
    // it did nothing at all: the object was pushed and the stack on the glass
    // went on showing what it showed before, because the ROM redraws when
    // something happens to the machine and nothing had. Measured on the phone
    // on 2026sep09 - 3.14158 copied, dropped, pasted, and the display still
    // read what was under it. ON is also CANCEL, so any latched shift comes off
    // first or ON would be OFF.
    QStringList seq;
    if (m_annunciators & X48_ANN_RIGHT) seq << QStringLiteral("SHR");
    if (m_annunciators & X48_ANN_LEFT)  seq << QStringLiteral("SHL");
    seq << QStringLiteral("ON");
    queueTaps(seq);
    return true;
}

// --- misc -------------------------------------------------------------------

void Agape48Engine::setRomSource(const QUrl &url)
{
    if (m_romSource == url)
        return;
    m_romSource = url;
    QSettings().setValue(QLatin1String("rom/source"), url.toString());
    emit romSourceChanged();
}

void Agape48Engine::setHapticsEnabled(bool on)
{
    if (m_haptics == on) return;
    m_haptics = on;
    emit hapticsEnabledChanged();
}

void Agape48Engine::setSoundEnabled(bool on)
{
    if (m_sound == on) return;
    m_sound = on;
    emit soundEnabledChanged();
}

void Agape48Engine::setError(const QString &what)
{
    if (m_lastError == what)
        return;
    m_lastError = what;
    emit lastErrorChanged();
}
