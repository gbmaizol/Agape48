#include "StateFileManager.h"

#include "x48_shim.h"

#include <algorithm>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QSettings>
#include <QStandardPaths>
#include <QSysInfo>

#ifdef Q_OS_WIN
#  include <windows.h>
#else
#  include <csignal>
#  include <cerrno>
#endif

#ifdef Q_OS_ANDROID
#  include <QCoreApplication>
#  include <QJniEnvironment>
#  include <QJniObject>
#  include <QtCore/qnativeinterface.h>
#endif

namespace {
constexpr auto kSettingsKey = "state/location";
constexpr auto kFingerprintKey = "state/fingerprint";
constexpr auto kInstanceKey    = "state/instance";
// Once a minute against a fifteen-minute expiry, which is fifteen missed beats
// before anything is concluded. One a second was the first proposal and is far
// too much FOR A SYNCED FOLDER - 28,800 writes and as many sync events over an
// eight-hour session, on a file whose entire content is "still here". Local
// disk would not care; Dropbox and an Android radio would.
constexpr int kHeartbeatMs = 60 * 1000;
constexpr int kQuietMinutes = 15;

// Everything that has to travel when the state folder moves. It used to be
// the Storage Access Framework's own list, which was positional and named the
// state file "state" where the C core names it "hp48" - so migrating copied
// "ram", looked in vain for a "state", and left the ROM behind. The folder it
// produced could not boot: dogfood #8, where a moved state folder gave "No HP
// 48 ROM selected" on the next start, with the message hidden behind the
// settings window that the same failure had opened.
const char *const kMigrateFiles[] = { "rom", "ram", "hp48", "port1", "port2" };

// kWatchedFiles is what a calculator IS: change any of them and the machine in
// memory and the machine on disk are two different calculators. The ROM is not
// here - it does not change, and a sync client re-landing an
// identical ROM is not a reason to stop.
const char *const kWatchedFiles[] = { "ram", "hp48", "port1", "port2" };

// Long enough that a sync client landing four files is one event rather than
// four, short enough to beat a person reaching for a key.
constexpr int kSettleMs = 500;

// One instance asking another to save the calculator and let go. There is no
// channel between two machines and there is not going to be one - the whole
// premise is a folder somebody else syncs - so the request is a small file in
// the calculator's own folder, and it travels exactly the way everything else
// does. The answer is the lock file disappearing.
constexpr auto kSleepName = "sleep-request";
// A request nobody ever answered is rubbish after a while, and obeying one
// found days later would put a calculator to sleep the moment it opened.
constexpr int kSleepStaleMinutes = 10;

// WHAT THIS MACHINE IS CALLED, in the lock files and the handover records.
//
// QSysInfo::machineHostName() is the answer on a desktop and is "localhost" on
// every Android phone ever made - the kernel's hostname, which no Android
// device sets. Gert saw it: dogfood android-09 line 22, "the phone calls itself
// localhost in the lock file, so a handover from it would say left open on
// localhost". Harmless while the phone had a shelf of its own; not harmless now
// that a phone, a laptop and a Windows box can hold calculators on one shared
// folder, where this name is the only thing telling them apart.
//
// device_name is what the owner typed into Settings, so it is the name they
// already know the phone by ("Nothing (4a) Pro do Gert" on Gert's). Build.MODEL
// is the fallback for a device that has none.
#ifdef Q_OS_ANDROID
QString androidDeviceName()
{
    QString name;
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid()) {
        const QJniObject resolver = context.callObjectMethod(
            "getContentResolver", "()Landroid/content/ContentResolver;");
        if (resolver.isValid()) {
            const QJniObject got = QJniObject::callStaticObjectMethod(
                "android/provider/Settings$Global", "getString",
                "(Landroid/content/ContentResolver;Ljava/lang/String;)Ljava/lang/String;",
                resolver.object<jobject>(),
                QJniObject::fromString(QStringLiteral("device_name")).object<jstring>());
            if (got.isValid())
                name = got.toString();
        }
    }
    if (name.trimmed().isEmpty()) {
        const QJniObject model =
            QJniObject::getStaticObjectField<jstring>("android/os/Build", "MODEL");
        if (model.isValid())
            name = model.toString();
    }
    // A lock file is line-oriented "key=value", so a newline in this name would
    // make the rest of the file unreadable to the machine that has to parse it.
    // Length is cut for the dialog that says it out loud, not for the file.
    name = name.simplified().left(40);
    return name.isEmpty() ? QStringLiteral("Android") : name;
}
#endif

// THE FOLDER PICKER HANDS BACK A content:// TREE, AND THE CORE NEEDS A PATH.
//
// This is the whole of dogfood android-09 line 24. Gert put the shared shelf on
// the phone at /sdcard/Documents/Agape48Emulator/TestShelf, pressed the "..."
// button, picked it, allowed it - and got "Migration between local and SAF
// storage is not implemented yet", which is what this program used to say to
// anything that was not a plain path. "I believe no progress is possible while
// [that]. Please do fix it."
//
// A tree uri is not as opaque as it looks. The system's own storage provider
// builds it out of a volume and a relative path -
//
//   content://com.android.externalstorage.documents/tree/primary%3ADocuments%2FX
//                                                        \_____/ \__________/
//                                                        volume    path in it
//
// - so it can be turned back into /storage/emulated/0/Documents/X, and then
// every line of this file works on it exactly as it does on a desktop: the
// shelf, the per-calculator folders, the locks, the handover records and the
// watcher. That is worth far more than the four open file descriptors the SAF
// path used to produce, which the C core never read (x48_config_t::fd_ram and
// its three neighbours are declared and used nowhere) - so that path was
// fiction from end to end, and it is gone.
//
// Done with QUrl rather than DocumentsContract.getTreeDocumentId() on purpose:
// that method throws IllegalArgumentException at anything that is not a tree
// uri, and a pending Java exception has to be found and cleared before the next
// JNI call or it surfaces somewhere else entirely. QUrl::path() already decodes
// the %3A and the %2F, and it can be tested on a desktop, which JNI cannot.
QUrl localised(const QUrl &picked)
{
    if (picked.isLocalFile() || picked.scheme() != QLatin1String("content"))
        return picked;
#ifdef Q_OS_ANDROID
    // Only the OS's own storage provider names files that exist on this device.
    // Google Drive, Dropbox and the rest are content providers over a network;
    // there is no path behind them and there is not going to be one.
    if (picked.host() != QLatin1String("com.android.externalstorage.documents"))
        return {};
    const QString path = picked.path();
    const int tree = path.indexOf(QLatin1String("/tree/"));
    if (tree < 0)
        return {};
    QString id = path.mid(tree + 6);
    // Some providers append the document inside the tree; the tree is the part
    // before it, and the tree is what was granted.
    const int doc = id.indexOf(QLatin1String("/document/"));
    if (doc >= 0)
        id = id.left(doc);
    const int colon = id.indexOf(QLatin1Char(':'));
    if (colon < 0)
        return {};
    const QString volume = id.left(colon);
    const QString inside = id.mid(colon + 1);
    QString base;
    if (volume == QLatin1String("primary")) {
        const QJniObject dir = QJniObject::callStaticObjectMethod(
            "android/os/Environment", "getExternalStorageDirectory",
            "()Ljava/io/File;");
        if (dir.isValid()) {
            const QJniObject abs =
                dir.callObjectMethod("getAbsolutePath", "()Ljava/lang/String;");
            if (abs.isValid())
                base = abs.toString();
        }
    } else {
        // An SD card, mounted under its own volume id.
        base = QStringLiteral("/storage/") + volume;
    }
    if (base.isEmpty())
        return {};
    return QUrl::fromLocalFile(
        inside.isEmpty() ? base : base + QLatin1Char('/') + inside);
#else
    return {};
#endif
}

