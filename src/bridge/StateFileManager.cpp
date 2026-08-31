#include "StateFileManager.h"

#include "x48_shim.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
#  include <QJniObject>
#  include <QtCore/qnativeinterface.h>
#endif

namespace {
constexpr auto kSettingsKey = "state/location";
constexpr auto kFingerprintKey = "state/fingerprint";

// Two lists, because they had two jobs and one of them was silently wrong.
//
// kSafFiles is POSITIONAL: Android opens one fd per name, in the order of
// { fd_ram, fd_port1, fd_port2, fd_state }, so nothing may be inserted here.
// The fourth was called "state"; the C core names that file "hp48"
// (x48_shim.c sets conf_filename), so the Android tree was writing a file the
// desktop would never read back.
const char *const kSafFiles[] = { "ram", "port1", "port2", "hp48" };

// kMigrateFiles is everything that has to travel when the state folder moves.
// It used to be the list above, which meant migrating copied "ram", looked in
// vain for a "state", and left the ROM behind. The folder it produced could
// not boot: dogfood #8, where a moved state folder gave "No HP 48 ROM
// selected" on the next start, with the message hidden behind the settings
// window that the same failure had opened.
const char *const kMigrateFiles[] = { "rom", "ram", "hp48", "port1", "port2" };

#ifdef Q_OS_ANDROID
constexpr auto kSafClass = "dk/geeak/agape48/SafBridge";
#endif
} // namespace

StateFileManager::StateFileManager(QObject *parent)
    : QObject(parent)
{
    loadPersistedLocation();
}

void StateFileManager::loadPersistedLocation()
{
    QSettings s;
    const QString stored = s.value(QLatin1String(kSettingsKey)).toString();
    m_lastFingerprint = s.value(QLatin1String(kFingerprintKey)).toULongLong();
    if (!stored.isEmpty()) {
        m_location = QUrl(stored);
        return;
    }
    useDefaultLocation();
}

void StateFileManager::persistLocation()
{
    QSettings s;
    s.setValue(QLatin1String(kSettingsKey), m_location.toString());
}

void StateFileManager::useDefaultLocation()
{
    setError(QString());
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    setLocation(QUrl::fromLocalFile(dir));
}

bool StateFileManager::isDefault() const
{
    const QString def =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return m_location.isLocalFile() && m_location.toLocalFile() == def;
}

void StateFileManager::setLocation(const QUrl &url)
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
    if (wasHeld && !claim()) {
        m_location = previous;
        claim();
        return;                     // lastError() already says why
    }
    persistLocation();
    emit locationChanged();
}

// --- one calculator, one instance -------------------------------------------

namespace {

constexpr auto kLockName = "in-use";

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
    QString started;
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
    }
    return in;
}

} // namespace

bool StateFileManager::isBusy(const QUrl &url) const
{
    if (!url.isLocalFile())
        return false;
    const QDir dir(url.toLocalFile());
    if (!dir.exists())
        return false;
    const LockInfo in = readLock(dir.filePath(QLatin1String(kLockName)));
    return in.present
        && in.host == QSysInfo::machineHostName()
        && in.pid != QCoreApplication::applicationPid()
        && processAlive(in.pid);
}

