#include "LcdItem.h"

#include "Agape48Engine.h"

#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>

LcdItem::LcdItem(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true);
    setSmooth(false);
}

void LcdItem::setEngine(Agape48Engine *engine)
{
    if (m_engine == engine)
        return;
    if (m_engine)
        disconnect(m_engine, nullptr, this, nullptr);
    m_engine = engine;
    if (m_engine) {
        connect(m_engine, &Agape48Engine::frameReady, this, &LcdItem::onFrameReady);
        connect(m_engine, &QObject::destroyed, this, [this] { m_engine = nullptr; });
    }
    m_textureDirty = true;
    emit engineChanged();
    update();
}

void LcdItem::onFrameReady()
{
    if (!m_engine || m_engine->frameSerial() == m_lastSerial)
        return;
    m_lastSerial = m_engine->frameSerial();
    m_textureDirty = true;
    update();
}

void LcdItem::rebuildColorTable()
{
    // Indexed8 with a two- (or three-) entry palette: the shim hands us one
    // byte per pixel already, so there is no per-pixel work at all here.
    QList<QRgb> table;
    table.reserve(3);
    table << m_backgroundColor.rgb() << m_pixelColor.rgb();
    if (m_grayscale) {
        const QColor mid = QColor::fromRgbF(
            (m_pixelColor.redF()   + m_backgroundColor.redF())   / 2.0,
            (m_pixelColor.greenF() + m_backgroundColor.greenF()) / 2.0,
            (m_pixelColor.blueF()  + m_backgroundColor.blueF())  / 2.0);
        table << mid.rgb();
    }
    m_image.setColorTable(table);
}

QSGNode *LcdItem::updatePaintNode(QSGNode *old, UpdatePaintNodeData *)
{
    if (!m_engine || width() <= 0 || height() <= 0) {
        delete old;
        return nullptr;
    }

    auto *node = static_cast<QSGSimpleTextureNode *>(old);
    if (!node) {
        node = new QSGSimpleTextureNode;
        node->setFiltering(QSGTexture::Nearest);      // square pixels, always
        node->setOwnsTexture(true);
        m_textureDirty = true;
    }

    if (m_textureDirty) {
        const x48_frame_t &f = m_engine->frame();
        const int w = f.width  > 0 ? f.width  : X48_LCD_WIDTH;
        const int h = f.height > 0 ? f.height : X48_LCD_HEIGHT;

        // Wraps the shim's buffer without copying. Safe because we are inside
        // updatePaintNode(), where the GUI thread is blocked and cannot tick.
        m_image = QImage(f.pixels, w, h, f.stride, QImage::Format_Indexed8);
        rebuildColorTable();

        // TODO(grayscale): when m_grayscale is on, OR this frame with
        // m_previous into a 3-level index buffer before upload, then keep a
        // copy. Needs its own scratch buffer - the wrap above is read-only.

        delete node->texture();
        node->setTexture(window()->createTextureFromImage(
            m_image, QQuickWindow::TextureIsOpaque));
        m_textureDirty = false;
    }

    node->setRect(boundingRect());
    return node;
}

void LcdItem::setPixelColor(const QColor &c)
{
    if (m_pixelColor == c) return;
    m_pixelColor = c;
    m_textureDirty = true;
    emit pixelColorChanged();
    update();
}

void LcdItem::setBackgroundColor(const QColor &c)
{
    if (m_backgroundColor == c) return;
    m_backgroundColor = c;
    m_textureDirty = true;
    emit backgroundColorChanged();
    update();
}

void LcdItem::setGrayscale(bool on)
{
    if (m_grayscale == on) return;
    m_grayscale = on;
    m_previous = QImage();
    m_textureDirty = true;
    emit grayscaleChanged();
    update();
}
