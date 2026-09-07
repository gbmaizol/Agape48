// ---------------------------------------------------------------------------
// StateFileManager - "bring your own sync".
//
// The HP 48 state is a handful of raw byte images (ram, port1, port2, state).
// They are byte-identical across Windows, Linux and Android, so syncing them is
// just syncing files - no REST client, no QtNetwork, no account. The user picks
// a directory; whatever they have watching that directory does the transport.
//
//   Desktop : a plain path, e.g. ~/Dropbox/Agape48 . Handed to the core as
//             cfg.state_dir.
//   Android : a content:// TREE uri from ACTION_OPEN_DOCUMENT_TREE, with a
//             persisted permission grant. A tree uri has no POSIX path, so the
//             files are opened here via SAF and handed to the core as open
//             file descriptors (cfg.fd_*).
//
// The hard part is not the transport, it is the conflict: two devices editing
// one .ram between syncs. See conflictDetected() and README "Sync conflicts".
// ---------------------------------------------------------------------------
#pragma once

#include <QDir>
#include <QHash>
#include <QObject>
#include <QPair>
#include <QTimer>
#include <QVariantMap>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

// x48_shim.h now tags the struct, so this can stay a forward declaration and
// keep the C header out of every translation unit that includes this one. A
// bare "struct x48_config_t" does not exist - only the typedef does, which is
// why the previous version of this line did not compile.
struct x48_config_s;
using x48_config_t = x48_config_s;

class QFileSystemWatcher;

class StateFileManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reached through Agape48Engine.state")

    Q_PROPERTY(QUrl    location    READ location    WRITE setLocation NOTIFY locationChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY locationChanged)
    Q_PROPERTY(bool    writable    READ isWritable  NOTIFY locationChanged)
    Q_PROPERTY(bool    isDefault   READ isDefault   NOTIFY locationChanged)
    Q_PROPERTY(QString lastError   READ lastError   NOTIFY lastErrorChanged)

    // Which calculator inside the state folder is open. The state folder is a
    // shelf, not a calculator: it holds one shared ROM and a subfolder per
    // calculator, each with its own ram, hp48, ports and en-uzo lock. Two
    // calculators at once is two subfolders, and the one-instance-per-folder
    // rule from 2026aug30 now applies per calculator rather than per shelf.
    Q_PROPERTY(QString instance READ instance NOTIFY instanceChanged)