// WHICH FOLDERS ARE THIS APP'S OWN, which on Android is the whole question.
//
// Scoped storage does not hand out read and write as one thing, and the way it
// fails is far worse than a refusal. Measured on Gert's phone at 22:51 on
// 2026sep09, pointing at his shared shelf with no permission granted:
//
//   the folder listed              - Documents/Agape48Emulator/TestShelf showed
//                                    its three calculators
//   a NEW file could be made in it - the en-uzo lock was written, 109 bytes,
//                                    with this device's name in it
//   an EXISTING file could not be  - "agape48: can't open .../Windows box/hp48"
//     read                           because that file belongs to whichever app
//                                    put it there
//
// So every check this program had - exists(), isWritable(), and even writing a
// probe file and deleting it again - said yes, and the calculator came up blank
// with somebody else's memory sitting unread beside it. Worse, it had claimed
// the lock, and the next save would have written a fresh machine over a
// calculator carried there from another computer.
//
// There is no probe for this. The rule is the one Android actually applies: a
// folder inside this app's own storage is ours whatever the permissions say,
// and everything else needs "all files access". These are the three roots -
// the internal data folder, /Android/data/<pkg>/files and
// /Android/media/<pkg> - asked of the system rather than spelled out here,
// because a phone with an SD card has two of each.
#ifdef Q_OS_ANDROID
// THE SHELF A PHONE STARTS WITH, and since 2026sep09 it is a folder that can
// be seen. Android/media/<package>/Agape48 calculators: a real path, no
// permission of any kind, the app's own - and unlike the app's data folder, not
// hidden from every file manager on the phone. Gert, dogfood android-08 line 7:
// "I don't have access to the internal calculator folder, and I can't change it
// to a visible folder before you implement this possibility." Now nothing has
// to be changed for it to be visible; it starts that way.
//
// IT IS NOT DURABLE, AND THAT IS MEASURED, NOT ASSUMED. On this phone (Android
// 16) uninstalling the app deleted /sdcard/Android/media/br.gbmaizol.agape48
// whole, marker file and all - which is the other half of what happened to him:
// "I even tried to uninstall it and install again, but this caused the newly
// installed version to have no memory folder". Visible is not the same as safe.
// A calculator that must outlive the app belongs in a folder of the user's own,
// which is what the picker and canUseAnyFolder() are for.
//
// A SUBFOLDER, not the media directory itself: the shelf adopts every
// subdirectory it finds as a calculator, and the media scanner and other apps
// both write into that directory - the same mistake the old default made once,
// when Qt's own "settings" folder appeared on the shelf wearing the name of a
// calculator.
#ifdef Q_OS_ANDROID
QString androidMediaShelf()
{
    QJniEnvironment env;
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return {};
    // File[], one per storage volume - internal first, then any SD card. The
    // first one that exists and can be written to wins; a phone with no card
    // returns a single entry, and an unmounted volume returns a null one.
    const QJniObject dirs =
        context.callObjectMethod("getExternalMediaDirs", "()[Ljava/io/File;");
    if (!dirs.isValid())
        return {};
    const auto array = dirs.object<jobjectArray>();
    if (!array)
        return {};
    const jsize count = env->GetArrayLength(array);
    for (jsize i = 0; i < count; ++i) {
        const QJniObject dir(env->GetObjectArrayElement(array, i));
        if (!dir.isValid())
            continue;
        const QJniObject path =
            dir.callObjectMethod("getAbsolutePath", "()Ljava/lang/String;");
        if (!path.isValid())
            continue;
        const QString shelf =
            path.toString() + QStringLiteral("/Agape48 calculators");
        if (!QDir().mkpath(shelf))
            continue;
        if (!QFileInfo(shelf).isWritable())
            continue;
        return shelf;
    }
    return {};
}
#endif

QStringList ownStorageRoots()
{
    QStringList roots;
    const QString internal =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (!internal.isEmpty())
        roots << internal;
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (context.isValid()) {
        QJniEnvironment env;
        const QJniObject dirs[] = {
            context.callObjectMethod("getExternalFilesDirs",
                                     "(Ljava/lang/String;)[Ljava/io/File;",
                                     static_cast<jstring>(nullptr)),
            context.callObjectMethod("getExternalMediaDirs", "()[Ljava/io/File;")
        };
        for (const QJniObject &list : dirs) {
            if (!list.isValid())
                continue;
            const auto array = list.object<jobjectArray>();
            if (!array)
                continue;
            const jsize count = env->GetArrayLength(array);
            for (jsize i = 0; i < count; ++i) {
                const QJniObject dir(env->GetObjectArrayElement(array, i));
                if (!dir.isValid())
                    continue;   // an unmounted card comes back null
                const QJniObject abs =
                    dir.callObjectMethod("getAbsolutePath", "()Ljava/lang/String;");
                if (abs.isValid())
                    roots << abs.toString();
            }
        }
    }
    return roots;
}

bool withinOwnStorage(const QString &path)
{
    // Once: they cannot change while the process lives, and this is asked on
    // every start.
    static const QStringList roots = ownStorageRoots();
    const QString clean = QDir::cleanPath(path);
    for (const QString &root : roots) {
        const QString r = QDir::cleanPath(root);
        if (!r.isEmpty() && (clean == r || clean.startsWith(r + QLatin1Char('/'))))
            return true;
    }
    return false;
}
#endif

// isWritable() asks the filesystem's opinion; this asks the filesystem. On
// Android the two disagree: a folder in shared storage that the app has no
// permission for is reported writable by stat and refuses every open, so a
// migration would delete its way through half a calculator before finding out.
// One file, made and removed, before anything is copied.
bool canWriteInto(const QDir &dir)
{
    QFile probe(dir.filePath(QStringLiteral(".agape48-write-test")));
    if (!probe.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const bool ok = probe.write("x", 1) == 1;
    probe.close();
    probe.remove();
    return ok;
}

QString thisHost()
{
#ifdef Q_OS_ANDROID
    static const QString name = androidDeviceName();
    return name;
#else
    return QSysInfo::machineHostName();
#endif
}

// Comparing rather than equality, because of what the phone used to write.
// Every Agape48 built before 2026sep09 put "localhost" in its lock files on
// Android, and a lock this build does not recognise as its own is a calculator
// the phone would refuse to open - on the very upgrade that gave it a name. No
// real machine on a shared shelf is called localhost; the ambiguity is only
// with another Android that has not been upgraded yet, which would be this
// phone's own past self.
bool isThisHost(const QString &host)
{
    if (host == thisHost())
        return true;
#ifdef Q_OS_ANDROID
    return host == QLatin1String("localhost");
#else
    return false;
#endif
}
} // namespace

StateFileManager::StateFileManager(QObject *parent)
    : QObject(parent)
{
    m_heartbeat.setInterval(kHeartbeatMs);
    m_heartbeat.setTimerType(Qt::VeryCoarseTimer);
    connect(&m_heartbeat, &QTimer::timeout, this, &StateFileManager::beat);
    m_settle.setSingleShot(true);
    m_settle.setInterval(kSettleMs);
    connect(&m_settle, &QTimer::timeout, this, &StateFileManager::settle);
    loadPersistedLocation();
}

void StateFileManager::loadPersistedLocation()
{
    QSettings s;
    const QString stored = s.value(QLatin1String(kSettingsKey)).toString();
    m_lastFingerprint = s.value(QLatin1String(kFingerprintKey)).toULongLong();
    if (!stored.isEmpty()) {
        // An older build could store a content:// tree here. It can be turned
        // into a path now; if it cannot, the default is a working calculator
        // and a stored uri that nothing can open is not.
        const QUrl saved = QUrl(stored);
        m_location = saved.isLocalFile() ? saved : localised(saved);
        if (m_location.isLocalFile()) {
            prepareInstances();
            return;
        }
    }
    useDefaultLocation();
}

void StateFileManager::persistLocation()
{
    QSettings s;
    s.setValue(QLatin1String(kSettingsKey), m_location.toString());
}

// AppLocalData, not AppData. They are the same directory on Linux and Android
// and two different ones on Windows: AppDataLocation is AppData\Roaming, which
// a domain-joined machine's policy may sync between the user's computers all by
// itself. This folder holds a 512 KB ROM, a 128 KB memory image and an "en-uzo"
// file naming one host and one pid - roaming it would carry a calculator
// between machines behind the back of the very lock that exists to stop two
// machines sharing one, and with none of the conflict handling the state folder
// gets. Carrying the calculator between machines is what the user-chosen synced
// folder is FOR; it should not also happen by accident.
//
// Gert's Windows laptop is Azure-AD joined, which is what raised it. Only fresh
// installs move: the location is written to QSettings on first run, so anything
// already running keeps the folder it has.
// A SUBFOLDER ON ANDROID, the data folder itself everywhere else.
//
// The shelf is a folder whose subdirectories are calculators, so it has to be a
// folder nothing else writes into. On Android it was not: AppConfigLocation is
// AppLocalDataLocation + "/settings" there, so the moment QSettings saved
// anything it created a directory called "settings" inside the shelf and the
// program adopted it as a calculator. Measured on the phone at ef76dd2 - the
// nameplate read "settings", which is Qt's own config directory wearing the
// name of a calculator.
//
// Desktop is unaffected: the config folder is in a different tree there, and
// changing this on Windows or Linux would move everybody's calculators.
QString StateFileManager::defaultLocationPath()
{
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
#ifdef Q_OS_ANDROID
    // Asked once. It cannot change while the process lives, and isDefault()
    // asks it on every repaint of the settings page.
    static const QString shelf = androidMediaShelf();
    // The fallback is the folder this used to be: a phone with no external
    // storage mounted at all still has to start.
    return shelf.isEmpty() ? base + QStringLiteral("/calculators") : shelf;
#else
    return base;
#endif
}

