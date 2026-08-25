// ---------------------------------------------------------------------------
// LcdItem - draws the x48 pixel buffer as a scene-graph texture node.
//
// A custom QQuickItem rather than a QML Canvas: Canvas round-trips through a
// JS 2D context and an FBO every frame, which on a phone is the difference
// between "free" and "visible battery cost". This path is one texture upload
// of 8.4 KB, only on frames the HP 48 actually changed, with nearest-neighbour
// filtering so the pixels stay square at any zoom.
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
    // The HP 48 fakes grey by toggling pixels between refreshes. Averaging two
    // consecutive frames reproduces it; off by default because it doubles the
    // upload rate for a feature most skins never exercise.
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
    QImage  m_image;                  // Format_Indexed8, wraps the shim buffer
    QImage  m_previous;               // only allocated when grayscale is on
    QColor  m_pixelColor      { 0x00, 0x00, 0x00 };
    QColor  m_backgroundColor { 0x9f, 0xbf, 0x7a };   // classic HP 48 green
    bool    m_grayscale = false;
    bool    m_textureDirty = true;
    quint64 m_lastSerial = 0;
};
