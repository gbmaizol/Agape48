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

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUrl>

struct x48_config_t;

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

public:
    explicit StateFileManager(QObject *parent = nullptr);

    QUrl location() const { return m_location; }
    QString displayName() const;
    bool isWritable() const;
    bool isDefault() const;
    QString lastError() const { return m_lastError; }

    void setLocation(const QUrl &url);

    // --- used by Agape48Engine, not QML ------------------------------------
    // Fills either cfg->state_dir (desktop; the QByteArray keeps the bytes
    // alive for the caller) or cfg->fd_* (Android SAF). Returns false and sets
    // lastError() if the location is unusable.
    bool populate(x48_config_t *cfg, QByteArray *stateDirStorage);

    // Called after a successful core save: records the new fingerprint so the
    // next external change is detectable.
    bool commit(quint64 fingerprint);

    bool hasExternalChange() const;

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
    void lastErrorChanged();
    void pickerRequested();
    void externalChangeDetected();
    void conflictDetected(const QString &detail);

private:
    bool populateDesktop(x48_config_t *cfg, QByteArray *storage);
    bool populateAndroidSaf(x48_config_t *cfg);
    void loadPersistedLocation();
    void persistLocation();
    void setError(const QString &what);

    QUrl    m_location;
    quint64 m_lastFingerprint = 0;
    QString m_lastError;
};