// ANDROID KEEPS AN APP OUT OF THE USER'S OWN FOLDERS unless the user says
// otherwise, once, in the system settings. Everything Agape48 does by default
// stays inside its own storage and asks for nothing - the folder it starts in,
// and the Android/media folder the button below hands out, are both the app's
// own. The moment the user wants the calculators in a folder that a sync client
// already watches - Gert's is Documents/Agape48Emulator - that is somebody
// else's storage, and Android has exactly one answer for an app that needs to
// read and write arbitrary folders: MANAGE_EXTERNAL_STORAGE, granted by hand on
// a system screen, revocable there at any time.
//
// It is declared in the manifest and requested nowhere else, which is the rule
// Gert set on 2026sep06: "Remember to request the required permissions for the
// APK, but make them be requested to the system as they become required. For
// example, only if the user desires to change to a folder outside the sandbox
// to integrate Dropbox, and so on." Nothing asks for it at startup; nothing
// asks for it to open the folder the app starts in; the settings page offers it
// only when a chosen folder turns out to need it.
bool StateFileManager::canUseAnyFolder() const
{
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<jboolean>(
        "android/os/Environment", "isExternalStorageManager", "()Z");
#else
    // Every desktop lets a program open the folders its user can open.
    return true;
#endif
}

void StateFileManager::requestAnyFolderAccess()
{
#ifdef Q_OS_ANDROID
    const QJniObject context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
        return;
    const QJniObject package =
        context.callObjectMethod("getPackageName", "()Ljava/lang/String;");
    if (!package.isValid())
        return;
    // ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION with our own package in the
    // data uri, which opens the switch for THIS app. The list-of-every-app
    // screen is the version without the uri, and it makes the user find us.
    const QJniObject uri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString(QStringLiteral("package:") + package.toString())
            .object<jstring>());
    if (!uri.isValid())
        return;
    QJniObject intent(
        "android/content/Intent", "(Ljava/lang/String;Landroid/net/Uri;)V",
        QJniObject::fromString(
            QStringLiteral("android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION"))
            .object<jstring>(),
        uri.object<jobject>());
    if (!intent.isValid())
        return;
    context.callMethod<void>("startActivity", "(Landroid/content/Intent;)V",
                             intent.object<jobject>());
#endif
}

QUrl StateFileManager::sharedLocation()
{
    setError(QString());
#ifdef Q_OS_ANDROID
    const QString shelf = androidMediaShelf();
    if (!shelf.isEmpty())
        return QUrl::fromLocalFile(shelf);
    // Every volume refused. It says so rather than returning empty in silence:
    // the caller is a button, and a button that does nothing and explains
    // nothing is the worst of the three outcomes.
    setError(tr("This device has no external storage to keep the calculators on."));
#endif
    return {};
}

void StateFileManager::useDefaultLocation()
{
    setError(QString());
    const QString dir = defaultLocationPath();
    QDir().mkpath(dir);
    setLocation(QUrl::fromLocalFile(dir));
    prepareInstances();
}

bool StateFileManager::isDefault() const
{
    return m_location.isLocalFile()
           && m_location.toLocalFile() == defaultLocationPath();
}

void StateFileManager::setLocation(const QUrl &url, bool mustClaim)
{
    if (m_location == url)
        return;
    // Let go of the old one first, or moving A -> B -> A finds our own lock on
    // A and refuses. If B turns out to be someone else's, we go back to A and
    // take it again, so a refused move costs the user nothing.
    const QUrl previous = m_location;
    const bool wasHeld = m_held;
    release();
    m_location = url;
    // mustClaim false is joining a shared folder: the calculator there may
    // well be in use - that is the whole reason the sleep dialog exists - and
    // bouncing back to the old folder would mean the user could never move to
    // a shelf while anybody was using it. Land there unheld instead, and let
    // start() report who has it.
    if (mustClaim && wasHeld && !claim()) {
        m_location = previous;
        claim();
        return;                     // lastError() already says why
    }
    persistLocation();
    emit locationChanged();
}

// --- one calculator, one instance -------------------------------------------

namespace {

constexpr auto kLockName = "en-uzo";

bool processAlive(qint64 pid)
{
    if (pid <= 0)
        return false;
#ifdef Q_OS_WIN
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, DWORD(pid));
    if (!h)
        return false;
    const bool running = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return running;
#else
    // EPERM means it exists and belongs to somebody else, which still counts.
    return ::kill(pid_t(pid), 0) == 0 || errno == EPERM;
#endif
}

struct LockInfo {
    bool    present = false;
    qint64  pid = 0;
    QString host;
    QString started;      // UTC ISO-8601, when the calculator was opened
    QString seen;         // UTC ISO-8601, refreshed while it is still open
};


LockInfo readLock(const QString &path)
{
    LockInfo in;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return in;
    in.present = true;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq < 0)
            continue;
        const QString key = line.left(eq), val = line.mid(eq + 1);
        if      (key == QLatin1String("pid"))     in.pid = val.toLongLong();
        else if (key == QLatin1String("host"))    in.host = val;
        else if (key == QLatin1String("started")) in.started = val;
        else if (key == QLatin1String("seen"))    in.seen = val;
    }
    return in;
}

struct SleepReq {
    bool    present = false;
    qint64  pid = 0;
    QString host;
    QDateTime at;      // UTC
};

SleepReq readSleep(const QString &path)
{
    SleepReq r;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return r;
    r.present = true;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq < 0)
            continue;
        const QString key = line.left(eq), val = line.mid(eq + 1);
        if      (key == QLatin1String("pid"))  r.pid = val.toLongLong();
        else if (key == QLatin1String("host")) r.host = val;
        else if (key == QLatin1String("at"))   r.at = QDateTime::fromString(val, Qt::ISODate);
    }
    return r;
}

// Ours means this pid ON THIS MACHINE. pid alone was enough while both windows
// were local; across a synced folder it is not - two machines number their
// processes independently, and a collision is not impossible over a long
// session. Getting this wrong means a window that has lost the calculator
// carries on believing it holds it.
bool lockIsOurs(const LockInfo &in)
{
    return in.present && in.pid == QCoreApplication::applicationPid()
           && isThisHost(in.host);
}

// A calculator, as opposed to an empty folder somebody made by hand: it has at
// least one of the files a calculator is made of. An empty folder of the same
// name is not something to step around.
bool occupied(const QDir &shelf, const QString &calc)
{
    if (!shelf.exists(calc))
        return false;
    const QDir dir(shelf.filePath(calc));
    for (const char *leaf : kWatchedFiles)
        if (dir.exists(QLatin1String(leaf)))
            return true;
    return false;
}

// --- what is IN the folder, as opposed to who holds it ----------------------
//
// A sync client delivers a folder one file at a time, cheapest first, and the
// lock is the cheapest thing in it. So the machine waiting for a handover sees
// the lock go while the memory it was protecting is still crossing, reads the
// folder, and gets half of one calculator and half of another. Gert found it
// in dogfood both-03 line 19 and asked for exactly this: the handover comes
// with a hash, and the taker waits for a full match before loading.
//
// HASHES, not sizes or mtimes, and that is measured rather than assumed. ram
// is always exactly 131,072 bytes, so size tells nobody anything; and Dropbox
// truncates sub-second mtimes in the Windows-to-Linux direction only, so a
// writer that recorded 15:23:05.025 would be waited on for ever by a receiver
// that can only ever see 15:23:05.000. Content is the only thing that survives
// the crossing intact.
constexpr auto kContentsName = "contents";

QString digestOfFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f))
        return QString();
    return QString::fromLatin1(h.result().toHex());
}

// Who this instance is, in the one form both sides of a handover can compare.
// host AND pid: two machines number their processes independently, and the
// whole point of the tag is to be unmistakably ours.
QString instanceTag()
{
    return thisHost() + QLatin1Char('/')
           + QString::number(QCoreApplication::applicationPid());
}

struct Contents {
    bool    present = false;
    QString by;
    QString answers;                    // "<host>/<pid>" this handover is for
    QHash<QString, QString> digest;     // leaf name -> sha256, hex
};

Contents readContents(const QString &path)
{
    Contents c;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return c;
    c.present = true;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq < 0)
            continue;
        const QString key = line.left(eq), val = line.mid(eq + 1);
        if      (key == QLatin1String("by"))      c.by = val;
        else if (key == QLatin1String("answers")) c.answers = val;
        else if (key == QLatin1String("at"))      ;   // for a human reading it
        else                                      c.digest.insert(key, val);
    }
    return c;
}

