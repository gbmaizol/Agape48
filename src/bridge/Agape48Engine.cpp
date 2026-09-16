#include "Agape48Engine.h"

#include "agape48_build.h"

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QtCore/qcoreapplication_platform.h>
#endif

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

#include <memory>

namespace {

// Emulation pacing. The HP 48 Saturn runs at ~4 MHz (48G) / ~2 MHz (48S), so a
// 60 Hz tick is roughly 70000 cycles. Ticking on the GUI thread is deliberate:
// a 4 MHz interpreter costs low single-digit percent of one modern core, and a
// worker thread would buy nothing but a frame-handoff race. If profiling ever
// says otherwise, this is the one place that has to move.
constexpr int  kTickIntervalMs = 16;

// La aŭtomata →STR / STR→: kiom longe la programo rajtas resti neruligita sur
// nivelo 1 post la klavoj, kaj kiom longe la gardado daŭras entute.
constexpr qint64 kAutoStartMs   = 1000;
constexpr qint64 kAutoTimeoutMs = 10000;

// La plej granda tekstdosiero, kiun demeto legas por decidi. La RAM de GX havas
// 128 KB, do ĉeno pli granda ol tio neniam trovus lokon.
constexpr qint64 kDropTextMaxBytes = 256 * 1024;

// How long a key is guaranteed to stay down. The HP 48's ROM polls the matrix
// on its own schedule - about 40 ms between scans - so a press and release
// inside one gap is never seen at all. Dogfood #11 line 2: pressing Esc did
// not switch the calculator back on, because from SHUTDN the scan only begins
// after the key arrives, and a tap was over before it got there.
constexpr int  kMinHoldMs      = 60;
constexpr int  kCyclesPerTick  = 70000;
constexpr int  kIdleIntervalMs = 100;

// REAL HP 48 SPEED, and the number is triangulated rather than picked. 2026sep09,
// on a game: the emulator was running at five times the speed it should be.
//
// Free-running, this frontend delivers kCyclesPerTick every kTickIntervalMs =
// 4,375,000 instructions a second, so that factor of five puts a real machine at
// about 875,000. TWO INDEPENDENT THINGS AGREE with that: 875,000 times the
// Saturn's ~4.2 cycles per instruction is 3.68 MHz, which is the 48GX's clock,
// and x48_shim.c:327 already guessed "roughly 17000 of these" for a 60 Hz tick,
// which is 1.06 M/s - the same number to twenty percent, and never once
// reconciled with the 70000 two lines above it.
//
// THE TWO CONSTANTS INSIDE X48 DISAGREE, with each other and with both of
// those: the timer fallback at emulate.c:2376 assumes 8192 instructions per
// 1/16 s, which is 131,072/s, and the dead throttle at emulate.c:2466 busy-waits
// 2 us each, which is 500,000/s. Neither is authoritative - which is exactly why
// this one is derived from something observed on a real game instead of chosen
// from the source.
// 205,000, MEASURED, and every earlier number in this comment's history was
// wrong because it was taken through a ruler that was broken.
//
// The reference is Emu48 with Authentic Calculator Speed on: a 500-iteration
// empty loop takes 1.30615234375 s, identical to the last digit on four
// consecutive runs. Under Agape48 paced at 500,000, the same loop took 0.53588 s
// - the mean of 186 samples with a standard deviation of 4.6%, out of 200 run in
// one go on 2026sep10. So we were 2.437x too fast, and 500,000 / 2.437 is the
// number above. (The other 14 samples of the 200 each spanned a moment when the
// window lost focus, which stops the emulator outright - see Main.qml's
// onActiveChanged - and they read 1.7 s to 27 s. Discarding them is not
// cherry-picking: they measure a hand at the keyboard, not the calculator.)
//
// A SECOND MEASUREMENT, sharing nothing with the first, lands on a textbook
// constant. speed-probe.txt counted 46,106,722 instructions across 142 of those
// samples, which is 324,695 instructions per sample; over Emu48's 1.30615 s that
// makes a real 48 execute 248,589 instructions a second. Emu48 stores
// GXCycles = 123 cycles per timer2 tick, so 123 x 8192 = 1,007,616 cycles a
// second, and 1007616 / 248589 = 4.05 CYCLES PER INSTRUCTION - the canonical
// Saturn average. Two unrelated routes agreeing on 4 cycles is the only
// corroboration this constant has ever had.
//
// The two numbers differ - 248,589 executed against 205,000 asked for - because
// the pacer overshoots its target by about a fifth on this laptop. 205,000 is
// therefore the SETTING that produces authentic speed here, not the machine's
// instruction rate; per-machine is also why speed/rate is persisted.
//
// The estimates this replaces, and why each failed: 875,000 came from the naked
// eye - five times too fast - against the free-running
// 4,375,000; 488,446 came from 49 runs whose median went through a t2_tick that
// was being ratcheted from 61 to 3811 by get_t1_t2(); and 500,000 was x48's own
// dead busy-wait of 2 us per instruction, a constant in code this build never
// calls. x48's other internal figure, the timer fallback's 8192 instructions per
// 1/16 s = 131,072/s, is 1.6x low.
constexpr int    kRealSpeedInstrPerSec = 205000;

// The most time one slice may make up. A stall - the phone backgrounding us, a
// long frame, waking out of the 100 ms idle tick - must not hand the Saturn a
// burst and make a game jump; two ticks' worth is enough to ride out a late
// timer and small enough that nothing visible can accumulate behind it.

// What the rate may be set to. The ceiling is the free-running rate itself -
// past that the throttle would be asking for more instructions than the tick
// can deliver and would silently do nothing - and the floor is low enough to be
// obviously wrong on screen rather than to look like a hang.
constexpr int    kRateFloor  = 50000;
// The far left of the speed slider. A sluggish 0.1 is a toy setting, and there
// is no lower bound worth defending below it, because a tenth
// of a real 48 is already slower than anything anybody would sit through.
constexpr double kFactorFloor = 0.1;
constexpr int    kRateCeiling = kCyclesPerTick * 1000 / kTickIntervalMs;

// The most owed time a single slice may make up. This is a STALL threshold, not
// a smoothing knob: past it the Saturn genuinely loses time, so it has to be far
// longer than any ordinary late tick. 32 ms was the first value and it was much
// too tight - see realSpeedBudget().
constexpr qint64 kPaceMaxOwedUs = 250000;

// And the most one slice may actually RUN, as a multiple of a tick's worth. The
// debt above this stays owed and is paid off over the ticks that follow, so
// nothing is lost and no single slice can burst.
constexpr int    kPaceMaxSliceTicks = 4;

// How often the speedometer is read. x48 resamples eight times a second, so
// anything faster reads the same number twice; this is about twice a second.
constexpr int    kRateSampleTicks = 32;

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
// parked until the next key press happened to shake it loose, which is why Esc
// took three taps, and the same reason clicking the title bar appeared to
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
// option is on. It exists because of dogfood #8, where the only record of why
// the app would not start was a red banner hidden behind a window.
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
// that refusal covered every file there was: on the phone, putting a file on the
// stack failed with "it can only read a file in this computer".
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

// --- the calculator's settings, as opposed to this computer's ---------------
//
// settings.ini beside the ROM, so a folder carried to another machine carries
// its keymap and its preferences with it. What does NOT go in there is anything
// that describes the machine: the window's geometry, the live-resize switch, and
// above all speed/rate, which is a CALIBRATION - 205000 instructions a second is
// what this laptop measured against a real 48, and another computer's number
// will be different. Carrying that one across would make the calculator run at
// the wrong speed on arrival, which is exactly the bug the whole speed
// investigation of 2026sep09 turned out to be.
std::unique_ptr<QSettings> calcSettings(StateFileManager *st)
{
    const QString path = st ? st->settingsFile().toLocalFile() : QString();
    if (path.isEmpty())
        return std::make_unique<QSettings>();
    return std::make_unique<QSettings>(path, QSettings::IniFormat);
}

// FIRST RUN AFTER THE MOVE, and every first run in a NEW folder. These values
// were machine-local until 2026sep10, so read the old place when the folder has
// nothing to say - otherwise everyone's preferences reset themselves on upgrade.
//
// Deliberately NOT a one-shot copy, because the same fallback answers a second
// question: what a folder that has never had settings of its own should start
// from. The machine's old values, not the factory defaults - so making a new
// shelf does not hand you a calculator with nothing set up. The first change
// writes it into the folder, and from then on the folder is the answer.
QVariant carried(QSettings *now, const char *key, const QVariant &def)
{
    const QString k = QLatin1String(key);
    return now->contains(k) ? now->value(k) : QSettings().value(k, def);
}

} // namespace

