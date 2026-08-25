#include "Agape48Engine.h"

#include "SkinModel.h"
#include "StateFileManager.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHash>
#include <QStringView>

namespace {

// Emulation pacing. The HP 48 Saturn runs at ~4 MHz (48G) / ~2 MHz (48S), so a
// 60 Hz tick is roughly 70000 cycles. Ticking on the GUI thread is deliberate:
// a 4 MHz interpreter costs low single-digit percent of one modern core, and a
// worker thread would buy nothing but a frame-handoff race. If profiling ever
// says otherwise, this is the one place that has to move.
constexpr int  kTickIntervalMs = 16;
constexpr int  kCyclesPerTick  = 70000;

// -----------------------------------------------------------------------------
// Key name -> (out row, in mask).
//
// The NAMES are the contract skins are written against, and they are stable.
// The CODES are not filled in: the exact (row, mask) pairs differ in ordering
// between x48 forks, and inventing 49 of them would produce a keyboard that
// looks right and types garbage. Copy them out of the vendored fork's keyboard
// table (x48ng: src/keyboard.c) as part of VENDORING.md step 4.
//
// Until then lookupKey() fails loudly and the engine reports which key.
// -----------------------------------------------------------------------------
struct KeyDef { const char *id; int row; int mask; };

constexpr int kUnmapped = -1;

const KeyDef kKeys[] = {
    // menu row
    { "A", kUnmapped, 0 }, { "B", kUnmapped, 0 }, { "C", kUnmapped, 0 },
    { "D", kUnmapped, 0 }, { "E", kUnmapped, 0 }, { "F", kUnmapped, 0 },
    // second row
    { "MTH", kUnmapped, 0 }, { "PRG", kUnmapped, 0 }, { "CST", kUnmapped, 0 },
    { "VAR", kUnmapped, 0 }, { "UP",  kUnmapped, 0 }, { "NXT", kUnmapped, 0 },
    // third row
    { "QUOTE", kUnmapped, 0 }, { "STO", kUnmapped, 0 }, { "EVAL", kUnmapped, 0 },
    { "LEFT",  kUnmapped, 0 }, { "DOWN",kUnmapped, 0 }, { "RIGHT",kUnmapped, 0 },
    // fourth row
    { "SIN", kUnmapped, 0 }, { "COS", kUnmapped, 0 }, { "TAN", kUnmapped, 0 },
    { "SQRT",kUnmapped, 0 }, { "POWER", kUnmapped, 0 },
    // fifth row
    { "INV", kUnmapped, 0 }, { "EEX", kUnmapped, 0 }, { "NEG", kUnmapped, 0 },
    { "DEL", kUnmapped, 0 }, { "BS",  kUnmapped, 0 },
    // numeric block
    { "ALPHA", kUnmapped, 0 }, { "N7", kUnmapped, 0 }, { "N8", kUnmapped, 0 },
    { "N9",    kUnmapped, 0 }, { "DIV",kUnmapped, 0 },
    { "SHL",   kUnmapped, 0 }, { "N4", kUnmapped, 0 }, { "N5", kUnmapped, 0 },
    { "N6",    kUnmapped, 0 }, { "MUL",kUnmapped, 0 },
    { "SHR",   kUnmapped, 0 }, { "N1", kUnmapped, 0 }, { "N2", kUnmapped, 0 },
    { "N3",    kUnmapped, 0 }, { "MINUS", kUnmapped, 0 },
    { "N0",    kUnmapped, 0 }, { "PERIOD",kUnmapped, 0 }, { "SPC", kUnmapped, 0 },
    { "PLUS",  kUnmapped, 0 }, { "ENTER", kUnmapped, 0 },
    // ON sits outside the matrix - X48_KB_ROW_ON. Part of the ON+A+F reset.
    { "ON", X48_KB_ROW_ON, 0 },
};

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
    m_tick.setInterval(kTickIntervalMs);
    m_tick.setTimerType(Qt::PreciseTimer);
    connect(&m_tick, &QTimer::timeout, this, &Agape48Engine::tick);

    // An external sync client rewriting the state file under us is the normal
    // case, not the exceptional one - that is the whole point of BYO-sync.
    connect(m_state, &StateFileManager::externalChangeDetected,
            this, [this] { if (!isRunning()) reloadState(); });

    m_skin->load(QUrl(QStringLiteral("qrc:/qt/qml/Agape48/assets/skins/default/layout.json")));
}

Agape48Engine::~Agape48Engine()
{
    if (m_ready) {
        x48_save_state();
        x48_shutdown();
    }
}

// --- lifecycle --------------------------------------------------------------

bool Agape48Engine::start()
{
    if (m_ready) {
        if (!m_tick.isActive()) {
            m_tick.start();
            emit runningChanged();
        }
        return true;
    }

    if (m_romSource.isEmpty()) {
        emit romRequired();
        setError(tr("No HP 48 ROM selected."));
        return false;
    }

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
    emit readyChanged();
    m_tick.start();
    emit runningChanged();
    return true;
}

void Agape48Engine::stop()
{
    if (!m_tick.isActive())
        return;
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

    // Deep sleep: the HP 48 spends most of its life in SHUTDN waiting for a
    // key. Stop ticking so a phone can actually idle; any key press restarts.
    if (x48_is_asleep())
        m_tick.stop();
}

// --- keys -------------------------------------------------------------------

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

void Agape48Engine::pressCode(int row, int mask)
{
    if (row < 0 || row >= X48_KB_ROWS)
        return;
    x48_key_down(row, quint16(mask));
    if (m_ready && !m_tick.isActive()) {   // wake from the deep-sleep stop above
        m_tick.start();
        emit runningChanged();
    }
}

void Agape48Engine::releaseCode(int row, int mask)
{
    if (row < 0 || row >= X48_KB_ROWS)
        return;
    x48_key_up(row, quint16(mask));
}

void Agape48Engine::releaseAllKeys()
{
    x48_key_release_all();
}

// --- state ------------------------------------------------------------------

void Agape48Engine::reset(bool cold)
{
    x48_reset(cold);
    if (m_ready && !m_tick.isActive()) {
        m_tick.start();
        emit runningChanged();
    }
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
    if (m_ready && !m_tick.isActive())
        m_tick.start();
    return true;
}

// --- misc -------------------------------------------------------------------

void Agape48Engine::setRomSource(const QUrl &url)
{
    if (m_romSource == url)
        return;
    m_romSource = url;
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