// answering is empty for an ordinary save. It is only filled in when this write
// IS the answer to somebody's request, which is what lets the asker tell the
// handover it is waiting for apart from a save that happened to land at the
// same moment - a stale contents file cannot carry a tag written after it.
bool writeContents(const QDir &dir, const QString &answering)
{
    QString body = QStringLiteral("agape48-contents 1\nby=%1\nat=%2\n")
                       .arg(thisHost(),
                            QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    if (!answering.isEmpty())
        body += QStringLiteral("answers=%1\n").arg(answering);
    for (const char *leaf : kWatchedFiles) {
        const QString d = digestOfFile(dir.filePath(QLatin1String(leaf)));
        if (!d.isEmpty())
            body += QStringLiteral("%1=%2\n").arg(QLatin1String(leaf), d);
    }
    QFile f(dir.filePath(QLatin1String(kContentsName)));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;                   // read-only folder: populate() says so
    f.write(body.toUtf8());
    return true;
}

// Only what the record actually claims. A port1 the record says nothing about
// is a card this machine has and the writer never had, which is not a torn
// delivery and must not be treated as one - the wait would never end.
bool contentsMatch(const QDir &dir, const Contents &c)
{
    for (auto it = c.digest.cbegin(); it != c.digest.cend(); ++it)
        if (digestOfFile(dir.filePath(it.key())) != it.value())
            return false;
    return true;
}

} // namespace

bool StateFileManager::busyAt(const QString &dirPath) const
{
#ifdef Q_OS_ANDROID
    // ANDROID RUNS ONE COPY OF AN APP, so the question this asks - is a SECOND
    // Agape48 on THIS device holding that folder - has one answer there, and it
    // is no. Another device's lock is a different question and is asked in
    // claim().
    //
    // Answering it the desktop way is not merely pointless on a phone, it is
    // wrong. The system kills this process whenever it likes and the lock
    // outlives it; the pid in that lock is then reused, quickly, by some other
    // app; and /proc is hidden between apps, so kill(pid, 0) comes back EPERM -
    // "it exists and belongs to somebody else, which still counts" - about a
    // pid that belongs to a browser. The calculator would refuse to open its
    // own memory, and nothing the user could do would clear it.
    Q_UNUSED(dirPath)
    return false;
#else
    const QDir dir(dirPath);
    if (dirPath.isEmpty() || !dir.exists())
        return false;
    const LockInfo in = readLock(dir.filePath(QLatin1String(kLockName)));
    return in.present
        && isThisHost(in.host)
        && in.pid != QCoreApplication::applicationPid()
        && processAlive(in.pid);
#endif
}

// The whole shelf, not one folder. The lock moved into each calculator's own
// folder when the shelf appeared, so asking about the shelf ROOT - which is
// what this did - asks about a file that is never there any more, and
// migrateTo() has been unguarded ever since. It writes into every calculator on
// the shelf, so the question it needs answered is about every one of them.
bool StateFileManager::isBusy(const QUrl &url) const
{
    if (!url.isLocalFile())
        return false;
    const QDir shelf(url.toLocalFile());
    if (busyAt(shelf.absolutePath()))
        return true;                    // a pre-shelf folder, still possible
    const QStringList calcs = shelf.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &calc : calcs)
        if (busyAt(shelf.filePath(calc)))
            return true;
    return false;
}

// --- calculators inside the state folder ------------------------------------

QString StateFileManager::instanceDir() const
{
    if (!m_location.isLocalFile())
        return QString();               // Android SAF keeps the flat layout
    const QString base = m_location.toLocalFile();
    return m_instance.isEmpty() ? base : QDir(base).filePath(m_instance);
}

QString StateFileManager::freeInstanceName() const
{
    return freeNameIn(QDir(m_location.toLocalFile()));
}

QString StateFileManager::freeNameIn(const QDir &base)
{
    for (int n = 1; n < 1000; ++n) {
        const QString name = tr("Calculator %1").arg(n);
        if (!base.exists(name))
            return name;
    }
    return QString();
}

// Called once the location is settled. Three jobs: move a flat state folder
// from before this existed into a calculator of its own, make sure there is at
// least one calculator, and choose which one to open.
void StateFileManager::prepareInstances(const QUrl &where)
{
    const QUrl look = where.isEmpty() ? m_location : where;
    if (!look.isLocalFile()) {
        m_instance.clear();
        return;
    }
    QDir base(look.toLocalFile());
    if (!base.exists())
        return;

    // The old layout put ram and hp48 straight in the state folder. Anything
    // that finds them there predates calculators and becomes the first one.
    // The ROM is NOT moved: one shared copy at the top is the point - it is
    // half a megabyte and every calculator wants the same one.
    if (base.exists(QStringLiteral("ram")) || base.exists(QStringLiteral("hp48"))) {
        const QString name = freeNameIn(base);
        if (!name.isEmpty() && base.mkdir(name)) {
            static const char *const move[] = { "ram", "hp48", "port1", "port2",
                                                kLockName };
            for (const char *leaf : move) {
                const QString from = base.filePath(QLatin1String(leaf));
                if (QFile::exists(from))
                    QFile::rename(from, base.filePath(name + QLatin1Char('/')
                                                      + QLatin1String(leaf)));
            }
        }
    }

    QStringList found = base.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    // Somebody else's folder is not a calculator. This is the belt to the
    // Android braces above: even when the shelf is the app's own data folder -
    // which it still is for anyone who ran an earlier build, because the
    // location was written to QSettings on their first run - the config
    // directory must never be offered as a machine to work on. Hidden
    // directories are already excluded by entryList, which is what keeps a
    // synced folder's .dropbox.cache or .stfolder out.
    const QString configDir = QDir(QStandardPaths::writableLocation(
                                       QStandardPaths::AppConfigLocation))
                                  .absolutePath();
    if (!configDir.isEmpty()) {
        found.removeIf([&](const QString &n) {
            return QDir(base.filePath(n)).absolutePath() == configDir;
        });
    }

    if (found.isEmpty()) {
        const QString name = freeNameIn(base);
        if (!name.isEmpty() && base.mkdir(name))
            found << name;
    }
    if (found.isEmpty()) {
        m_instance.clear();
        return;
    }

    const QString remembered = QSettings().value(QLatin1String(kInstanceKey)).toString();
    if (found.contains(remembered)) {
        m_instance = remembered;
        return;
    }
    // Whatever is chosen below becomes the remembered one, or "the calculator
    // you used last" is only ever true within a single run.
    // No memory of one, or it has been deleted: the most recently touched.
    QString best = found.first();
    QDateTime bestAt;
    for (const QString &n : std::as_const(found)) {
        const QFileInfo fi(base.filePath(n + QStringLiteral("/hp48")));
        const QDateTime at = fi.exists() ? fi.lastModified()
                                         : QFileInfo(base.filePath(n)).lastModified();
        if (!bestAt.isValid() || at > bestAt) { bestAt = at; best = n; }
    }
    m_instance = best;
    QSettings().setValue(QLatin1String(kInstanceKey), m_instance);
}

QVariantList StateFileManager::instances() const
{
    QVariantList out;
    if (!m_location.isLocalFile())
        return out;
    const QDir base(m_location.toLocalFile());
    const QStringList names = base.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &n : names) {
        const QString dir = base.filePath(n);
        const QFileInfo fi(dir + QStringLiteral("/hp48"));
        const LockInfo in = readLock(QDir(dir).filePath(QLatin1String(kLockName)));
        out.append(QVariantMap {
            { QStringLiteral("name"), n },
            { QStringLiteral("lastUsed"), fi.exists() ? fi.lastModified()
                                                      : QFileInfo(dir).lastModified() },
            { QStringLiteral("inUse"), busyAt(dir) },
            { QStringLiteral("heldBy"), in.present ? in.host : QString() },
            { QStringLiteral("heldSince"), in.present ? in.started : QString() },
            // "this window" means we actually hold it, not merely that it is
            // the one we asked for: a window that was turned away still has
            // the name set, and saying both "this window" and "open in
            // another window" about one row helps nobody.
            { QStringLiteral("current"), m_held && n == m_instance },
        });
    }
    std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("lastUsed")).toDateTime()
             > b.toMap().value(QStringLiteral("lastUsed")).toDateTime();
    });
    return out;
}

bool StateFileManager::openInstance(const QString &name, bool takeOver)
{
    if (name == m_instance)
        return true;
    const QDir base(m_location.toLocalFile());
    if (!base.exists(name)) {
        setError(tr("There is no calculator called %1.").arg(name));
        return false;
    }
    const QString previous = m_instance;
    const bool wasHeld = m_held;
    release();
    m_instance = name;
    if (wasHeld && !claim(takeOver)) {
        m_instance = previous;
        claim();
        return false;               // lastError() already says why
    }
    QSettings().setValue(QLatin1String(kInstanceKey), m_instance);
    noteStateOnDisk();              // different folder, different files to watch
    emit instanceChanged();
    return true;
}

