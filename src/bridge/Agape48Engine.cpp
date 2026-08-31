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
        if (!m_ready)
            start();
    });

    // An external sync client rewriting the state file under us is the normal
    // case, not the exceptional one - that is the whole point of BYO-sync.
    connect(m_state, &StateFileManager::externalChangeDetected,
            this, [this] { if (!isRunning()) reloadState(); });

    m_skin->load(QUrl(QStringLiteral("qrc:/qt/qml/Agape48/assets/skins/default/layout.json")));

    // Let go of the state folder on the way out. aboutToQuit rather than the
    // destructor alone: Qt.quit() from the menu tears the QML engine down in
    // an order this object does not control, and a lock left behind is a lock
    // the next start has to reason about. A crash still leaves one, which is
    // exactly what the dead-pid check in claim() is for.
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { m_state->release(); });
}

Agape48Engine::~Agape48Engine()
{
    if (m_ready) {
        x48_save_state();
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
        emit stateFolderBusy();
        setError(m_state->lastError());
        return false;
    }

    logStartupFacts();

    x48_config_t cfg {};
    cfg.fd_ram = cfg.fd_port1 = cfg.fd_port2 = cfg.fd_state = -1;
    cfg.throttle = true;

    const QByteArray romPath = m_romSource.toLocalFile().toUtf8();
    cfg.rom_path = romPath.constData();

    // Desktop fills cfg.state_dir; Android fills cfg.fd_* from SAF, because a
    // content:// tree URI has no POSIX path to hand to the C core.
    QByteArray stateDirUtf8;
    if (!m_state->populate(&cfg, &stateDirUtf8)) {
        setError(m_state->lastError());
        return false;
    }

    if (!x48_init(&cfg)) {
        setError(QString::fromUtf8(x48_last_error()));
        return false;
    }

    m_ready = true;
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
        if (off != m_displayOff) {
            m_displayOff = off;
            qWarning(off ? "screen off - the calculator has been switched off "
                           "(OFF is green-shift ON, and Ctrl is the green "
                           "shift). Press ON to switch it back on."
                         : "screen on");
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
    const bool asleep = x48_is_asleep() && m_releasePending.isEmpty();
    setTickRate(asleep ? kIdleIntervalMs : kTickIntervalMs);
}

// The timer never stops while the calculator is on; only its rate changes.
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
    const QString path = url.toLocalFile();
    if (path.isEmpty()) {
        setError(tr("Agape48 can only read a file on this computer."));
        return false;
    }
    // Safe to reach into the Saturn's memory from here: emulation runs on this
    // thread from a timer, so a menu handler is always between two slices and
    // never inside one.
    if (!x48_import_file(path.toUtf8().constData())) {
        setError(QString::fromUtf8(x48_last_error()));
        return false;
    }
    setError(QString());
    setTickRate(kTickIntervalMs);
    return true;
}

bool Agape48Engine::exportFile(const QUrl &url)
{
    if (!m_ready) {
        setError(tr("The calculator is not running."));
        return false;
    }
    const QString path = url.toLocalFile();
    if (path.isEmpty()) {
        setError(tr("Agape48 can only write a file on this computer."));
        return false;
    }
    if (!x48_export_file(path.toUtf8().constData())) {
        setError(QString::fromUtf8(x48_last_error()));
        return false;
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
    if (!x48_save_state()) {
        setError(QString::fromUtf8(x48_last_error()));
        return false;
    }
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
    m_frameSerial = 0;
    return true;
}

// --- clipboard --------------------------------------------------------------

bool Agape48Engine::copyStackToClipboard()
{
    QByteArray buf(512, Qt::Uninitialized);
    size_t need = x48_stack_to_text(buf.data(), size_t(buf.size()));
    if (need == 0)
        return false;
    if (need > size_t(buf.size())) {          // retry once with the real size
        buf.resize(int(need));
        need = x48_stack_to_text(buf.data(), size_t(buf.size()));
        if (need == 0 || need > size_t(buf.size()))
            return false;
    }
    buf.truncate(int(need));
    QGuiApplication::clipboard()->setText(QString::fromUtf8(buf));
    return true;
}

bool Agape48Engine::pasteClipboardToStack()
{
    const QString text = QGuiApplication::clipboard()->text();
    if (text.isEmpty())
        return false;
    if (!x48_text_to_stack(text.toUtf8().constData())) {
        setError(tr("Clipboard text is not a valid RPL object."));
        return false;
    }
    setTickRate(kTickIntervalMs);
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
