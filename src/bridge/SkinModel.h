// ---------------------------------------------------------------------------
// SkinModel - loads a calculator skin: one raster face plus a layout that maps
// screen rectangles to HP 48 keys.
//
// Two layout dialects, one in-memory model:
//   *.json  - Agape48's native format (schema "agape48.skin/1").
//   *.kml   - Emu48 Keypad Mapping Language, so the twenty-odd years of
//             existing Emu48 skins are usable. See KmlParser.
//
// Keys are exposed as a QVariantList of maps rather than a QAbstractListModel:
// the list is loaded once and never mutates, so a model's incremental-update
// machinery would be pure overhead. QML consumes it with a plain Repeater.
// ---------------------------------------------------------------------------
#pragma once

#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QRect>
#include <QSize>
#include <QUrl>
#include <QVariantList>

class SkinModel : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reached through Agape48Engine.skin")

    Q_PROPERTY(QUrl    source          READ source          NOTIFY changed)
    Q_PROPERTY(QString title           READ title           NOTIFY changed)
    Q_PROPERTY(QString author          READ author          NOTIFY changed)
    Q_PROPERTY(QString model           READ model           NOTIFY changed)
    Q_PROPERTY(QUrl    faceImage       READ faceImage       NOTIFY changed)
    Q_PROPERTY(QSize   faceSize        READ faceSize        NOTIFY changed)
    Q_PROPERTY(QRect   lcdRect         READ lcdRect         NOTIFY changed)
    Q_PROPERTY(int     lcdZoom         READ lcdZoom         NOTIFY changed)
    Q_PROPERTY(QColor  lcdPixelColor   READ lcdPixelColor   NOTIFY changed)
    Q_PROPERTY(QColor  lcdBackground   READ lcdBackground   NOTIFY changed)
    Q_PROPERTY(QVariantList keys          READ keys          NOTIFY changed)
    Q_PROPERTY(QVariantList annunciators  READ annunciators  NOTIFY changed)
    Q_PROPERTY(QString lastError       READ lastError       NOTIFY lastErrorChanged)

public:
    // One key's hit area. Kept as a struct here and flattened to a QVariantMap
    // for QML, with these exact key names:
    //   id      string   skin-local identifier, for diagnostics
    //   key     string   Agape48 key name ("ENTER"), or empty if code-only
    //   row     int      x48 matrix out row, -1 when the skin used a name
    //   mask    int      x48 matrix in mask
    //   rect    rect     hit area in face-image pixels
    //   pressed rect     optional source rect of the pressed-key artwork
    //   label   string   accessibility / physical-keyboard hint
    struct Key {
        QString id;
        QString key;
        int     row  = -1;
        int     mask = 0;
        QRect   rect;
        QRect   pressed;
        QString label;
    };

    explicit SkinModel(QObject *parent = nullptr);

    QUrl source() const { return m_source; }
    QString title() const { return m_title; }
    QString author() const { return m_author; }
    QString model() const { return m_model; }
    QUrl faceImage() const { return m_faceImage; }
    QSize faceSize() const { return m_faceSize; }
    QRect lcdRect() const { return m_lcdRect; }
    int lcdZoom() const { return m_lcdZoom; }
    QColor lcdPixelColor() const { return m_lcdPixelColor; }
    QColor lcdBackground() const { return m_lcdBackground; }
    QVariantList keys() const { return m_keys; }
    QVariantList annunciators() const { return m_annunciators; }
    QString lastError() const { return m_lastError; }

public slots:
    // Accepts qrc:, file: and (on Android) content: urls. Dialect is chosen by
    // suffix, falling back to sniffing the first non-blank byte: '{' -> JSON.
    bool load(const QUrl &url);
    bool loadDefault();

signals:
    void changed();
    void lastErrorChanged();

private:
    bool loadJson(const QByteArray &data, const QUrl &base);
    bool loadKml(const QByteArray &data, const QUrl &base);
    void clear();
    void setError(const QString &what);
    static QVariantMap keyToMap(const Key &k);

    QUrl    m_source;
    QString m_title, m_author, m_model;
    QUrl    m_faceImage;
    QSize   m_faceSize;
    QRect   m_lcdRect;
    int     m_lcdZoom = 2;
    QColor  m_lcdPixelColor { 0x00, 0x00, 0x00 };
    QColor  m_lcdBackground { 0x9f, 0xbf, 0x7a };
    QVariantList m_keys;
    QVariantList m_annunciators;
    QString m_lastError;
};