QString StateFileManager::createInstance()
{
    if (!m_location.isLocalFile()) {
        setError(tr("A new calculator needs a state folder on this device."));
        return QString();
    }
    QDir base(m_location.toLocalFile());
    const QString name = freeInstanceName();
    if (name.isEmpty() || !base.mkdir(name)) {
        setError(tr("Could not make a new calculator."));
        return QString();
    }
    // A new calculator starts as a copy of the one you are looking at. Gert,
    // both-05 line 37: "It also would be great if the new calculator was always
    // a clone of the calculator that's open when it's created."
    //
    // An empty folder makes the ROM build RAM from nothing, and that is the one
    // path that still ends at "Try To Recover Memory?" with no key getting past
    // it - the entry this report and the three before it all say to keep away
    // from. A clone starts from a memory image that is known to work.
    //
    // The caller has already saved and put the core down, so these are the
    // bytes the open calculator actually has. The lock and the contents record
    // are deliberately left behind: one names a process that does not hold this
    // folder, and the other describes files in a different one.
    const QString from = instanceDir();
    if (!from.isEmpty() && QDir(from).exists()) {
        const QDir src(from), dst(base.filePath(name));
        for (const char *leaf : { "ram", "hp48", "port1", "port2" }) {
            const QString one = src.filePath(QLatin1String(leaf));
            if (QFile::exists(one))
                QFile::copy(one, dst.filePath(QLatin1String(leaf)));
        }
    }
    return openInstance(name) ? name : QString();
}

bool StateFileManager::renameInstance(const QString &from, const QString &to)
{
    const QString clean = to.trimmed();
    if (clean.isEmpty() || clean.contains(QLatin1Char('/'))
            || clean.contains(QLatin1Char('\\'))) {
        setError(tr("That name cannot be used for a folder."));
        return false;
    }
    QDir base(m_location.toLocalFile());
    if (base.exists(clean)) {
        setError(tr("There is already a calculator called %1.").arg(clean));
        return false;
    }
    // Let go of the folder before moving it. Windows will not move a directory
    // that anything holds a handle on, and the file-system watcher holds one on
    // this very folder - ReadDirectoryChangesW keeps the directory itself open,
    // which is the whole point of watching it. MEASURED, 2026sep05: with the
    // watch armed, base.rename() returns false every time and the calculator
    // keeps its old name with no complaint the user can see. Linux does not
    // care, which is why this only ever showed up here.
    if (m_watch) {
        const QStringList watched = m_watch->files() + m_watch->directories();
        if (!watched.isEmpty())
            m_watch->removePaths(watched);
    }
    // Renaming the folder we are holding is fine - the lock file travels with
    // it and still names this process - but the remembered name has to follow.
    if (!base.rename(from, clean)) {
        watchFiles();                   // nothing moved: watch what is still there
        setError(tr("Could not rename %1.").arg(from));
        return false;
    }
    if (m_instance == from) {
        m_instance = clean;
        if (m_held)
            m_heldPath = QDir(base.filePath(clean)).filePath(QLatin1String(kLockName));
        QSettings().setValue(QLatin1String(kInstanceKey), m_instance);
        emit instanceChanged();
    }
    watchFiles();                       // the folder it watches has a new name
    return true;
}

bool StateFileManager::deleteInstance(const QString &name)
{
    setError(QString());
    if (!m_location.isLocalFile()) {
        setError(tr("Calculators can only be deleted from a local state folder."));
        return false;
    }
    const QString clean = name.trimmed();
    if (clean.isEmpty() || clean.contains(QLatin1Char('/'))
            || clean.contains(QLatin1Char('\\'))) {
        setError(tr("That name cannot be used for a folder."));
        return false;
    }
    // The open one. Refused here as well as disabled in the shelf - see the
    // header.
    if (clean == m_instance) {
        setError(tr("%1 is the calculator you are using. Open another one "
                    "first, then delete this one.").arg(clean));
        return false;
    }
    QDir base(m_location.toLocalFile());
    const QString path = base.filePath(clean);
    if (!QFileInfo::exists(path)) {
        setError(tr("There is no calculator called %1.").arg(clean));
        return false;
    }
    // Somebody else is mid-session in it. busyAt() only knows about live
    // processes on THIS machine, so the lock file answers for the other ones -
    // the same two-part question the shelf already asks to draw "open somewhere
    // else" beside a name.
    if (busyAt(path) || isHeldBySomebody(clean)) {
        setError(tr("%1 is open somewhere else. Close it there first.").arg(clean));
        return false;
    }
    // Stop watching before removing, for the reason renameInstance gives at
    // length: on Windows the watcher holds the directory open and the removal
    // fails with nothing the user can see.
    if (m_watch) {
        const QStringList watched = m_watch->files() + m_watch->directories();
        if (!watched.isEmpty())
            m_watch->removePaths(watched);
    }
    QDir doomed(path);
    const bool gone = doomed.removeRecursively();
    watchFiles();
    if (!gone) {
        setError(tr("Could not delete %1.").arg(clean));
        return false;
    }
    return true;
}

bool StateFileManager::claim(bool takeOver)
{
    m_held = false;
    m_heldPath.clear();
    if (!m_location.isLocalFile())
        return true;                    // SAF: see the note in the header
    const QDir dir(instanceDir());
    if (instanceDir().isEmpty() || !dir.exists())
        return true;                    // populate() says this better than we can

    const QString path = dir.filePath(QLatin1String(kLockName));
    const QString here = thisHost();
    const LockInfo in = readLock(path);

    if (!takeOver && busyAt(instanceDir())) {
        setError(tr("%1 is already open in another Agape48. Close it there, "
                    "or open a different calculator.").arg(m_instance));
        return false;
    }
    // A lock from a dead process - a crash, or a machine that went down with
    // it open - means nothing is reading the folder. Take it over, say nothing.

    // isThisHost(), not "!= here", and the difference is a phone that cannot
    // open its own calculator. The name this device puts in a lock file changed
    // on 2026sep09 - "localhost" for every build before it - so on the upgrade
    // itself the lock left behind by the previous run is written by a machine
    // this one does not recognise. Measured, on the first launch of the build
    // that introduced the name: "Calculator 1 is open on localhost, which
    // checked in less than 15 minutes ago", about itself.
    if (!takeOver && in.present && !in.host.isEmpty() && !isThisHost(in.host)) {
        // Another machine has it. Until 2026sep02 this allowed the claim and
        // merely warned, because there was no way to ask that machine anything
        // and refusing would have locked the user out whenever it was simply
        // switched off. There IS a way to ask now, so a lock that has checked
        // in recently is refused: somebody is probably there to answer, and the
        // dialog offers to ask them. Nobody is ever stuck - it can be taken
        // over from that dialog either way.
        const QDateTime seen =
            QDateTime::fromString(in.seen.isEmpty() ? in.started : in.seen, Qt::ISODate);
        const bool fresh = seen.isValid()
            && seen.secsTo(QDateTime::currentDateTimeUtc()) < kQuietMinutes * 60;
        if (fresh) {
            setError(tr("%1 is open on %2, which checked in less than %3 minutes "
                        "ago.").arg(m_instance, in.host).arg(kQuietMinutes));
            return false;
        }
        // Quiet long enough that there is probably nobody to ask: allow, and
        // say so, exactly as before.
        setError(tr("This calculator was left open on %1 (%2). If it really is "
                    "still open there, whichever one quits last wins.")
                     .arg(in.host, in.started));
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return true;                    // read-only folder: populate() reports it
    const QString nowUtc = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    f.write(QStringLiteral("agape48-lock 2\npid=%1\nhost=%2\nstarted=%3\nseen=%4\n")
                .arg(QCoreApplication::applicationPid())
                .arg(here, nowUtc, nowUtc)
                .toUtf8());
    f.close();
    m_held = true;
    m_heldPath = path;
    m_heartbeat.start();
    // A request left in the folder was addressed to whoever held it before us.
    // Answering it now would put this calculator to sleep the moment it opened.
    QFile::remove(dir.filePath(QLatin1String(kSleepName)));
    // Holding it again settles both halves of the last conversation: we owe
    // nobody a handover, and nobody owes us one.
    m_answering.clear();
    m_askedFor.clear();
    m_expectAnswer = false;
    noteStateOnDisk();      // the baseline: everything after this is somebody else
    return true;
}

// --- asking for a calculator somebody else has -----------------------------
//
// Gert, 2026sep02: "give a warning - send a sleep command to the other one and
// take it over?" It is a better answer than taking it over, and not only a
// politer one: taking it over makes the loser drop the calculator WITHOUT
// saving, because by then the folder is not its to write. Asking lets it save
// first, so what the asker picks up is everything the other machine did.

QString StateFileManager::instancePath(const QString &instance) const
{
    if (!m_location.isLocalFile() || instance.isEmpty())
        return QString();
    return QDir(m_location.toLocalFile()).filePath(instance);
}

bool StateFileManager::requestSleep(const QString &instance)
{
    const QString dir = instancePath(instance);
    if (dir.isEmpty() || !QDir(dir).exists()) {
        setError(tr("There is no calculator called %1.").arg(instance));
        return false;
    }
    QFile f(QDir(dir).filePath(QLatin1String(kSleepName)));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setError(tr("Could not ask for %1: the folder is read-only.").arg(instance));
        return false;
    }
    f.write(QStringLiteral("agape48-sleep 1\npid=%1\nhost=%2\nat=%3\n")
                .arg(QCoreApplication::applicationPid())
                .arg(thisHost(),
                     QDateTime::currentDateTimeUtc().toString(Qt::ISODate))
                .toUtf8());
    f.close();

    // Is there anybody there to answer? It has to be decided HERE, while the
    // lock can still be read: once it is gone, "the lock vanished" cannot tell
    // a machine that saved and handed over from a crash that left a lock nobody
    // was ever going to clear. Same test isHeldBySomebody() makes - a lock from
    // another host counts, one from a dead process on this machine does not.
    //
    // When nobody is owed, handoverComplete() stays out of the way and the wait
    // ends the moment the lock does, exactly as it did before any of this.
    const LockInfo held = readLock(QDir(dir).filePath(QLatin1String(kLockName)));
    m_askedFor = instance;
    m_expectAnswer = held.present
                     && (!isThisHost(held.host) || processAlive(held.pid));
    return true;
}

