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
    // Indexed8 kun du- (aŭ tri-) enskriba paletro: la kudro jam donas al ni
    // unu bajton por bildero, do ĉi tie estas nenia laboro po bildero.
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
        node->setFiltering(QSGTexture::Nearest);      // kvadrataj bilderoj, ĉiam
        node->setOwnsTexture(true);
        m_textureDirty = true;
    }

    if (m_textureDirty) {
        const x48_frame_t &f = m_engine->frame();
        const int w = f.width  > 0 ? f.width  : X48_LCD_WIDTH;
        const int h = f.height > 0 ? f.height : X48_LCD_HEIGHT;
        // Nulo ĝis la unua kadro alvenas, kaj QImage kun nula paŝlarĝo estas
        // nula bildo.
        const int stride = f.stride > 0 ? f.stride : X48_LCD_STRIDE;

        // Ĉirkaŭas la bufron de la kudro sen kopii. Sendanĝere ĉar ni estas
        // interne de updatePaintNode(), kie la fadeno de la fasado estas
        // blokita kaj ne povas tiktaki.
        m_image = QImage(f.pixels, w, h, stride, QImage::Format_Indexed8);
        rebuildColorTable();

        // TODO(grayscale): kiam m_grayscale estas ŝaltita, kunigu per OR ĉi
        // tiun kadron kun m_previous en trinivelan indeksbufron antaŭ la
        // alŝuto, poste konservu kopion. Bezonas propran laborbufron - la
        // ĉirkaŭigo supre estas nurlega.

        // Kadro kiu neniam alvenis - nenia ROM, aŭ la motoro malsukcesis
        // starti - lasas nulan QImage, kaj createTextureFromImage() tiam
        // redonas nullptr. QSGSimpleTextureNode::setTexture(nullptr) kaŭzas
        // segmentan fiaskon en la bildiga fadeno, do desegnu tute nenion ĝis
        // estos io por desegni. Jen kio kraŝigis la unuan Linuksan konstruon
        // je 2026aug29.
        QSGTexture *tex = m_image.isNull()
            ? nullptr
            : window()->createTextureFromImage(m_image,
                                               QQuickWindow::TextureIsOpaque);
        if (!tex) {
            delete node;
            return nullptr;
        }
        // setOwnsTexture(true) supre signifas ke la nodo mem forigas la
        // MALNOVAN teksturon interne de setTexture(). Forigi ĝin ĉi tie unue
        // lasis la nodon teni pendantan montrilon kiun setTexture() poste
        // forigis denove - duobla liberigo kiu kaŭzis segmentan fiaskon en la
        // bildiga fadeno je la unua Linuksa konstruo, 2026aug29.
        node->setTexture(tex);
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
