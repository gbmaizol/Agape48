// ---------------------------------------------------------------------------
// SkinModel - ŝargas kalkulilan haŭton: unu rastruman vizaĝon plus aranĝon kiu
// mapigas ekranajn rektangulojn al klavoj de HP 48.
//
// Unu aranĝformato: *.json, skemo "agape48.skin/1".
//
// La KML de Emu48 estis forlasita je 2026aug28 (design-questions, ero 5). Kun
// la ŝelo de Droid48 sur ĉiuj tri platformoj ne ekzistas Emu48-forma labortablo
// en kiun ŝargi Emu48-haŭtojn, kaj iliaj vizaĝoj estas BMP-oj de 200-400
// bilderoj desegnitaj por ekrano de la 1990-aj jaroj.
//
// La klavoj estas prezentataj kiel QVariantList de mapoj prefere ol kiel
// QAbstractListModel: la listo estas ŝargita unufoje kaj neniam mutacias, do la
// maŝinaro de modelo por pliigaj ĝisdatigoj estus pura ŝarĝo. QML konsumas ĝin
// per simpla Repeater.
// ---------------------------------------------------------------------------
#pragma once

#include <QColor>
#include <QObject>
#include <QQmlEngine>
#include <QRect>
#include <QSize>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

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
    // Kie la nomo de la malfermita kalkulilo estas skribita, inter la du vortoj
    // kiujn ĉi tiu haŭto presis sur sin mem. Malplena por haŭto kiu ne volas
    // tian, kaj Calculator.qml tiam desegnas nenion.
    Q_PROPERTY(QVariantMap  nameplate     READ nameplate     NOTIFY changed)
    Q_PROPERTY(QVariantMap  badge         READ badge         NOTIFY changed)
    Q_PROPERTY(QString lastError       READ lastError       NOTIFY lastErrorChanged)

public:
    // La trafzono de unu klavo. Tenata kiel strukturo ĉi tie kaj platigita al
    // QVariantMap por QML, kun ĉi tiuj ekzaktaj ŝlosilnomoj:
    //   id      string   haŭt-loka identigilo, por diagnozo
    //   key     string   Agape48-klavnomo ("ENTER"), aŭ malplena se nur-koda
    //   row     int      elira vico de la matrico de x48, -1 kiam la haŭto uzis nomon
    //   mask    int      enira masko de la matrico de x48
    //   rect    rect     trafzono en bilderoj de la vizaĝbildo
    //   cap rect         laŭvola; la desegnita klavo interne de la trafzono, kio
    //                    estas kion la premata emfazo kovras. tools/makeface.py
    //                    eligas ĝin, kaj rect tiam estas la ĉapo plus malgranda kadro.
    //   pressed rect     laŭvola fonta rektangulo de la premat-klava desegnaĵo
    //   label   string   alirebleco / helpo por la fizika klavaro
    struct Key {
        QString id;
        QString key;
        int     row  = -1;
        int     mask = 0;
        QRect   rect;
        QRect   cap;
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
    QVariantMap nameplate() const { return m_nameplate; }
    QVariantMap badge() const { return m_badge; }
    QVariantList keys() const { return m_keys; }
    QVariantList annunciators() const { return m_annunciators; }
    QString lastError() const { return m_lastError; }

public slots:
    // Akceptas qrc:-, file:- kaj (sur Androido) content:-adresojn. La dialekto
    // estas elektita laŭ la sufikso, retrofalante al flarado de la unua
    // nemalplena bajto: '{' -> JSON.
    bool load(const QUrl &url);
    bool loadDefault();

signals:
    void changed();
    void lastErrorChanged();

private:
    bool loadJson(const QByteArray &data, const QUrl &base);
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
    QVariantMap  m_nameplate;
    QVariantMap  m_badge;
    QString m_lastError;
};