void StateFileManager::withdrawSleepRequest(const QString &instance)
{
    const QString dir = instancePath(instance);
    if (dir.isEmpty())
        return;
    const QString path = QDir(dir).filePath(QLatin1String(kSleepName));
    const SleepReq r = readSleep(path);
    // Only our own. Two machines can be waiting on the same calculator, and
    // withdrawing somebody else's question would leave them waiting for ever.
    if (r.present && r.pid == QCoreApplication::applicationPid()
        && isThisHost(r.host))
        QFile::remove(path);
    // Answered, timed out or given up on - either way nobody owes us anything
    // now, and a stale expectation would gate the NEXT wait on this folder.
    if (instance == m_askedFor) {
        m_askedFor.clear();
        m_expectAnswer = false;
    }
}

// "Is somebody still holding it" for the machine that is waiting. A lock from
// another host cannot be checked for liveness at all, so it counts as held;
// one from a dead process on this machine does not, or the wait would never
// end after a crash.
bool StateFileManager::isHeldBySomebody(const QString &instance) const
{
    const QString dir = instancePath(instance);
    if (dir.isEmpty())
        return false;
    const LockInfo in = readLock(QDir(dir).filePath(QLatin1String(kLockName)));
    if (!in.present || lockIsOurs(in))
        return false;
    return !isThisHost(in.host) || processAlive(in.pid);
}

// The lock going is not the handover finishing. See the header for why, and
// requestSleep() for how we know whether an answer is owed at all.
//
// Deliberately true when nobody owes us anything: this gate exists to stop a
// half-delivered folder being read, not to become a new way of never opening a
// calculator. If no request of ours is outstanding, or the machine we asked was
// already dead, this says yes and the wait ends on the lock as it always did.
//
// When an answer IS owed, all three have to hold: the record is there, it names
// this instance as the one it was written for, and every file it claims matches
// the bytes on disk. The middle one is what a generation counter would be for -
// a contents file that arrives AFTER the memory, still describing the previous
// save, would otherwise look perfectly consistent and load the wrong calculator
// in silence. It cannot carry our tag, because it was written before we asked.
bool StateFileManager::handoverComplete(const QString &instance) const
{
    return handoverState(instance) == Complete;
}

StateFileManager::HandoverState
StateFileManager::handoverState(const QString &instance) const
{
    if (!m_expectAnswer || instance != m_askedFor)
        return Complete;
    const QString dir = instancePath(instance);
    if (dir.isEmpty())
        return Complete;
    const QDir d(dir);
    const Contents c = readContents(d.filePath(QLatin1String(kContentsName)));
    if (!c.present)
        return Nothing;                 // nothing has been written yet
    if (!contentsMatch(d, c))
        return Arriving;                // half a delivery: the defect itself
    // And it has to have been written FOR US. Nothing else will do, and two
    // weaker rules were tried and thrown away before this one.
    //
    // Both were attempts to spare the asker a ninety-second wait when the
    // holder quits rather than answering, by accepting a record that merely
    // looked new: first "different from the record we snapshotted when we
    // asked", then "the files have moved since we asked". Both accept a save
    // the holder made BEFORE our request, and that is reachable rather than
    // theoretical - a machine catching up on sync has an out-of-date record on
    // disk at the moment it asks, so the holder's EARLIER save arrives
    // afterwards, is perfectly self-consistent, and satisfies either rule. The
    // asker then picks up the calculator as it was some seconds before it
    // asked, and whatever was typed in between is quietly gone.
    //
    // Which is exactly the promise this mechanism exists to keep. Report
    // both-03 line 18: "Whatever you typed on Windows before asking is THERE.
    // That is the whole point of asking rather than taking."
    //
    // A wait that is too long is visible, has a countdown on it and a button
    // that ends it. A calculator quietly rolled back a few seconds is neither.
    // The tag is the only thing on disk that says "this is the save you asked
    // for", so the tag is the whole test - and a holder that quits instead of
    // answering costs the asker the full ninety seconds, deliberately.
    //
    // The digest test above comes FIRST on purpose. A folder whose files do not
    // match its own record is torn whoever wrote it, so Arriving outranks the
    // tag: NotOurs means "readable, just not the save we asked for", and
    // anything that is not readable must not be dressed up as merely untagged.
    return c.answers == instanceTag() ? Complete : NotOurs;
}

// Does that folder already hold somebody's calculator? Pointing at a folder
// that does is joining it, not moving in on top of it.
bool StateFileManager::shelfHasCalculators(const QUrl &shelf) const
{
    if (!shelf.isLocalFile())
        return false;
    const QDir dir(shelf.toLocalFile());
    if (!dir.exists())
        return false;
    const QStringList calcs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &calc : calcs)
        if (occupied(dir, calc))
            return true;
    return false;
}

// Rewrites only the seen= line's file, whole and small, once a minute. A
// process that is alive but cannot reach the network still writes this: being
// offline is not being dead, which is why nothing expires automatically.
void StateFileManager::beat()
{
    if (!m_held || m_heldPath.isEmpty())
        return;
    const LockInfo in = readLock(m_heldPath);
    if (!lockIsOurs(in)) {
        // Somebody decided we were gone and took it. Stop pretending.
        m_held = false;
        m_heartbeat.stop();
        emit lockLost();
        return;
    }
    QFile f(m_heldPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QStringLiteral("agape48-lock 2\npid=%1\nhost=%2\nstarted=%3\nseen=%4\n")
                .arg(QCoreApplication::applicationPid())
                .arg(in.host, in.started,
                     QDateTime::currentDateTimeUtc().toString(Qt::ISODate))
                .toUtf8());
}

// --- the files changing underneath us ---------------------------------------
//
// Gert's rule, 2026aug30: a calculator whose files change while it is open
// should go to sleep rather than race. The state on disk and the state in
// memory have become two different calculators, and every way of resolving
// that by hand is a guess - so stop, save nothing, and let ON decide, which
// reads whatever is actually there.
//
// This also makes a take-over immediate instead of up to a minute late: the
// lock file is inside the calculator's own folder, so it arrives with
// everything else and the loser hears about it as soon as the sync lands.
// beat() stays as the backstop for a folder no watcher can see.

void StateFileManager::watchFiles()
{
    if (!m_watch) {
        m_watch = new QFileSystemWatcher(this);
        auto bump = [this] { m_settle.start(); };
        connect(m_watch, &QFileSystemWatcher::fileChanged, this, bump);
        connect(m_watch, &QFileSystemWatcher::directoryChanged, this, bump);
    }
    const QStringList watched = m_watch->files() + m_watch->directories();
    if (!watched.isEmpty())
        m_watch->removePaths(watched);

    const QString dirPath = instanceDir();
    if (dirPath.isEmpty() || !m_location.isLocalFile() || !QDir(dirPath).exists())
        return;                         // SAF, or nothing opened yet

    // The directory as well as the files. A sync client does not edit a file in
    // place, it writes a new one and renames it over the top - and a watch on a
    // path that gets unlinked is dropped and never fires again. The directory
    // watch is what survives that, and every pass through here re-arms the
    // per-file ones.
    QStringList paths { dirPath };
    const QDir dir(dirPath);
    for (const char *name : kWatchedFiles) {
        const QString f = dir.filePath(QLatin1String(name));
        if (QFile::exists(f))
            paths << f;
    }
    const QString lock = dir.filePath(QLatin1String(kLockName));
    if (QFile::exists(lock))
        paths << lock;
    m_watch->addPaths(paths);
}