public:
    explicit StateFileManager(QObject *parent = nullptr);

    QUrl location() const { return m_location; }
    QString displayName() const;
    bool isWritable() const;
    bool isDefault() const;
    QString lastError() const { return m_lastError; }

    // mustClaim false: switch even if the calculator there is somebody else's,
    // which is what joining a shared folder means.
    void setLocation(const QUrl &url, bool mustClaim = true);

    // --- used by Agape48Engine, not QML ------------------------------------
    // Fills either cfg->state_dir (desktop; the QByteArray keeps the bytes
    // alive for the caller) or cfg->fd_* (Android SAF). Returns false and sets
    // lastError() if the location is unusable.
    bool populate(x48_config_t *cfg, QByteArray *stateDirStorage);

    // Called after a successful core save: records the new fingerprint so the
    // next external change is detectable.
    bool commit(quint64 fingerprint);

    // The baseline for "did anybody else touch these files": the size and
    // mtime of every state file as it is on disk right now. Called after our
    // own writes and after a reload, so that whatever the watcher reports
    // afterwards is somebody else's doing and not ours.
    void noteStateOnDisk();

    bool hasExternalChange() const;

    // --- one calculator, one instance --------------------------------------
    // The state folder IS the calculator, so it is opened by one instance at a
    // time, the way a word processor opens a document. claim() writes an
    // "en-uzo" file naming this process and this machine; a second instance
    // finds it and is turned away. Two calculators at once is two folders,
    // which the folder picker already does. Gert asked for this on 2026aug30
    // after two copies pointed at one folder quietly ate each other's memory -
    // every instance writes the whole state on quit, so the last one out won.
    //
    // Local files only. An Android content:// tree would need the whole SAF
    // dance to write one small file, and Android will not run two copies of an
    // app anyway.
    bool claim(bool takeOver = false);
    void release();

    // Who holds the calculator we could not claim, for the dialog to explain:
    // { host, sameMachine, alive, since, seen, quietMinutes }. Times are stored
    // UTC and handed over as local QDateTime, because two machines' clocks
    // differ - especially a laptop that has been suspended.
    Q_INVOKABLE QVariantMap lockHolder() const;
    Q_INVOKABLE QVariantMap lockHolderOf(const QString &instance) const;

    // --- asking for a calculator somebody else has -------------------------
    // The request is a file in that calculator's own folder, so it reaches
    // another machine the same way everything else does - through whatever the
    // user has syncing that folder. The answer is the lock file disappearing.
    // Better than taking it over rather than merely politer: a take-over makes
    // the loser drop the calculator without saving.
    Q_INVOKABLE bool requestSleep(const QString &instance);
    Q_INVOKABLE void withdrawSleepRequest(const QString &instance);
    Q_INVOKABLE bool isHeldBySomebody(const QString &instance) const;

    // Has the calculator we asked for actually ARRIVED, or only been let go of?
    //
    // Gert, dogfood both-03 line 19: "Because the handover file is the smallest,
    // it came first, so Linux thought they were all updated. The handover should
    // come with a hash, and the calculator that's taking over should wait for a
    // full match, meaning that all the files arrived, before considering the
    // takeover complete and loading the memory."
    //
    // The lock is 96 bytes and the memory is 131,072, so through a sync client
    // the lock's DELETION lands well before the files it was protecting. The
    // waiter saw it go, read the folder, and got a new hp48 against a stale ram
    // - which is a calculator whose objects do not parse, and is why an
    // "External" appeared on his stack.
    //
    // So the answer is not the lock going. The answer is a "contents" file
    // naming the request it answers and carrying a sha256 of every file the
    // releasing machine wrote, and this returns true only when that file is
    // there, is addressed to US, and every digest in it matches the bytes on
    // disk. It is deliberately generous when nobody owes us anything: see the
    // definition.
    Q_INVOKABLE bool handoverComplete(const QString &instance) const;

    // The same question, answered with WHY rather than with no. One false hid
    // three quite different situations, and the wait dialog reported the worst
    // of them for all three - dogfood both-05 line 15, where the countdown ran
    // out, the other machine let go eight seconds later, and the screen went on
    // offering "Take it over" for a folder whose ram was still the one from
    // before the handover.
    //
    //   Complete  nothing is owed, or the record is here, whole, and ours
    //   Arriving  the record describes files that are NOT all here yet. Half a
    //             delivery. Nothing may read this folder, by any route.
    //   NotOurs   whole and readable, but written before we asked, or for
    //             somebody else. Taking it costs their last few seconds.
    //   Nothing   they let go and wrote no record at all
    enum HandoverState { Complete, Arriving, NotOurs, Nothing };
    Q_ENUM(HandoverState)
    HandoverState handoverState(const QString &instance) const;

    // True if that folder already holds somebody's calculator. Pointing at one
    // that does means joining it, not copying over it.
    Q_INVOKABLE bool shelfHasCalculators(const QUrl &shelf) const;
    bool isHeld() const { return m_held; }

    // True if a LIVE instance on this machine holds that folder. Used by
    // claim(), and by migrateTo() before it copies anything - the check has to
    // come first there, or a refused move has already overwritten the memory
    // of the instance it was refused for.
    bool isBusy(const QUrl &url) const;

    // --- calculators inside the state folder --------------------------------
    QString instance() const { return m_instance; }

    // One entry per calculator: { name, lastUsed (QDateTime), inUse (bool),
    // heldBy (QString, empty unless inUse) }, most recently used first.
    Q_INVOKABLE QVariantList instances() const;

    // Switch to another calculator. Releases the current one first, and comes
    // back false with lastError() set if the new one is held by somebody.
    // takeOver is the answer of last resort, for a lock nobody will ever come
    // back to clear: it claims the calculator whether or not somebody holds it.
    Q_INVOKABLE bool openInstance(const QString &name, bool takeOver = false);

    // Make a fresh calculator and switch to it. Returns its name, or empty.
    Q_INVOKABLE QString createInstance();

    Q_INVOKABLE bool renameInstance(const QString &from, const QString &to);

    // location/<instance>, which is what the core is given as its state_dir.
    QString instanceDir() const;

    // location/<name>, for a calculator that is not the open one.
    QString instancePath(const QString &instance) const;

