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
#include <QTimer>
#include <QUrl>

#include "x48_shim.h"

class SkinModel;
class StateFileManager;

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

    // Aggregated sub-objects, so QML reaches everything through one root:
    //   engine.state.location, engine.skin.keys, ...
    Q_PROPERTY(StateFileManager *state READ state CONSTANT)
    Q_PROPERTY(SkinModel        *skin  READ skin  CONSTANT)

    // User preferences that live on the frontend side, not in the core.
    Q_PROPERTY(bool hapticsEnabled READ hapticsEnabled WRITE setHapticsEnabled NOTIFY hapticsEnabledChanged)
    Q_PROPERTY(bool soundEnabled   READ soundEnabled   WRITE setSoundEnabled   NOTIFY soundEnabledChanged)

public:
    explicit Agape48Engine(QObject *parent = nullptr);
    ~Agape48Engine() override;

    bool isReady() const     { return m_ready; }
    bool isRunning() const   { return m_tick.isActive(); }
    QUrl romSource() const   { return m_romSource; }
    int  contrast() const    { return m_frame.contrast; }
    int  annunciators() const{ return m_annunciators; }
    QString lastError() const{ return m_lastError; }
    StateFileManager *state() const { return m_state; }
    SkinModel        *skin()  const { return m_skin; }
    bool hapticsEnabled() const { return m_haptics; }
    bool soundEnabled() const   { return m_sound; }

    void setRomSource(const QUrl &url);
    void setHapticsEnabled(bool on);
    void setSoundEnabled(bool on);

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
    // form is what a KML skin carries. Both land on the same matrix write.
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
    void hapticsEnabledChanged();
    void soundEnabledChanged();

    void frameReady();                      // LcdItem listens; fires only on change
    void beep(int frequencyHz, int durationMs);
    void keyFeedback(const QString &keyId); // QML plays haptics/sound off this
    void romRequired();                     // no ROM yet - QML shows the picker

private:
    void tick();
    void setError(const QString &what);
    bool lookupKey(const QString &keyId, int *row, int *mask) const;

    QTimer            m_tick;
    x48_frame_t       m_frame {};
    quint64           m_frameSerial = 0;
    int               m_annunciators = 0;
    bool              m_ready = false;
    bool              m_haptics = true;
    bool              m_sound = true;
    QUrl              m_romSource;
    QString           m_lastError;
    StateFileManager *m_state = nullptr;
    SkinModel        *m_skin  = nullptr;
};