void StateFileManager::noteStateOnDisk()
{
    m_stamp.clear();
    const QString dirPath = instanceDir();
    if (!dirPath.isEmpty() && m_location.isLocalFile()) {
        const QDir dir(dirPath);
        for (const char *name : kWatchedFiles) {
            const QFileInfo fi(dir.filePath(QLatin1String(name)));
            m_stamp.insert(QLatin1String(name),
                           fi.exists() ? qMakePair(fi.size(),
                                                   fi.lastModified().toMSecsSinceEpoch())
                                       : qMakePair(qint64(-1), qint64(-1)));
        }
    }
    watchFiles();                       // a file may have just been created
}

// INEQUALITY, deliberately, and it must stay that way. A file a sync client
// lands carries the WRITER's mtime, which can be OLDER than our own last write
// to that path - two machines' clocks differ, and Dropbox preserves the mtime
// exactly (measured across the two laptops on 2026sep03). "Newer than ours"
// would therefore ignore an arriving calculator from a machine whose clock runs
// slow, silently, which is the one failure this whole mechanism exists to
// prevent. The question here is "is this still the file we wrote", never "is
// this more recent".
bool StateFileManager::filesChangedOnDisk() const
{
    if (m_stamp.isEmpty())
        return false;                   // no baseline yet: nothing to compare
    const QString dirPath = instanceDir();
    if (dirPath.isEmpty())
        return false;
    const QDir dir(dirPath);
    for (const char *name : kWatchedFiles) {
        const QFileInfo fi(dir.filePath(QLatin1String(name)));
        const auto now = fi.exists()
            ? qMakePair(fi.size(), fi.lastModified().toMSecsSinceEpoch())
            : qMakePair(qint64(-1), qint64(-1));
        if (m_stamp.value(QLatin1String(name), qMakePair(qint64(-1), qint64(-1))) != now)
            return true;
    }
    return false;
}

void StateFileManager::settle()
{
    watchFiles();                       // re-arm before deciding anything

    // The lock first: losing the calculator outranks the files changing, and it
    // has its own message. Only meaningful while we think we hold it.
    if (m_held && !m_heldPath.isEmpty() && !lockIsOurs(readLock(m_heldPath))) {
        m_held = false;
        m_heartbeat.stop();
        emit lockLost();
        return;
    }

    // Somebody has asked for this calculator. Only meaningful while we hold it,
    // and only somebody else's request - ours would be a question to ourselves.
    if (m_held) {
        const QString path = QDir(instanceDir()).filePath(QLatin1String(kSleepName));
        const SleepReq r = readSleep(path);
        if (r.present && !(r.pid == QCoreApplication::applicationPid()
                           && isThisHost(r.host))) {
            QFile::remove(path);        // letting go IS the answer
            if (!r.at.isValid()
                || r.at.secsTo(QDateTime::currentDateTimeUtc()) < kSleepStaleMinutes * 60) {
                // Whose question this is, so that what we write on the way out
                // is addressed to them and cannot be mistaken for an older
                // save that happens to be sitting in the folder.
                m_answering = r.host + QLatin1Char('/') + QString::number(r.pid);
                emit sleepRequested(r.host);
                return;
            }
            // Older than the wait anybody would sit through: whoever asked has
            // long since given up, and obeying now would look like a haunting.
        }
    }

    if (!filesChangedOnDisk())
        return;                         // our own write, or only the lock moved
    // Whatever is there now is the new truth, whether or not anyone acts on it.
    noteStateOnDisk();
    emit externalChangeDetected();
}

QVariantMap StateFileManager::lockHolder() const
{
    return lockHolderOf(m_instance);
}

// The same question about a calculator this window has NOT opened, which is
// what the shelf needs: you pick one from the list, it is somebody else's, and
// the dialog has to name them before it offers to do anything about it.
QVariantMap StateFileManager::lockHolderOf(const QString &instance) const
{
    QVariantMap out;
    const QString dir = instancePath(instance);
    if (dir.isEmpty())
        return out;
    const LockInfo in = readLock(QDir(dir).filePath(QLatin1String(kLockName)));
    if (!in.present)
        return out;

    const QDateTime seen = QDateTime::fromString(in.seen.isEmpty() ? in.started
                                                                   : in.seen,
                                                 Qt::ISODate);
    const qint64 quiet = seen.isValid()
        ? QDateTime::currentDateTimeUtc().secsTo(seen.toUTC()) / -60 : -1;

    out.insert(QStringLiteral("calculator"), instance);
    out.insert(QStringLiteral("host"), in.host);
    out.insert(QStringLiteral("sameMachine"), isThisHost(in.host));
    out.insert(QStringLiteral("alive"),
               isThisHost(in.host) && processAlive(in.pid));
    out.insert(QStringLiteral("since"),
               QDateTime::fromString(in.started, Qt::ISODate).toLocalTime());
    out.insert(QStringLiteral("seen"), seen.toLocalTime());
    out.insert(QStringLiteral("quietMinutes"), quiet);
    // Only a suggestion. A machine that is merely offline looks exactly like a
    // machine that has died, so this never acts on its own.
    out.insert(QStringLiteral("probablyGone"), quiet >= kQuietMinutes);
    return out;
}

void StateFileManager::release()
{
    if (!m_held)
        return;
    // Only if it is still ours. Another instance may have decided we were dead
    // and taken it over, and deleting its claim would undo the whole point.
    const LockInfo in = readLock(m_heldPath);
    if (in.present && in.pid == QCoreApplication::applicationPid()) {
        // Handing over on request: say what is in the folder BEFORE saying the
        // folder is free. Both files then travel, and whichever arrives first
        // the asker is safe - the record without the memory does not match, and
        // the memory without the record is not addressed to them.
        //
        // Only when a request is being answered. Quitting normally, and going
        // to sleep because somebody else's files landed, both come through here
        // too, and neither is anybody's cue to read the folder. Writing then
        // would be one more file into a synced folder for nothing - and in the
        // second case it would put our name on bytes we did not write.
        if (!m_answering.isEmpty())
            writeContents(QFileInfo(m_heldPath).dir(), m_answering);
        QFile::remove(m_heldPath);
    }
    m_answering.clear();
    m_held = false;
    m_heldPath.clear();
    m_heartbeat.stop();
}

QString StateFileManager::displayName() const
{
    if (m_location.isLocalFile())
        return QDir::toNativeSeparators(m_location.toLocalFile());
    // Nothing else can become the location any more - see localised() - so this
    // is only ever reached by a location saved by an older build.
    return m_location.toString();
}

bool StateFileManager::isWritable() const
{
    return m_location.isLocalFile()
           && QFileInfo(m_location.toLocalFile()).isWritable();
}

// --- population -------------------------------------------------------------

bool StateFileManager::populate(x48_config_t *cfg, QByteArray *storage)
{
    if (!cfg || !storage)
        return false;
#ifdef Q_OS_ANDROID
    // The permission is the user's to revoke, in Android's own settings, while
    // this app is not running. Say so plainly rather than starting a calculator
    // that cannot read its own memory.
    if (m_location.isLocalFile() && !canUseAnyFolder()
        && !withinOwnStorage(m_location.toLocalFile())) {
        setError(tr("Agape48 is not allowed into %1 any more. Turn \"Allow "
                    "access to manage all files\" back on for Agape48, or use "
                    "the settings page to put the calculators back in a folder "
                    "of its own.")
                     .arg(QDir::toNativeSeparators(m_location.toLocalFile())));
        return false;
    }
#endif
    return populateDesktop(cfg, storage);
}

bool StateFileManager::populateDesktop(x48_config_t *cfg, QByteArray *storage)
{
    const QString path = instanceDir();
    if (path.isEmpty()) {
        setError(tr("State location is not a local directory."));
        return false;
    }
    if (!QDir().mkpath(path)) {
        setError(tr("Cannot create %1.").arg(path));
        return false;
    }
    *storage = QFile::encodeName(path);
    cfg->state_dir = storage->constData();
    return true;
}

// --- conflicts --------------------------------------------------------------

bool StateFileManager::hasExternalChange() const
{
    // Cheap and good enough: the core's own fingerprint of what is on disk vs.
    // what we wrote last. A mismatch means someone else wrote it.
    return m_lastFingerprint != 0 && x48_state_fingerprint() != m_lastFingerprint;
}

bool StateFileManager::commit(quint64 fingerprint)
{
    if (m_lastFingerprint != 0 && fingerprint != 0) {
        const quint64 onDisk = x48_state_fingerprint();
        if (onDisk != fingerprint && onDisk != m_lastFingerprint) {
            emit conflictDetected(
                tr("The state file changed on another device since this session "
                   "started. Your local session was saved; the remote version is "
                   "the one your sync client keeps as a conflicted copy."));
        }
    }
    m_lastFingerprint = fingerprint;
    QSettings().setValue(QLatin1String(kFingerprintKey), fingerprint);
    // We just wrote them, so this is what the files are supposed to look like.
    // Without it the watcher reports our own save as an external change and the
    // calculator puts itself to sleep every time it is saved.
    noteStateOnDisk();
    // And the same thing said in a form that survives a sync client: sizes and
    // mtimes are a local baseline, digests are what another machine can check.
    // If this save is the one answering a request, it carries the answer, and
    // release() then has nothing left to write.
    if (!instanceDir().isEmpty() && m_location.isLocalFile()) {
        writeContents(QDir(instanceDir()), m_answering);
        m_answering.clear();
    }
    return true;
}

