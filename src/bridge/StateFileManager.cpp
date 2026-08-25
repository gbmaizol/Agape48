#include "StateFileManager.h"

#include "x48_shim.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#ifdef Q_OS_ANDROID
#  include <QCoreApplication>
#  include <QJniObject>
#  include <QtCore/qnativeinterface.h>
#endif

namespace {
constexpr auto kSettingsKey = "state/location";
constexpr auto kFingerprintKey = "state/fingerprint";

// The four images x48 keeps. Names match upstream so an existing ~/.x48ng
// directory can be pointed at directly.
const char *const kStateFiles[] = { "ram", "port1", "port2", "state" };

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
    m_location = url;
    persistLocation();
    emit locationChanged();
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
            QJniObject::fromString(QLatin1String(kStateFiles[i])).object<jstring>());
        if (fd < 0) {
            setError(tr("Storage Access Framework refused \"%1\". "
                        "Re-pick the folder to renew the permission grant.")
                         .arg(QLatin1String(kStateFiles[i])));
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
    if (!m_location.isLocalFile() || !destination.isLocalFile()) {
        // TODO: the local -> content:// direction needs SAF writes; do it by
        // reading each file here and pushing the bytes through SafBridge.
        setError(tr("Migration between local and SAF storage is not implemented yet."));
        return false;
    }
    const QDir from(m_location.toLocalFile());
    const QDir to(destination.toLocalFile());
    if (!QDir().mkpath(to.absolutePath())) {
        setError(tr("Cannot create %1.").arg(to.absolutePath()));
        return false;
    }
    for (const char *name : kStateFiles) {
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