void Agape48Engine::loadCalcSettings()
{
    const auto s = calcSettings(m_state);
    m_runUnfocused = carried(s.get(), "window/runUnfocused", false).toBool();
    m_realSpeed    = carried(s.get(), "speed/real", false).toBool();
    m_autoToStr    = carried(s.get(), "clipboard/autoToStr", false).toBool();
    m_autoStrTo    = carried(s.get(), "clipboard/autoStrTo", false).toBool();
    // Clamped against the rate in force, because the far right of the slider is
    // a function of it. A factor stored where the calibration was different must
    // not push the budget past the ceiling.
    m_speedFactor  = qBound(kFactorFloor,
                            carried(s.get(), "speed/factor", 1.0).toDouble(),
                            speedFactorMax());
}

Agape48Engine::Agape48Engine(QObject *parent)
    : QObject(parent)
    , m_state(new StateFileManager(this))
    , m_skin(new SkinModel(this))
{
    if (QSettings().value(QLatin1String("debug/logging"), false).toBool())
        setDebugLogging(true);

    // Machine-local, both of them: a workaround for a slow compositor and a
    // measurement of this computer. See the note on calcSettings().
    m_liveResize = QSettings().value(QLatin1String("window/liveResize"), false).toBool();
    m_realSpeedRate = qBound(kRateFloor,
                             QSettings().value(QLatin1String("speed/rate"),
                                               kRealSpeedInstrPerSec).toInt(),
                             kRateCeiling);
    // The rest belong to the calculator and are read from its folder. m_state
    // already knows where that is: its own constructor loads the location.
    loadCalcSettings();

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
        // A different folder is a different calculator, with its own keymap and
        // its own preferences. Read them before anything runs on them, and tell
        // QML, which is showing the old ones.
        loadCalcSettings();
        emit runUnfocusedChanged();
        emit realSpeedChanged();
        emit autoToStrChanged();
        emit autoStrToChanged();
        emit speedFactorChanged();
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
    // 2026sep03: handing over runs the calculator's auto-sleep, which has to
    // have the same effect as pressing the green shift followed by ON.
    //
    // That is not only tidiness. Until now this saved and let go
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

    // The files changed underneath us, and the rule for that is the right one:
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
    // MEASURED on the phone, 2026sep07: cold-boot a new calculator, answer
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
        //
        // KAJ DE KIE PRENI ĜIN, ekde provo 17, kiam freŝa instalo alvenis al ĉi
        // tiu strio kaj al nenio alia: "What if the red message says something
        // in the lines of 'Download an official one at
        // https://www.hpcalc.org/hp48/pc/emulators/ ctrl-f to search for "HP 48GX
        // Revision"?'" Ĝi estas la sola ekrano kiun homo sen ROM certe vidos, kaj
        // ĝi estis la sola loko kiu sciis pri la problemo kaj diris nenion pri la
        // solvo. La serĉĉeno estas laŭvorte kion oni tajpas en Ctrl-F sur tiu
        // paĝo: ĝi kaŝas dek unu ROM-ojn inter multe da alia.
        setError(tr("No HP 48 ROM. There is no file named \"rom\" in %1, and "
                    "none has been chosen in Settings.\n"
                    "A free one: open https://www.hpcalc.org/hp48/pc/emulators/ "
                    "and search the page for \"HP 48GX Revision\". Download "
                    "gxrom-r.zip, unzip it, and choose the 524,288-byte file "
                    "called gxrom-r that comes out - or rename it to \"rom\" and "
                    "drop it in the folder above.")
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
        // LA ELEKTITA DOKUMENTO VENKAS TIUN KIU JAM KUŜAS TIE. Ĝis 2026sep16 la
        // kopio okazis nur kiam neniu "rom" ekzistis apud la stato, do sur breto
        // kiu jam havis unu la elekto estis silente forĵetita: la kampo montris
        // la elektitan dokumenton, la kerno legis la malnovan dosieron, kaj
        // nenio diris tion. Mezurite sur la telefono: ROM de 48SX elektita per
        // la elektilo, kaj la kalkulilo plu estis 48GX, kun la sama md5 kiel la
        // antaŭa dosiero.
        //
        // Skribita apuden kaj poste renomita, ĉar la kopio nun anstataŭigas ion
        // kio funkciis: kopio kiu malsukcesas duonvoje rajtas perdi nenion krom
        // si mem.
        const QString half = dest + QLatin1String(".nova");
        QFile in(m_romSource.toString());
        QFile out(half);
        if (!in.open(QIODevice::ReadOnly)
            || !out.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || out.write(in.readAll()) <= 0) {
            out.remove();
            setError(tr("Could not copy the chosen ROM into %1.")
                         .arg(m_state->location().toLocalFile()));
            return false;
        }
        out.close();
        QFile::remove(dest);
        if (!QFile::rename(half, dest)) {
            QFile::remove(half);
            setError(tr("Could not copy the chosen ROM into %1.")
                         .arg(m_state->location().toLocalFile()));
            return false;
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
    // La dosiero kiun la kerno ĵus malfermis, konservita kiel loka vojo por ke
    // Agordoj povu montri ĝin. La kampo bezonas la ŝargitan ROM-on, ne la
    // peton: elekto kiu ne ekfunkciis lasas romSource montranta al dosiero kiun
    // neniu kalkulilo uzas.
    m_romLoadedPath = QDir::toNativeSeparators(m_romSource.toLocalFile());
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
    // THE SAVE HAPPENS EITHER WAY, and only the stop is optional. Losing focus
    // is what writes the state folder, it is how the hand-over between the two
    // laptops stays safe, and it is how every measurement in the speed work was
    // read off disk at all - so a switch that skipped it would break something
    // load-bearing to fix something cosmetic.
    if (!m_runUnfocused)
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
    ++m_tickCount;
    // OFF IS THE UNTOUCHED PATH. Not one clock read, not one branch taken
    // beyond this ternary, because "normally we want the calculator to run as
    // fast as it can" and a throttle that costs something when it is off is a
    // throttle nobody would leave off.
    x48_run_slice(m_realSpeed ? realSpeedBudget() : kCyclesPerTick);

    // Read the speedometer in BOTH modes, because the free-running rate is the
    // reference the throttled one gets calibrated against. Twice a second, one
    // long and one comparison, which is not a cost by any measure.
    if (++m_rateSample >= kRateSampleTicks) {
        m_rateSample = 0;
        const int rate = int(x48_instructions_per_second());
        if (rate != m_measuredRate) {
            m_measuredRate = rate;
            emit measuredRateChanged();
        }
    }

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
        //                   keeps syncing forever": the log shows the state
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
            // the phone, 2026sep07, a fresh install sat on a blank green
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
            // machine: save it, then let go of the lock. The worry about
            // hitting Ctrl+Esc by accident does not survive inspection - an
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
    if (m_autoStep != 0)
        stepAuto();
    // Demeto venas el alia fenestro, kiu tenas la fokuson, do la kalkulilo estis
    // haltigita kiam ĝi alvenis. La demeto rekomencigis la takton por siaj klavoj
    // kaj por la ROM; kiam ĉio finiĝis, la paŭzo de nefokusita fenestro revenas
    // per la kutima vojo, kun konservo.
    if (m_dropWoke && m_autoStep == 0 && m_tapQueue.isEmpty()
        && m_releasePending.isEmpty() && m_downAt.isEmpty() && x48_is_asleep()) {
        m_dropWoke = false;
        if (!m_runUnfocused && QGuiApplication::applicationState() != Qt::ApplicationActive)
            suspend();
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
// both-04 line 3: the screen started off, carrying "The chosen memory is in use
// by another device." It turns on instead, right after taking over.
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
    // NO PACE RESET HERE. It used to set m_paceAt = 0 on every change, and the
    // first slice after a reset hands out a whole tick's worth whatever the
    // clock says - so every flip between the 16 ms run tick and the 100 ms idle
    // tick was free instructions. The 48 drops into SHUTDN between key scans by
    // design, so that flip happens constantly and the machine ran above its
    // target by an amount nobody could predict. The elapsed-time arithmetic in
    // realSpeedBudget() already handles a longer interval correctly.
    if (m_tick.interval() != ms)
        m_tick.setInterval(ms);
    if (m_ready && !m_tick.isActive()) {
        m_tick.start();
        emit runningChanged();
    }
}

// --- keys -------------------------------------------------------------------

// Ambaŭ estas konstantoj de la kompililo, do ili ne bezonas la motoron por ion
// ajn; ili vivas ĉi tie ĉar la motoro estas kion QML jam havas ĉe la mano.
QString Agape48Engine::buildStamp() const
{
    // fromUtf8, ne fromLatin1: la kaptilo portas "·" kaj BuildStamp.cmake skribas
    // la dosieron en UTF-8, do Latin-1 faris el ĝi "Â·" - vidita sur la ekrano.
    return QString::fromUtf8(AGAPE48_BUILD);
}

QString Agape48Engine::qtVersion() const
{
    return QString::fromLatin1(QT_VERSION_STR);
}

// Androido scias tion kaj Qt ne demandas ĝin: QInputDevice::primaryKeyboard()
// elpensas "core keyboard"-aparaton kiam neniu estas registrita, do nombri la
// aparatojn de Qt respondas "jes" sur ĉiu telefono. Configuration.keyboard
// estas la kanona respondo - KEYBOARD_NOKEY = 1 - kaj hardKeyboardHidden
// kaptas la duan kazon, klavaro kiu ekzistas sed estas fermita aŭ malkonektita
// (HARDKEYBOARDHIDDEN_YES = 2).
//
// La labortabloj respondas jes senkondiĉe. Tekokomputilo sen klavaro ne estas
// kazo kiun ĉi tiu programo bezonas trakti, kaj la ŝpruchelpilo tie estas
// petita funkcio.
bool Agape48Engine::keyboardAttached() const
{
#ifdef Q_OS_ANDROID
    QJniObject ctx(QNativeInterface::QAndroidApplication::context());
    if (!ctx.isValid())
        return false;
    const QJniObject res =
        ctx.callObjectMethod("getResources", "()Landroid/content/res/Resources;");
    if (!res.isValid())
        return false;
    const QJniObject cfg =
        res.callObjectMethod("getConfiguration", "()Landroid/content/res/Configuration;");
    if (!cfg.isValid())
        return false;
    return cfg.getField<jint>("keyboard") != 1
        && cfg.getField<jint>("hardKeyboardHidden") != 2;
#else
    return true;
#endif
}

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

// The far right of the slider: the factor at which the paced budget reaches the
// free-running ceiling. Beyond it the throttle would be asking for more
// instructions a second than the tick can deliver, which is not "faster", it is
// just an unpayable debt - see the note on kPaceMaxOwedUs.
double Agape48Engine::speedFactorMax() const
{
    return m_realSpeedRate > 0 ? double(kRateCeiling) / m_realSpeedRate : 1.0;
}

int Agape48Engine::effectiveRate() const
{
    return qBound(kRateFloor,
                  int(qRound(m_realSpeedRate * m_speedFactor)),
                  kRateCeiling);
}

void Agape48Engine::setSpeedFactor(double factor)
{
    const double f = qBound(kFactorFloor, factor, speedFactorMax());
    if (qFuzzyCompare(m_speedFactor, f))
        return;
    m_speedFactor = f;
    calcSettings(m_state)->setValue(QLatin1String("speed/factor"), f);
    // Start from now rather than settling a debt incurred at a different rate,
    // the same reasoning as the switch and the rate.
    m_paceAt   = 0;
    m_paceOwed = 0;
    emit speedFactorChanged();
}

void Agape48Engine::setRunUnfocused(bool on)
{
    if (m_runUnfocused == on)
        return;
    m_runUnfocused = on;
    calcSettings(m_state)->setValue(QLatin1String("window/runUnfocused"), on);
    // Take effect immediately rather than at the next alt-tab: if the window is
    // inactive right now, the calculator is already stopped, and a switch the
    // user has just turned on ought to start it.
    if (on && m_ready && !m_detached)
        start();
    emit runUnfocusedChanged();
}

void Agape48Engine::setRealSpeed(bool on)
{
    if (m_realSpeed == on)
        return;
    m_realSpeed = on;
    calcSettings(m_state)->setValue(QLatin1String("speed/real"), on);
    // Start from now, and throw the debt away: flipping the switch is not a
    // reason to catch up on time the calculator spent running free.
    m_paceAt   = 0;
    m_paceOwed = 0;
    emit realSpeedChanged();
}

void Agape48Engine::setAutoToStr(bool on)
{
    if (m_autoToStr == on)
        return;
    m_autoToStr = on;
    calcSettings(m_state)->setValue(QLatin1String("clipboard/autoToStr"), on);
    emit autoToStrChanged();
}

void Agape48Engine::setAutoStrTo(bool on)
{
    if (m_autoStrTo == on)
        return;
    m_autoStrTo = on;
    calcSettings(m_state)->setValue(QLatin1String("clipboard/autoStrTo"), on);
    emit autoStrToChanged();
}

// How many instructions real time has earned since the last slice. THIS IS THE
// WHOLE OF THE THROTTLE: a smaller budget, never a wait. So real speed costs
// LESS cpu than running free - which is the opposite of x48's own throttle, a
// gettimeofday() spin loop of 2 us per instruction on the thread that draws,
// and the reason that one would have been wrong for a phone even if it had been
// reachable. It is not: it lives in emulate(), which this build never calls.
//
// Paced on the clock rather than on a fixed budget per tick because a fixed
// budget is only as accurate as the timer, and a tick that fires late silently
// loses Saturn time. In a game that is jitter.
int Agape48Engine::realSpeedBudget()
{
    const qint64 now = m_clock.nsecsElapsed() / 1000;   // microseconds
    if (m_paceAt == 0)
        m_paceAt = now;                                 // the first slice only

    qint64 us = now - m_paceAt;
    m_paceAt = now;
    // Only a genuine stall loses time. Measured 2026sep09: with this clamp at
    // two ticks, 49 runs of one fixed loop had a standard deviation of 61% of
    // their mean - 0.44 s to 3.17 s - against Emu48 producing the SAME NUMBER
    // to twelve digits four times in a row. Anything above the clamp was
    // discarded, and this laptop was building with -j 14 through most of that
    // session, so ticks fired late constantly and the Saturn quietly lost the
    // overflow every time.
    if (us > kPaceMaxOwedUs)
        us = kPaceMaxOwedUs;

    // The remainder is carried in instruction-microseconds. Without it every
    // tick would round down and the rate would drift low by up to one
    // instruction per tick - sixty a second, which is small but is a bias
    // rather than noise, so it never averages out.
    const int rate = effectiveRate();
    m_paceOwed += us * rate;
    qint64 n = m_paceOwed / 1000000;

    // Cap what one slice RUNS without cancelling what is owed: a burst of two
    // hundred thousand instructions in one tick is a visible jump in a game,
    // and throwing the debt away instead is the jitter above. Bounded above,
    // paid off below.
    // AGAINST THE CURRENT INTERVAL, not the 16 ms one. The 48 drops into SHUTDN
    // between key scans by design, so setTickRate() moves this timer to
    // kIdleIntervalMs constantly - and a 100 ms tick owes 50000 instructions
    // at half a million a second while a cap of four 16 ms ticks would only
    // ever pay 32000 of them. Ten ticks a second times 32000 is 320000, the
    // debt grows for ever, and the calculator runs at two thirds of its target
    // or worse. Measured 2026sep10: 84 samples of one fixed loop took minutes
    // each instead of seconds.
    const qint64 maxSlice = qint64(kPaceMaxSliceTicks) * m_tick.interval()
                            * 1000 * rate / 1000000;
    if (n > maxSlice)
        n = maxSlice;

    m_paceOwed -= n * 1000000;
    return int(n);
}

// The one number the calibration produces. Persisted per machine, which is
// right: the rate is a property of a real HP 48 and not of this laptop, but the
// only way to arrive at it is to compare against something, and what is
// available to compare against differs from machine to machine.
void Agape48Engine::setRealSpeedRate(int instructionsPerSecond)
{
    const int rate = qBound(kRateFloor, instructionsPerSecond, kRateCeiling);
    if (m_realSpeedRate == rate)
        return;
    m_realSpeedRate = rate;
    QSettings().setValue(QLatin1String("speed/rate"), rate);
    // Same reasoning as the switch: start from now rather than settling a debt
    // that was incurred at a different rate.
    m_paceAt   = 0;
    m_paceOwed = 0;
    emit realSpeedRateChanged();
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
// so an imported object sat there invisibly until the next keypress. Agape48
// presses ON itself (dogfood #16): on the 48 that is also CANCEL, so a
// half-typed command line is lost, and that cost was accepted.
//
// The condition is the interesting half. ON with a shift active is not ON - it
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
    // pressing one of them costs: a calculator whose contents record named a ram
    // that had not arrived opened with the memory from before the handover
    // instead of the one a minute and a half of waiting had been for.
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
// and still reads as held. That is deliberate here, and the rule of 2026sep04 is
// literal: "If another host died and left the lock on, keep showing the locked
// screen as is with the phrase on it." An offline machine and a dead one are the same file,
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
    // stack in line 8 is what reading it looked like.
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
// under its old name. both-05 line 37: "there were not two, but 3:
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
        // Measured on the phone before the fix: "test123.hpp, 0 B".
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

// DIAGNOSTIC, and meant to be removed once the rate is settled. Two of these
// taken at two saves give instructions per wall second AND ticks per wall
// second, neither of which needs saturn.i_per_s - which on 2026sep10 held
// steady at 434000 while the wall clock said the same loop took 58.8 s and
// then 10.3 s - nor the 48's own TICKS, which disagreed by a factor of twenty.
// The variance is best explained by Windows treating an unfocused process
// differently, and ticks per wall second is exactly the number that settles it.
void Agape48Engine::writeSpeedProbe()
{
    const QString dir = m_state->location().toLocalFile();
    if (dir.isEmpty())
        return;
    QFile f(QDir(dir).filePath(QStringLiteral("speed-probe.txt")));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;
    QTextStream out(&f);
    out << "uptime_ms    " << m_clock.elapsed() << '\n'
        << "instructions " << qulonglong(x48_instructions_total()) << '\n'
        << "ticks        " << m_tickCount << '\n'
        << "tick_ms      " << m_tick.interval() << '\n'
        << "asleep       " << (x48_is_asleep() ? 1 : 0) << '\n'
        << "real         " << (m_realSpeed ? 1 : 0) << '\n'
        << "rate         " << m_realSpeedRate << '\n'
        << "factor       " << m_speedFactor << '\n'
        << "effective    " << effectiveRate() << '\n'
        << "i_per_s      " << x48_instructions_per_second() << '\n'
        << "focused      "
        << (QGuiApplication::focusWindow() != nullptr ? 1 : 0) << '\n';
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
    // on the calculator's stack?" It should. Measured: the log's
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
    writeSpeedProbe();
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

// Nivelo 1 kiel teksto, per la tradukado de la kerno. False, kun la frazo en
// x48_last_error(), kiam nivelo 1 estas nek nombro nek ĉeno.
static bool level1Text(QString *text)
{
    QByteArray buf(512, Qt::Uninitialized);
    size_t need = x48_stack_to_text(buf.data(), size_t(buf.size()));
    if (need > size_t(buf.size())) {          // retry once with the real size
        buf.resize(int(need));
        need = x48_stack_to_text(buf.data(), size_t(buf.size()));
    }
    if (need == 0 || need > size_t(buf.size()))
        return false;
    // need counts the terminator the core writes; the string does not want it.
    buf.truncate(int(need) - 1);
    *text = QString::fromUtf8(buf);
    return true;
}

QString Agape48Engine::copyStackToClipboard()
{
    // Kun la aŭtomata →STR la teksto venas de la ROM, kelkajn momentojn poste:
    // stepAuto() metas ĝin en la tondujon kaj anoncas ĝin per clipboardCopied.
    if (m_autoToStr) {
        if (readyForObject(true)) {
            if (!x48_stack_has_object())
                setError(tr("There is nothing on level 1 to copy."));
            else if (!x48_push_tostr_program())
                setError(QString::fromUtf8(x48_last_error()));
            else
                beginAuto(1);
        }
        return {};
    }
    QString text;
    if (!level1Text(&text)) {
        setError(QString::fromUtf8(x48_last_error()));
        return {};
    }
    QGuiApplication::clipboard()->setText(text);
    emit clipboardCopied(text);
    return text;
}

bool Agape48Engine::pasteClipboardToStack()
{
    const QString text = QGuiApplication::clipboard()->text();
    if (text.isEmpty())
        return false;
    return pasteText(text);
}

// --- objects arriving from outside ------------------------------------------

// Ŝovklavo ŝlosita ŝanĝas la sencon de la sekva klavo - maldekstra ŝovo faras
// el EVAL →NUM, dekstra faras el ON OFF - do ĉiu premata sekvenco komenciĝas per
// malŝlosado, same kiel la puŝeto post Enporti.
QStringList Agape48Engine::unlatchShifts() const
{
    QStringList seq;
    if (m_annunciators & X48_ANN_RIGHT) seq << QStringLiteral("SHR");
    if (m_annunciators & X48_ANN_LEFT)  seq << QStringLiteral("SHL");
    return seq;
}

QString Agape48Engine::notReadyReason() const
{
    switch (x48_readiness()) {
    case X48_READY:       return {};
    case X48_NOT_RUNNING: return tr("The calculator is not running.");
    case X48_NOT_GX:      return tr("This needs an HP 48GX ROM.");
    case X48_BUSY:        return tr("The calculator is busy. Wait for it to finish, or press ON to stop it.");
    case X48_OFF:         return tr("The calculator is off. Press ON first.");
    case X48_EDITING:     return tr("The command line is open. Press ENTER or ON first.");
    case X48_MESSAGE:     return tr("A message is on the calculator's screen. Press ON first.");
    case X48_ELSEWHERE:   break;
    }
    return tr("The calculator is not at the stack. Leave the form or application first.");
}

// La antaŭkondiĉo de Alglui kaj de ĉiu demeto: la kalkulilo atendas ĉe la stako.
// La puŝeto post la metado estas ON, kaj ON estas CANCEL - en malfermita
// komandlinio ĝi forviŝus la tajpitan tekston. Enporti per la menuo ne pasas
// tra ĉi tie: sur Androido la dosierelektilo sendas la aplikaĵon al la fono, kaj
// la stato post la reveno ne estas mezurita.
bool Agape48Engine::readyForObject(bool pressesKeys)
{
    if (m_autoStep != 0) {
        setError(tr("The calculator is still finishing the last copy or paste."));
        return false;
    }
    const QString why = notReadyReason();
    if (!why.isEmpty()) {
        setError(why);
        return false;
    }
    // USER-reĝimo povas doni al EVAL alian taskon, kaj la aŭtomata vojo premas
    // EVAL. Pli bone rifuzi ol ruli ies propran klavon.
    if (pressesKeys && x48_user_mode()) {
        setError(tr("USER mode is on, and it can give EVAL another job. Turn USER mode off to use automatic →STR or STR→."));
        return false;
    }
    // En alfa-reĝimo EVAL tajpus la literon O en novan komandlinion.
    if (pressesKeys && (m_annunciators & X48_ANN_ALPHA)) {
        setError(tr("Alpha mode is on. Press α until its indicator goes off, then try again."));
        return false;
    }
    return true;
}

bool Agape48Engine::pasteText(const QString &text)
{
    const bool strTo = m_autoStrTo;
    if (!readyForObject(strTo))
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
    // Nombro jam estas objekto; nur ĉeno bezonas STR→.
    if (strTo && x48_level1_is_string()) {
        level1Text(&m_autoPasted);
        if (x48_push_strto_program()) {
            beginAuto(2);
            return true;
        }
        setError(QString::fromUtf8(x48_last_error()));
    }
    // THE SAME NUDGE IMPORT NEEDS, and leaving it out is why Paste looked like
    // it did nothing at all: the object was pushed and the stack on the glass
    // went on showing what it showed before, because the ROM redraws when
    // something happens to the machine and nothing had. Measured on the phone
    // on 2026sep09 - 3.14158 copied, dropped, pasted, and the display still
    // read what was under it.
    queueTaps(unlatchShifts() << QStringLiteral("ON"));
    return true;
}

// LA ROM RULAS LA PROGRAMON, NE ĈI TIU KODO. La programo jam kuŝas sur nivelo 1,
// kaj EVAL rulas ĝin. stepAuto() premas EVAL nur kiam la kalkulilo denove atendas
// ĉe la stako post eventuala malŝlosado de ŝovklavo, kaj poste atendas, ĝis la
// programo malaperis de nivelo 1.
//
// NENIU ON ANTAŬ EVAL. ON estas ATTN, kaj la ROM malplenigas sian klavbufron dum
// ĝi traktas ATTN: EVAL premita dum tiu traktado simple malaperas. Mezurite en
// 2026sep13: ON je 17 ms, EVAL je 86 ms, la Saturn okupata ĝis ĉirkaŭ 170 ms, kaj
// la programo ankoraŭ sur nivelo 1 poste. La alfa-reĝimo, kontraŭ kiu ON estis
// tie, nun estas rifuzata antaŭe per sia indikilo.
void Agape48Engine::beginAuto(int step)
{
    m_autoStep   = step;
    m_autoObject = x48_level1_address();
    m_autoSince  = m_clock.elapsed();
    m_autoEvalQueued = false;
    setError(QString());
    const QStringList shifts = unlatchShifts();
    if (!shifts.isEmpty())
        queueTaps(shifts);
    else
        setTickRate(kTickIntervalMs);
}

void Agape48Engine::stepAuto()
{
    // La klavoj unue devas eniri, alie la stato ankoraŭ estas tiu de antaŭ EVAL.
    if (!m_tapQueue.isEmpty() || !m_releasePending.isEmpty() || !m_downAt.isEmpty())
        return;

    const x48_readiness_t state = x48_readiness();

    if (!m_autoEvalQueued) {
        if (state != X48_READY) {
            if (m_clock.elapsed() - m_autoSince < kAutoStartMs)
                return;
            m_autoStep = 0;
            if (x48_level1_address() == m_autoObject)
                x48_drop_level1();
            setError(notReadyReason());
            return;
        }
        queueTaps({ QStringLiteral("EVAL") });
        m_autoEvalQueued = true;
        m_autoSince = m_clock.elapsed();
        return;
    }
    const bool ran    = x48_level1_address() != m_autoObject;
    const qint64 wait = m_clock.elapsed() - m_autoSince;
    const int step    = m_autoStep;

    if (state == X48_BUSY || (!ran && wait < kAutoStartMs)) {
        if (wait < kAutoTimeoutMs)
            return;
        m_autoStep = 0;
        // STR→ povas plenumi longan kalkulon, kaj tio estas ĝia rajto: la
        // algluita teksto kuras plu, nur ĉi tiu gardado ĉesas. Por →STR tiom da
        // tempo signifas, ke io misiris.
        if (step == 1)
            setError(tr("→STR is taking too long. Copy stopped waiting for it."));
        return;
    }
    m_autoStep = 0;

    if (!ran) {
        // EVAL neniam atingis la programon. Ĝi foriras, kaj nenio alia ŝanĝiĝis.
        if (x48_level1_address() == m_autoObject)
            x48_drop_level1();
        queueTaps(unlatchShifts() << QStringLiteral("ON"));
        setError(tr("The calculator did not run the program. Nothing was changed."));
        return;
    }

    if (step == 1) {
        if (state != X48_READY || !x48_level1_is_string()) {
            setError(tr("→STR stopped before it finished. The calculator's screen says why."));
            return;
        }
        QString text;
        if (!level1Text(&text)) {
            setError(QString::fromUtf8(x48_last_error()));
            return;
        }
        // La ĉeno estis nur la vojo al la tondujo; la stako restas kia ĝi estis.
        x48_drop_level1();
        queueTaps(unlatchShifts() << QStringLiteral("ON"));
        QGuiApplication::clipboard()->setText(text);
        emit clipboardCopied(text);
        return;
    }

    // RIFUZITAN STR→ MONTRAS LA TEKSTO, NE LA STATO. La ROM remetas la ĉenon per
    // LASTARG, ĉe alia adreso ol la algluita (mezurite), kaj la eraro staras en la
    // statusaj linioj, kiujn x48_readiness() ne legas. Sukcesa STR→ lasas ĉenon
    // kun la sama teksto nur se la teksto mem ĝin rekreas, ekzemple nomo de
    // variablo, kiu enhavas ĝuste tiun tekston.
    QString back;
    const bool refused = x48_level1_is_string() && level1Text(&back)
                         && back == m_autoPasted;
    m_autoPasted.clear();
    if (state != X48_READY || refused)
        setError(tr("STR→ stopped with a message. The calculator's screen says why."));
}

QString Agape48Engine::sniffDropFile(const QUrl &url) const
{
    if (!url.isLocalFile())
        return url.scheme() == QLatin1String("content") ? QStringLiteral("unknown") : QString();
    const QString path = url.toLocalFile();
    QFile f(path);
    if (!QFileInfo(path).isFile() || !f.open(QIODevice::ReadOnly))
        return {};
    const qint64 size = f.size();
    const QByteArray head = f.peek(8);
    if (head.startsWith("HPHP48-"))
        return x48_object_file_loadable(path.toUtf8().constData())
                   ? QStringLiteral("object") : QString();
    // La du formoj, kiujn la ROM-leganto de x48 mem rekonas - pakita kaj
    // malpakita - je la tri grandoj, kiujn HP 48-ROM havas.
    if ((size == 262144 || size == 524288 || size == 1048576)
        && (head.startsWith(QByteArrayLiteral("\x32\x96\x1b\x80"))
            || head.startsWith(QByteArrayLiteral("\x02\x03\x06\x09"))))
        return QStringLiteral("rom");
    if (size > 0 && size <= kDropTextMaxBytes) {
        const QByteArray all = f.readAll();
        if (!all.contains('\0') && x48_text_loadable(all.constData()))
            return QStringLiteral("text");
    }
    return {};
}

QString Agape48Engine::dropKind(const QList<QUrl> &urls, const QString &text)
{
    if (urls.isEmpty()) {
        if (text.isEmpty())
            return QStringLiteral("unknown");
        return x48_text_loadable(text.toUtf8().constData()) ? QStringLiteral("text") : QString();
    }
    if (urls.size() == 1)
        return sniffDropFile(urls.first());
    // Pluraj dosieroj nur kiam ĉiuj estas objektoj: ROM venas sola, kaj teksto
    // kun aŭtomata STR→ okupas la kalkulilon ĝis la ROM finis.
    for (const QUrl &u : urls) {
        const QString k = sniffDropFile(u);
        if (k != QLatin1String("object") && k != QLatin1String("unknown"))
            return {};
    }
    return QStringLiteral("object");
}

bool Agape48Engine::dropAllowed(const QString &kind) const
{
    if (kind.isEmpty() || m_autoStep != 0)
        return false;
    const bool noRom = m_romSource.isEmpty();
    if (kind == QLatin1String("rom"))
        return noRom && !m_ready;
    if (kind == QLatin1String("unknown"))
        return (noRom && !m_ready) || x48_readiness() == X48_READY;
    return x48_readiness() == X48_READY;
}

bool Agape48Engine::drop(const QList<QUrl> &urls, const QString &text)
{
    // Androida dokumento estas legebla nur post la demeto. Unu loka kopio, kaj de
    // tie la sama kontrolo kiel por dosiero sur la labortablo.
    QList<QUrl> local;
    QStringList scratch;
    for (int n = 0; n < urls.size(); ++n) {
        const QUrl &u = urls.at(n);
        if (u.isLocalFile()) {
            local << u;
            continue;
        }
        const QString copy = transferScratchPath() + QStringLiteral("-drop%1").arg(n);
        if (!copyBytes(u.toString(), copy)) {
            for (const QString &c : scratch)
                QFile::remove(c);
            QFile::remove(copy);
            setError(tr("Could not read that file."));
            return false;
        }
        scratch << copy;
        local << QUrl::fromLocalFile(copy);
    }

    const QString kind = dropKind(local, text);
    const bool wasStopped = !m_tick.isActive();
    bool ok = false;
    if (kind.isEmpty() || kind == QLatin1String("unknown")) {
        setError(tr("That cannot be loaded into the calculator."));
    } else if (!dropAllowed(kind)) {
        setError(kind == QLatin1String("rom")
                     ? tr("A ROM is only taken when the calculator has none.")
                     : notReadyReason());
    } else if (kind == QLatin1String("rom")) {
        // La originala adreso, ne la kopio: start() mem kopias content://-ROM-on
        // en la dosierujon de la kalkulilo, kaj la kopio ĉi tie baldaŭ malaperos.
        setRomSource(urls.first());
        ok = start();
    } else if (local.isEmpty()) {
        ok = pasteText(text);
    } else if (kind == QLatin1String("text")) {
        QFile f(local.first().toLocalFile());
        ok = f.open(QIODevice::ReadOnly) && pasteText(QString::fromUtf8(f.readAll()));
    } else {
        ok = true;
        for (const QUrl &u : local) {
            if (!(ok = importFile(u)))
                break;
        }
    }
    for (const QString &c : scratch)
        QFile::remove(c);
    if (ok && wasStopped)
        m_dropWoke = true;
    return ok;
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

// KONTROLO ANTAŬ ŜARĜO. La kerno malfermas la ROM-on per fopen() kaj diras sian
// opinion nur al la protokolo, do ĝis 2026sep16 malbona vojo kaj dosiero kiu ne
// estas ROM aspektis same kiel sukceso. Ĉi tio legas la samajn kvar kapajn
// bajtojn kaj la saman nibbleon 0x29 kiel read_rom_file() en romio.c, kaj
// nomas la kialon: la kampo devas povi diri ĈU la vojo, ĈU la dosiero.
//
// Ĝi legas 42 bajtojn kaj demandas la grandon, do ĝi kostas sufiĉe malmulte por
// esti vokata je ĉiu tajpita signo.
QVariantMap Agape48Engine::inspectRom(const QString &text) const
{
    QVariantMap out;
    out[QStringLiteral("ok")] = false;
    out[QStringLiteral("model")] = QString();

    const QString wanted = text.trimmed();
    if (wanted.isEmpty()) {
        out[QStringLiteral("problem")] = tr("No ROM file has been chosen.");
        return out;
    }

    // La sama konvertado kiel ĉie aliloke: vojo tajpita de homo ne estas URL,
    // kaj content:// el la Androida elektilo ne estas vojo.
    const QUrl source = pathToUrl(wanted);
    const QString name = source.isLocalFile() ? source.toLocalFile()
                                              : source.toString();
    const QString shown = urlToPath(source);

    if (source.isLocalFile()) {
        const QFileInfo info(name);
        if (info.isDir()) {
            out[QStringLiteral("problem")] =
                tr("%1 is a folder, not a ROM image.").arg(shown);
            return out;
        }
        if (!info.exists()) {
            out[QStringLiteral("problem")] =
                tr("There is no file at %1.").arg(shown);
            return out;
        }
    }

    QFile file(name);
    if (!file.open(QIODevice::ReadOnly)) {
        out[QStringLiteral("problem")] = tr("%1 cannot be read.").arg(shown);
        return out;
    }
    const QByteArray head = file.read(0x2a);
    const qint64 bytes = file.size();
    file.close();

    if (head.size() < 0x2a) {
        out[QStringLiteral("problem")] =
            tr("%1 is far too small to be an HP 48 ROM.").arg(shown);
        return out;
    }

    // La kvar kapaj bajtoj diras kiel la nibbleoj kuŝas: paka dosiero portas du
    // en bajto, nepaka unu. Ĉio ĉi estas read_rom_file(), sen la ŝargo.
    const uchar *h = reinterpret_cast<const uchar *>(head.constData());
    qint64 nibbles = 0;
    bool packed = false;
    // Ĉu la dosiero mem diras kio ĝi estas. La tria vojo sube akceptas kian ajn
    // dosieron kies unua bajto ne estas nulo, kiel nudan kopion de memoro, kaj
    // tial dosiero kiu falas tra ĝi meritas alian frazon ol ROM tranĉita
    // duonvoje: unu estas malbona elŝuto, la alia estas simple alia dosiero.
    bool declared = true;
    if (h[0] == 0x02 && h[1] == 0x03 && h[2] == 0x06 && h[3] == 0x09) {
        nibbles = bytes;
    } else if (h[0] == 0x32 && h[1] == 0x96 && h[2] == 0x1b && h[3] == 0x80) {
        nibbles = 2 * bytes;
        packed = true;
    } else if (h[1] == 0x49) {
        out[QStringLiteral("problem")] =
            tr("%1 is an HP 49 ROM, and this is an HP 48.").arg(shown);
        return out;
    } else if (h[0]) {
        nibbles = bytes;
        declared = false;
    } else {
        out[QStringLiteral("problem")] =
            tr("%1 is not an HP 48 ROM.").arg(shown);
        return out;
    }

    // La nibbleo 0x29 diras la serion, kaj la grando devas kongrui kun ĝi -
    // ROM_SIZE_SX kaj ROM_SIZE_GX el romio.h, en nibbleoj. Mezurite sur ambaŭ
    // veraj dosieroj: gxrom-r portas tie 0, sxrom-j portas 8.
    const int at = packed ? 0x29 / 2 : 0x29;
    const uchar nibble = packed ? ((h[at] >> 4) & 0xf) : (h[at] & 0xf);
    const bool gx = nibble == 0;
    const qint64 whole = gx ? 0x100000 : 0x080000;
    if (nibbles != whole) {
        out[QStringLiteral("problem")] = declared
            ? tr("%1 is not a whole HP 48%2 ROM: one is %3 bytes, and this file "
                 "is %4.").arg(shown, gx ? QStringLiteral("G/GX")
                                         : QStringLiteral("S/SX"),
                               QString::number(packed ? whole / 2 : whole),
                               QString::number(bytes))
            : tr("%1 is not an HP 48 ROM.").arg(shown);
        return out;
    }

    out[QStringLiteral("ok")] = true;
    out[QStringLiteral("model")] = gx ? QStringLiteral("48GX")
                                      : QStringLiteral("48SX");
    out[QStringLiteral("problem")] = QString();
    return out;
}

// LA SOLA VOJO KIU ŜANĜAS LA ROM-ON DE FUNKCIANTA KALKULILO. start() revenas
// tuj kiam la kerno jam staras, do "romSource = elektita; start()" ŝanĝis
// nenion ajn dum kalkulilo kuris - la dua duono de tio kion la elektilo
// silente perdis. La kerno estas malkonstruita unue, kio konservas la
// kalkulilon kiun ĝi forlasas, same kiel ŝanĝo de kalkulilo sur la breto.
//
// ROM kiu ne ekfunkcias remetas tiun kiu funkciis: la fenestro rajtas rifuzi
// elekton, sed ne rajtas lasi la uzanton kun morta kalkulilo pro provo.
bool Agape48Engine::loadRom(const QUrl &source)
{
    const QUrl had = m_romSource;
    if (source == had && m_ready)
        return true;
    shutdownCore();
    setRomSource(source);
    if (start())
        return true;

    const QString why = m_lastError;
    setRomSource(had);
    if (start())
        setError(why);
    return false;
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