public slots:
    // Desktop: emits pickerRequested() so QML can show a folder chooser
    // (QtQuick.Dialogs FolderDialog - part of Quick, not Widgets).
    // Android: fires the SAF intent through the Java helper.
    void requestLocation();

    // Reverts to QStandardPaths::AppDataLocation.
    void useDefaultLocation();

    // Copies the current state into a newly chosen location, so switching to a
    // synced folder does not silently start from a blank calculator.
    bool migrateTo(const QUrl &destination);

signals:
    void locationChanged();
    void instanceChanged();
    // Another instance decided we were gone and took the calculator. Only ever
    // fires after a take-over, which is always somebody's deliberate choice.
    void lockLost();
    // Another instance asked for the calculator we are holding. Agape48Engine
    // saves it, lets go, and says who asked.
    void sleepRequested(const QString &byHost);
    void lastErrorChanged();
    void pickerRequested();
    void externalChangeDetected();
    void conflictDetected(const QString &detail);

private:
    bool populateDesktop(x48_config_t *cfg, QByteArray *storage);
    bool populateAndroidSaf(x48_config_t *cfg);
    void loadPersistedLocation();
    void beat();
    // The other half of the sync rule Gert set on 2026aug30: a calculator can
    // have its files changed underneath it while it is open - a sync client
    // landing another machine's copy, or a file dropped in by hand - and
    // racing with that is how a memory image gets torn in half. The watcher
    // says when; Agape48Engine decides what to do about it.
    void watchFiles();
    void settle();
    bool filesChangedOnDisk() const;
    // Which calculator on that shelf to open. Takes the folder explicitly
    // because joining one has to choose before m_location moves.
    void prepareInstances(const QUrl &where = QUrl());
    // The shelf used when the user has not chosen one. A subfolder of the app's
    // data folder on Android, where Qt's own config directory lives inside it;
    // the data folder itself everywhere else. See the definition.
    static QString defaultLocationPath();
    QString freeInstanceName() const;
    // The same question asked of a folder that is not ours yet, which is what
    // migrating onto somebody else's shelf needs.
    static QString freeNameIn(const QDir &base);
    bool busyAt(const QString &dir) const;
    void persistLocation();
    void setError(const QString &what);

    QUrl    m_location;
    QString m_instance;
    // The request we are answering by letting go, as "<host>/<pid>", so the
    // machine that asked can tell OUR handover from a save that happened to
    // land at the same moment. Set when the request is honoured, written into
    // the contents file, and cleared the moment it has been.
    QString m_answering;
    // The other side of the same conversation: what we are waiting for, and
    // whether anybody is actually there to answer. Decided when the request
    // goes out, while the lock is still readable - afterwards it is gone, and
    // "the lock vanished" cannot tell a saved handover from a dead process's
    // leftovers.
    QString m_askedFor;
    bool    m_expectAnswer = false;
    quint64 m_lastFingerprint = 0;
    QString m_lastError;
    bool    m_held = false;
    QString m_heldPath;
    QTimer  m_heartbeat;

    QFileSystemWatcher *m_watch = nullptr;
    // A sync client lands a folder as a burst of separate files, so the first
    // event is never the last one. Coalesce, then look once.
    QTimer  m_settle;
    // file name -> (size, mtime in ms). Size and mtime rather than a hash: the
    // point is to tell OUR write apart from somebody else's, not to checksum
    // 128 KB of RAM on every event.
    QHash<QString, QPair<qint64, qint64>> m_stamp;
};
