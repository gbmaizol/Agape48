// ---------------------------------------------------------------------------
// LcdItem - desegnas la bilderbufron de x48 kiel teksturnodon de la scengrafo.
//
// Propra QQuickItem prefere ol QML-a Canvas: Canvas iras tien kaj reen tra
// JS-a dukampa kunteksto kaj FBO ĉiun kadron, kio sur telefono estas la
// diferenco inter "senkosta" kaj "videbla bateria kosto". Ĉi tiu vojo estas
// unu teksturalŝuto de 8,4 KB, nur je kadroj kiujn la HP 48 vere ŝanĝis, kun
// filtrado laŭ la plej proksima najbaro por ke la bilderoj restu kvadrataj je
// ĉia zomo.
// ---------------------------------------------------------------------------
#pragma once

#include <QColor>
#include <QImage>
#include <QQmlEngine>
#include <QQuickItem>

class Agape48Engine;

class LcdItem : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(Agape48Engine *engine READ engine WRITE setEngine NOTIFY engineChanged)
    Q_PROPERTY(QColor pixelColor      READ pixelColor      WRITE setPixelColor      NOTIFY pixelColorChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    // La HP 48 falsas grizon per baskuligo de bilderoj inter refreŝigoj.
    // Averaĝi du sinsekvajn kadrojn reproduktas tion; malŝaltita defaŭlte ĉar
    // ĝi duobligas la alŝutrapidon por funkcio kiun plej multaj haŭtoj neniam
    // uzas.
    Q_PROPERTY(bool grayscale READ grayscale WRITE setGrayscale NOTIFY grayscaleChanged)

public:
    explicit LcdItem(QQuickItem *parent = nullptr);

    Agape48Engine *engine() const { return m_engine; }
    QColor pixelColor() const { return m_pixelColor; }
    QColor backgroundColor() const { return m_backgroundColor; }
    bool grayscale() const { return m_grayscale; }

    void setEngine(Agape48Engine *engine);
    void setPixelColor(const QColor &c);
    void setBackgroundColor(const QColor &c);
    void setGrayscale(bool on);

signals:
    void engineChanged();
    void pixelColorChanged();
    void backgroundColorChanged();
    void grayscaleChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override;

private:
    void rebuildColorTable();
    void onFrameReady();

    Agape48Engine *m_engine = nullptr;
    QImage  m_image;                  // Format_Indexed8, ĉirkaŭas la bufron de la kudro
    QImage  m_previous;               // asignita nur kiam grizoskalo estas ŝaltita
    QColor  m_pixelColor      { 0x00, 0x00, 0x00 };
    QColor  m_backgroundColor { 0x9f, 0xbf, 0x7a };   // la klasika verdo de HP 48
    bool    m_grayscale = false;
    bool    m_textureDirty = true;
    quint64 m_lastSerial = 0;
};
