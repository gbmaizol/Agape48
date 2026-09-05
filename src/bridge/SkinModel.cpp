#include "SkinModel.h"

#include "x48_shim.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

QRect rectFromJson(const QJsonValue &v)
{
    // Accepts [x, y, w, h] or { "x":, "y":, "w":, "h": }.
    if (v.isArray()) {
        const QJsonArray a = v.toArray();
        if (a.size() == 4)
            return QRect(a.at(0).toInt(), a.at(1).toInt(),
                         a.at(2).toInt(), a.at(3).toInt());
        return {};
    }
    const QJsonObject o = v.toObject();
    return QRect(o.value(QStringLiteral("x")).toInt(),
                 o.value(QStringLiteral("y")).toInt(),
                 o.value(QStringLiteral("w")).toInt(),
                 o.value(QStringLiteral("h")).toInt());
}

QSize sizeFromJson(const QJsonValue &v)
{
    if (v.isArray()) {
        const QJsonArray a = v.toArray();
        if (a.size() == 2)
            return QSize(a.at(0).toInt(), a.at(1).toInt());
    }
    return {};
}

} // namespace

SkinModel::SkinModel(QObject *parent) : QObject(parent) {}

QVariantMap SkinModel::keyToMap(const Key &k)
{
    return QVariantMap {
        { QStringLiteral("id"),      k.id },
        { QStringLiteral("key"),     k.key },
        { QStringLiteral("row"),     k.row },
        { QStringLiteral("mask"),    k.mask },
        { QStringLiteral("rect"),    k.rect },
        { QStringLiteral("cap"),     k.cap.isEmpty() ? k.rect : k.cap },
        { QStringLiteral("pressed"), k.pressed },
        { QStringLiteral("label"),   k.label },
    };
}

bool SkinModel::loadDefault()
{
    return load(QUrl(QStringLiteral(
        "qrc:/qt/qml/Agape48/assets/skins/default/layout.json")));
}

bool SkinModel::load(const QUrl &url)
{
    // QFile handles qrc:, local paths and - on Android - content: urls, so
    // there is no per-platform branch here.
    const QString path = url.isLocalFile()
        ? url.toLocalFile()
        : (url.scheme() == QLatin1String("qrc")
               ? QLatin1Char(':') + url.path()
               : url.toString());

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        setError(tr("Cannot open skin %1: %2").arg(url.toString(), f.errorString()));
        return false;
    }
    const QByteArray data = f.readAll();
    f.close();

    clear();
    m_source = url;

    const bool ok = loadJson(data, url);

    if (ok)
        emit changed();
    return ok;
}

bool SkinModel::loadJson(const QByteArray &data, const QUrl &base)
{
    QJsonParseError err {};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (doc.isNull()) {
        setError(tr("Skin JSON is invalid at offset %1: %2")
                     .arg(err.offset).arg(err.errorString()));
        return false;
    }
    const QJsonObject root = doc.object();

    const QString schema = root.value(QStringLiteral("schema")).toString();
    if (!schema.startsWith(QStringLiteral("agape48.skin/"))) {
        setError(tr("Not an Agape48 skin (schema = \"%1\").").arg(schema));
        return false;
    }

    m_title  = root.value(QStringLiteral("title")).toString();
    m_author = root.value(QStringLiteral("author")).toString();
    m_model  = root.value(QStringLiteral("model")).toString();

    const QJsonObject face = root.value(QStringLiteral("face")).toObject();
    m_faceImage = base.resolved(
        QUrl(face.value(QStringLiteral("image")).toString()));
    m_faceSize = sizeFromJson(face.value(QStringLiteral("size")));

    const QJsonObject lcd = root.value(QStringLiteral("lcd")).toObject();
    m_lcdRect = rectFromJson(lcd.value(QStringLiteral("rect")));
    m_lcdZoom = lcd.value(QStringLiteral("zoom")).toInt(2);
    if (lcd.contains(QStringLiteral("pixelColor")))
        m_lcdPixelColor = QColor(lcd.value(QStringLiteral("pixelColor")).toString());
    if (lcd.contains(QStringLiteral("background")))
        m_lcdBackground = QColor(lcd.value(QStringLiteral("background")).toString());

    const QJsonArray keys = root.value(QStringLiteral("keys")).toArray();
    m_keys.reserve(keys.size());
    for (const QJsonValue &kv : keys) {
        const QJsonObject o = kv.toObject();
        Key k;
        k.id    = o.value(QStringLiteral("id")).toString();
        k.key   = o.value(QStringLiteral("key")).toString();
        k.label = o.value(QStringLiteral("label")).toString();
        k.rect  = rectFromJson(o.value(QStringLiteral("rect")));
        if (o.contains(QStringLiteral("cap")))
            k.cap = rectFromJson(o.value(QStringLiteral("cap")));
        if (o.contains(QStringLiteral("pressed")))
            k.pressed = rectFromJson(o.value(QStringLiteral("pressed")));
        // A skin may address the matrix directly instead of by name.
        if (o.contains(QStringLiteral("code"))) {
            const QJsonArray c = o.value(QStringLiteral("code")).toArray();
            if (c.size() == 2) {
                k.row  = c.at(0).toInt(-1);
                k.mask = c.at(1).toInt(0);
            }
        }
        if (k.rect.isEmpty()) {
            setError(tr("Key \"%1\" has an empty hit area.").arg(k.id.isEmpty() ? k.key : k.id));
            return false;
        }
        if (k.key.isEmpty() && k.row < 0) {
            setError(tr("Key \"%1\" has neither a name nor a matrix code.").arg(k.id));
            return false;
        }
        m_keys.append(keyToMap(k));
    }

    const QJsonArray anns = root.value(QStringLiteral("annunciators")).toArray();
    for (const QJsonValue &av : anns) {
        const QJsonObject o = av.toObject();
        const QString img = o.value(QStringLiteral("image")).toString();
        m_annunciators.append(QVariantMap {
            { QStringLiteral("id"),   o.value(QStringLiteral("id")).toString() },
            { QStringLiteral("bit"),  o.value(QStringLiteral("bit")).toInt() },
            { QStringLiteral("rect"), rectFromJson(o.value(QStringLiteral("rect"))) },
            // Drawn by tools/makeface.py, one small PNG each. A glyph in a font
            // would not survive the trip to Android or Windows.
            { QStringLiteral("image"), img.isEmpty() ? QUrl() : base.resolved(QUrl(img)) },
        });
    }
    // Optional: a skin with nothing to say here simply omits it, and the
    // nameplate is not drawn. Passed through as it is written rather than
    // unpacked into properties of its own - it is one block that belongs to one
    // Text item, and every field of it is the skin's business, not ours.
    const QJsonObject plate = root.value(QStringLiteral("nameplate")).toObject();
    if (!plate.isEmpty()) {
        m_nameplate = plate.toVariantMap();
        m_nameplate.insert(QStringLiteral("rect"),
                           rectFromJson(plate.value(QStringLiteral("rect"))));
    }
    return true;
}

void SkinModel::clear()
{
    m_title.clear(); m_author.clear(); m_model.clear();
    m_faceImage = QUrl();
    m_faceSize = QSize();
    m_lcdRect = QRect();
    m_lcdZoom = 2;
    m_keys.clear();
    m_annunciators.clear();
    m_nameplate.clear();
}

void SkinModel::setError(const QString &what)
{
    if (m_lastError == what)
        return;
    m_lastError = what;
    emit lastErrorChanged();
}