bool StateFileManager::claim()
{
    m_held = false;
    m_heldPath.clear();
    if (!m_location.isLocalFile())
        return true;                    // SAF: see the note in the header
    const QDir dir(m_location.toLocalFile());
    if (!dir.exists())
        return true;                    // populate() says this better than we can

    const QString path = dir.filePath(QLatin1String(kLockName));
    const QString here = QSysInfo::machineHostName();
    const LockInfo in = readLock(path);

    if (isBusy(m_location)) {
        setError(tr("That calculator is already open in another Agape48 "
                    "window. Close it, or choose a different state folder."));
        return false;
    }
    // A lock from a dead process - a crash, or a machine that went down with
    // it open - means nothing is reading the folder. Take it over, say nothing.

    if (in.present && !in.host.isEmpty() && in.host != here) {
        // No way to ask another machine whether its copy is still running, and
        // refusing would lock the user out of their own calculator whenever
        // that machine is simply switched off. So: allow, and say so.
        setError(tr("This calculator was left open on %1 (%2). If it really is "
                    "still open there, whichever one quits last wins.")
                     .arg(in.host, in.started));
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return true;                    // read-only folder: populate() reports it
    f.write(QStringLiteral("agape48-lock 1\npid=%1\nhost=%2\nstarted=%3\n")
                .arg(QCoreApplication::applicationPid())
                .arg(here,
                     QDateTime::currentDateTime().toString(Qt::ISODate))
                .toUtf8());
    f.close();
    m_held = true;
    m_heldPath = path;
    return true;
}

void StateFileManager::release()
{
    if (!m_held)
        return;
    // Only if it is still ours. Another instance may have decided we were dead
    // and taken it over, and deleting its claim would undo the whole point.
    const LockInfo in = readLock(m_heldPath);
    if (in.present && in.pid == QCoreApplication::applicationPid())
        QFile::remove(m_heldPath);
    m_held = false;
    m_heldPath.clear();
}

QString StateFileManager::displayName() const
{
    if (m_location.isLocalFile())
        return QDir::toNativeSeparators(m_location.toLocalFile());
#ifdef Q_OS_ANDROID
    // content:// uris are unreadable to humans; ask SAF for the tree's name.
    const QJniObject name = QJniObject::callStaticObjectMethod(
        kSafClass, "displayName",
        "(Ljava/lang/String;)Ljava/lang/String;",
        QJniObject::fromString(m_location.toString()).object<jstring>());
    if (name.isValid())
        return name.toString();
#endif
    return m_location.toString();
}

bool StateFileManager::isWritable() const
{
    if (m_location.isLocalFile())
        return QFileInfo(m_location.toLocalFile()).isWritable();
#ifdef Q_OS_ANDROID
    return QJniObject::callStaticMethod<jboolean>(
        kSafClass, "isWritable", "(Ljava/lang/String;)Z",
        QJniObject::fromString(m_location.toString()).object<jstring>());
#endif
    return false;
}

// --- population -------------------------------------------------------------

bool StateFileManager::populate(x48_config_t *cfg, QByteArray *storage)
{
    if (!cfg || !storage)
        return false;
#ifdef Q_OS_ANDROID
    if (!m_location.isLocalFile())
        return populateAndroidSaf(cfg);
#endif
    return populateDesktop(cfg, storage);
}

bool StateFileManager::populateDesktop(x48_config_t *cfg, QByteArray *storage)
{
    const QString path = m_location.toLocalFile();
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

bool StateFileManager::populateAndroidSaf(x48_config_t *cfg)
{
#ifdef Q_OS_ANDROID
    // SafBridge.openFd() does buildDocumentUriUsingTree + createDocument if
    // missing + openFileDescriptor("rw") + ParcelFileDescriptor.detachFd(),
    // so the fd outlives the Java object and the C core can keep it.
    int *slots[] = { &cfg->fd_ram, &cfg->fd_port1, &cfg->fd_port2, &cfg->fd_state };
    for (int i = 0; i < 4; ++i) {
        const jint fd = QJniObject::callStaticMethod<jint>(
            kSafClass, "openFd",
            "(Ljava/lang/String;Ljava/lang/String;)I",
            QJniObject::fromString(m_location.toString()).object<jstring>(),
            QJniObject::fromString(QLatin1String(kSafFiles[i])).object<jstring>());
        if (fd < 0) {
            setError(tr("Storage Access Framework refused \"%1\". "
                        "Re-pick the folder to renew the permission grant.")
                         .arg(QLatin1String(kSafFiles[i])));
            return false;
        }
        *slots[i] = int(fd);
    }
    return true;
#else
    Q_UNUSED(cfg)
    return false;
#endif
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
    return true;
}

// --- picking ----------------------------------------------------------------

void StateFileManager::requestLocation()
{
#ifdef Q_OS_ANDROID
    // Java side runs ACTION_OPEN_DOCUMENT_TREE and, on result, calls
    // takePersistableUriPermission() and stores the uri. It then notifies back
    // through SafBridge -> a registered native callback; wire that in
    // main.cpp with QJniEnvironment::registerNativeMethods().
    QJniObject::callStaticMethod<void>(kSafClass, "pickTree", "()V");
#else
    emit pickerRequested();   // QML shows QtQuick.Dialogs FolderDialog
#endif
}

bool StateFileManager::migrateTo(const QUrl &destination)
{
    // Clear the previous complaint first. Dogfood #10 line 17: the "no such
    // folder" message stayed on screen after picking a folder that did exist,
    // because nothing ever put lastError back to empty.
    setError(QString());
    if (!m_location.isLocalFile() || !destination.isLocalFile()) {
        // TODO: the local -> content:// direction needs SAF writes; do it by
        // reading each file here and pushing the bytes through SafBridge.
        setError(tr("Migration between local and SAF storage is not implemented yet."));
        return false;
    }
    const QDir from(m_location.toLocalFile());
    const QDir to(destination.toLocalFile());

    // Before a single byte moves. setLocation() at the end of this function
    // would refuse a busy folder, but by then the copy has already been over
    // the top of the other instance's memory, which is the exact thing the
    // lock exists to prevent.
    if (isBusy(destination)) {
        setError(tr("That calculator is already open in another Agape48 "
                    "window. Close it, or choose a different state folder."));
        return false;
    }

    // Same folder in, same folder out. Without this the loop below removes each
    // destination file and then copies it from itself, which deletes the lot -
    // pressing Enter twice on the same path wiped the calculator's memory.
    if (QFileInfo(from.absolutePath()).canonicalFilePath()
        == QFileInfo(to.absolutePath()).canonicalFilePath())
        return true;

    // The folder has to exist. It used to be created here, so a typo in the
    // path silently made a folder and moved into it - dogfood #9: "folders
    // should be created by an explicit click, not by a typo."
    if (!to.exists()) {
        setError(tr("There is no folder called %1. Use the \"…\" button to "
                    "pick one, or to make one.").arg(to.absolutePath()));
        return false;
    }
    for (const char *name : kMigrateFiles) {
        const QString src = from.filePath(QLatin1String(name));
        if (!QFile::exists(src))
            continue;
        const QString dst = to.filePath(QLatin1String(name));
        QFile::remove(dst);
        if (!QFile::copy(src, dst)) {
            setError(tr("Could not copy %1.").arg(name));
            return false;
        }
    }
    setLocation(destination);
    return true;
}

void StateFileManager::setError(const QString &what)
{
    if (m_lastError == what)
        return;
    m_lastError = what;
    emit lastErrorChanged();
}