// --- picking ----------------------------------------------------------------

void StateFileManager::requestLocation()
{
    // One picker on every platform: QtQuick.Dialogs FolderDialog, which on
    // Android is the system's own ACTION_OPEN_DOCUMENT_TREE screen. The Java
    // half this used to call was never reachable - SafBridge.activity was
    // declared, never assigned, and every method began by returning on it being
    // null - so the button it belonged to did nothing at all on a phone.
    emit pickerRequested();
}

bool StateFileManager::migrateTo(const QUrl &destination)
{
    // Clear the previous complaint first. Dogfood #10 line 17: the "no such
    // folder" message stayed on screen after picking a folder that did exist,
    // because nothing ever put lastError back to empty.
    setError(QString());
    // What the folder picker gives back on Android is a content:// tree, and
    // what everything below this line needs is a path. See localised().
    const QUrl where = localised(destination);
    if (!where.isLocalFile()) {
        setError(tr("That folder is not on this device. Agape48 keeps the "
                    "calculator's memory in real files, so it needs a folder on "
                    "the phone or on its card - let your sync app put one "
                    "there, and point Agape48 at that."));
        return false;
    }
#ifdef Q_OS_ANDROID
    // BEFORE THE FOLDER IS EVEN LOOKED AT. Everything that could ask the
    // filesystem whether this will work gets the wrong answer - see
    // withinOwnStorage() - so the only sound order is to ask Android's question
    // first: is this the app's own storage, and if not, has the user said yes?
    if (!canUseAnyFolder() && !withinOwnStorage(where.toLocalFile())) {
        setError(tr("Android will not let Agape48 into %1 until you say so. "
                    "Use \"Let Agape48 into your own folders\" below - the folder "
                    "you picked is remembered and moved into as soon as you come "
                    "back. Or use \"Move it where other apps can see it\", which "
                    "needs no permission at all.")
                     .arg(QDir::toNativeSeparators(where.toLocalFile())));
        return false;
    }
#endif
    if (!m_location.isLocalFile()) {
        // A location left behind by a build that could store a content:// uri.
        setError(tr("The calculators are in a folder Agape48 can no longer "
                    "read. Use the house button to go back to the folder it "
                    "starts in, then move them from there."));
        return false;
    }
    const QDir from(m_location.toLocalFile());
    const QDir to(where.toLocalFile());

    // Same folder in, same folder out, and this has to come BEFORE the busy
    // check below: pressing Enter on the path you are already on moves nothing,
    // and the busy check now asks about every calculator on the shelf, so it
    // would answer "already open" about this very window.
    // Without the early return the loop below removes each
    // destination file and then copies it from itself, which deletes the lot -
    // pressing Enter twice on the same path wiped the calculator's memory.
    if (QFileInfo(from.absolutePath()).canonicalFilePath()
        == QFileInfo(to.absolutePath()).canonicalFilePath())
        return true;

    // The folder has to exist. It used to be created here, so a typo in the
    // path silently made a folder and moved into it - dogfood #9: "folders
    // should be created by an explicit click, not by a typo."
    if (!to.exists() || !canWriteInto(to)) {
        // ANDROID SAYS NO BEFORE IT SAYS ANYTHING ELSE. A folder of the user's
        // own - Documents, Download, a card - is not merely unwritable to an
        // app without "all files" access: it does not exist, because scoped
        // storage hides what it does not grant. Saying "there is no folder
        // called ..." about a folder he is looking at in his file manager is
        // the least helpful true sentence available, so ask the other question
        // first and let the caller offer the switch.
        if (!to.exists()) {
            setError(tr("There is no folder called %1. Use the \"…\" button to "
                        "pick one, or to make one.").arg(to.absolutePath()));
            return false;
        }
        setError(tr("%1 is read-only, so the calculator's memory cannot live "
                    "there.").arg(QDir::toNativeSeparators(to.absolutePath())));
        return false;
    }
    // Gert, 2026sep02: "if the folder already contains a calculator, it just
    // loads the calculator that's there. I think this would be much more
    // harmonious." He is right, and it is the difference between joining a
    // shared folder and moving in on top of whoever is already in it. Copying
    // in was only ever the right answer for a folder with nothing in it.
    //
    // Your own calculator is not touched and not moved. It stays in the folder
    // it was in, which is what makes this reversible: point back at that folder
    // and there it is.
    if (shelfHasCalculators(where)) {
        // The remembered name must NOT come with us. Every machine has a
        // "Calculator 1", so carrying the name over is how a window opens the
        // other machine's calculator believing it is its own.
        const QString leaving = from.absolutePath();
        // The remembered name must NOT come with us, and the new one has to be
        // chosen BEFORE the switch: setLocation() releases the old lock and
        // claims the new calculator on the spot, and it can only claim one it
        // knows the name of.
        QSettings().remove(QLatin1String(kInstanceKey));
        m_instance.clear();
        prepareInstances(where);
        setLocation(where, /*mustClaim=*/false);
        emit instanceChanged();
        setError(tr("That folder already has calculators in it, so %1 opened "
                    "instead of moving yours in. Yours is untouched, in %2.")
                     .arg(m_instance, QDir::toNativeSeparators(leaving)));
        return true;
    }

    // Nothing below this line runs for a folder that already has calculators in
    // it, so this guard is now about the copy alone - which is the only thing
    // that can hurt anybody. A calculator being open is not a reason to refuse
    // to JOIN a shelf; it is the reason the sleep dialog exists.
    if (isBusy(where)) {
        setError(tr("A calculator in that folder is open in another Agape48. "
                    "Close it there, or choose a different state folder."));
        return false;
    }

    // The shelf moves, not one calculator: the shared ROM at the top, and every
    // calculator subfolder with it. kMigrateFiles is still the per-calculator
    // list and is used inside the loop.
    const QString romSrc = from.filePath(QStringLiteral("rom"));
    if (QFile::exists(romSrc)) {
        const QString dst = to.filePath(QStringLiteral("rom"));
        QFile::remove(dst);
        if (!QFile::copy(romSrc, dst)) {
            setError(tr("Could not copy the ROM."));
            return false;
        }
    }
    const QStringList calcs = from.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList landedAs;
    for (const QString &calc : calcs) {
        // A calculator of that name already on the shelf is SOMEBODY ELSE'S.
        // Every machine calls its first calculator "Calculator 1", so two
        // machines pointing at one shared folder is not a corner case, it is
        // the normal one - and the loop below removes each destination file
        // before copying, so the second machine to arrive would quietly put its
        // memory where the first machine's was. Land beside it instead.
        QString name = calc;
        if (occupied(to, calc)) {
            name = freeNameIn(to);
            if (name.isEmpty()) {
                setError(tr("The folder already has a calculator called %1, and "
                            "there is no free name to put yours under.").arg(calc));
                return false;
            }
            landedAs << tr("%1 arrived as %2").arg(calc, name);
            if (m_instance == calc) {
                // The open one has to follow its own files, or the next start
                // opens the OTHER machine's calculator of that name.
                m_instance = name;
                QSettings().setValue(QLatin1String(kInstanceKey), m_instance);
            }
        }
        if (!to.exists(name) && !to.mkdir(name)) {
            setError(tr("Could not make %1 in the new folder.").arg(name));
            return false;
        }
        for (const char *leaf : kMigrateFiles) {
            if (qstrcmp(leaf, "rom") == 0)
                continue;                       // shared, handled above
            const QString src = from.filePath(calc + QLatin1Char('/')
                                              + QLatin1String(leaf));
            if (!QFile::exists(src))
                continue;
            const QString dst = to.filePath(name + QLatin1Char('/')
                                            + QLatin1String(leaf));
            QFile::remove(dst);
            if (!QFile::copy(src, dst)) {
                setError(tr("Could not copy %1 of %2.").arg(QLatin1String(leaf), calc));
                return false;
            }
        }
    }
    setLocation(where);
    prepareInstances();
    // Said after the move rather than asked before it: nothing was lost either
    // way - the originals are still in the old folder - but the user has to be
    // told which calculator is now which.
    if (!landedAs.isEmpty())
        setError(tr("That folder already had calculators with these names, so "
                    "yours were put beside them: %1.")
                     .arg(landedAs.join(QStringLiteral(", "))));
    return true;
}

void StateFileManager::setError(const QString &what)
{
    if (m_lastError == what)
        return;
    m_lastError = what;
    emit lastErrorChanged();
}
