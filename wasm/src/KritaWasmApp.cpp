#include "KritaWasmApp.h"

#include <QApplication>
#include <QColorDialog>
#include <QDir>
#include <QDockWidget>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStatusBar>
#include <QTabletEvent>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWheelEvent>

// Real KDE/krita core headers compiled directly from /opt/krita
#include <KritaVersionWrapper.h>
#include <KisSupportedArchitectures.h>
#include <KoColor.h>
#include <KoColorModelStandardIds.h>
#include <KoColorProfile.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoCompositeOp.h>
#include <KoCompositeOpRegistry.h>
#include <KoStore.h>
#include <klocalizedstring.h>
#include <kundo2command.h>
#include <kundo2view.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <cmath>
#include <cstring>

namespace {

KritaWasmMainWindow *g_mainWindow = nullptr;

class KisWasmStrokeCommand : public KUndo2Command
{
public:
    KisWasmStrokeCommand(KritaCanvasWidget *canvas,
                         int layerIndex,
                         const QRect &dirtyRect,
                         const QByteArray &beforeRegion,
                         const QByteArray &afterRegion,
                         const KUndo2MagicString &text)
        : KUndo2Command(text)
        , m_canvas(canvas)
        , m_layerIndex(layerIndex)
        , m_rect(dirtyRect)
        , m_before(beforeRegion)
        , m_after(afterRegion)
    {}

    void undo() override {
        if (m_canvas) {
            m_canvas->applyLayerPatch(m_layerIndex, m_rect, m_before);
        }
    }

    void redo() override {
        if (m_firstRedo) {
            m_firstRedo = false;
            return;
        }
        if (m_canvas) {
            m_canvas->applyLayerPatch(m_layerIndex, m_rect, m_after);
        }
    }

private:
    KritaCanvasWidget *m_canvas;
    int m_layerIndex;
    QRect m_rect;
    QByteArray m_before;
    QByteArray m_after;
    bool m_firstRedo {true};
};

QByteArray extractRegionBytes(const QByteArray &fullBuffer, int strideBytes, int pixelSize, const QRect &rect)
{
    if (rect.isEmpty()) return {};
    const int rowBytes = rect.width() * pixelSize;
    QByteArray out(rowBytes * rect.height(), Qt::Uninitialized);
    for (int y = 0; y < rect.height(); ++y) {
        const char *src = fullBuffer.constData() + ((rect.y() + y) * strideBytes) + (rect.x() * pixelSize);
        char *dst = out.data() + (y * rowBytes);
        std::memcpy(dst, src, static_cast<size_t>(rowBytes));
    }
    return out;
}

void notifyBrowserHost(const QString &eventType, const QString &payloadJson = QStringLiteral("{}"))
{
#ifdef __EMSCRIPTEN__
    const QByteArray ev = eventType.toUtf8();
    const QByteArray pl = payloadJson.toUtf8();
    EM_ASM({
        if (typeof window !== 'undefined' && typeof window.__kritaWasmBridgeEvent === 'function') {
            window.__kritaWasmBridgeEvent(UTF8ToString($0), UTF8ToString($1));
        }
    }, ev.constData(), pl.constData());
#else
    Q_UNUSED(eventType);
    Q_UNUSED(payloadJson);
#endif
}

} // namespace

KritaCanvasWidget::KritaCanvasWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAttribute(Qt::WA_AcceptTouchEvents, true);
    m_undoStack = new KUndo2Stack(this);
    newDocument(1280, 800, RGBAColorModelID.id(), Integer8BitsColorDepthID.id());
}

KritaCanvasWidget::~KritaCanvasWidget()
{
    makeCurrent();
    if (m_canvasTex != 0) {
        glDeleteTextures(1, &m_canvasTex);
        m_canvasTex = 0;
    }
    doneCurrent();
}

void KritaCanvasWidget::newDocument(int width, int height, const QString &colorModelId, const QString &colorDepthId)
{
    m_width = qBound(64, width, 4096);
    m_height = qBound(64, height, 4096);

    KoColorSpaceRegistry *registry = KoColorSpaceRegistry::instance();
    const KoColorSpace *cs = registry->colorSpace(colorModelId, colorDepthId, QString());
    if (!cs) {
        cs = registry->rgb8();
    }
    m_colorSpace = cs;

    m_layers.clear();
    m_undoStack->clear();

    const int pixelSize = static_cast<int>(m_colorSpace->pixelSize());
    const int totalBytes = m_width * m_height * pixelSize;

    // Layer 0: Paper background filled using real KoColor + KoColorSpace pixel conversion
    KritaWasmLayer bg;
    bg.name = i18n("Background");
    bg.visible = true;
    bg.opacity = 255;
    bg.compositeOpId = COMPOSITE_OVER;
    bg.rawPixels.resize(totalBytes);

    KoColor paperColor(QColor(248, 249, 250), m_colorSpace);
    quint8 *dst = reinterpret_cast<quint8 *>(bg.rawPixels.data());
    const int numPixels = m_width * m_height;
    for (int i = 0; i < numPixels; ++i) {
        std::memcpy(dst + i * pixelSize, paperColor.data(), static_cast<size_t>(pixelSize));
    }

    // Layer 1: Transparent Paint Layer
    KritaWasmLayer paint;
    paint.name = i18n("Paint Layer 1");
    paint.visible = true;
    paint.opacity = 255;
    paint.compositeOpId = COMPOSITE_OVER;
    paint.rawPixels.fill(0, totalBytes);

    m_layers.push_back(std::move(bg));
    m_layers.push_back(std::move(paint));
    m_activeLayer = 1;

    m_compositedRaw.resize(totalBytes);
    m_compositedSrgb = QImage(m_width, m_height, QImage::Format_RGBA8888);

    m_hasUserViewTransform = false;
    resetView();
    recomposeDocument();
    emit layersChanged();
    emit documentChanged();
}

void KritaCanvasWidget::recomposeDocument()
{
    if (!m_colorSpace || m_layers.empty()) return;

    const int pixelSize = static_cast<int>(m_colorSpace->pixelSize());
    const int totalBytes = m_width * m_height * pixelSize;
    if (m_compositedRaw.size() != totalBytes) {
        m_compositedRaw.resize(totalBytes);
    }
    m_compositedRaw.fill(0);

    // Real Krita KoColorSpace::bitBlt compositing across layers!
    KoCompositeOp::ParameterInfo params;
    params.dstRowStart = reinterpret_cast<quint8 *>(m_compositedRaw.data());
    params.dstRowStride = m_width * pixelSize;
    params.maskRowStart = nullptr;
    params.maskRowStride = 0;
    params.rows = m_height;
    params.cols = m_width;
    params.flow = 1.0f;

    for (const KritaWasmLayer &layer : m_layers) {
        if (!layer.visible || layer.opacity == 0 || layer.rawPixels.size() != totalBytes) {
            continue;
        }
        const KoCompositeOp *op = m_colorSpace->compositeOp(layer.compositeOpId);
        if (!op) {
            op = m_colorSpace->compositeOp(COMPOSITE_OVER);
        }
        params.srcRowStart = reinterpret_cast<const quint8 *>(layer.rawPixels.constData());
        params.srcRowStride = m_width * pixelSize;
        params.opacity = static_cast<float>(layer.opacity) / 255.0f;
        m_colorSpace->bitBlt(
            m_colorSpace,
            params,
            op,
            KoColorConversionTransformation::internalRenderingIntent(),
            KoColorConversionTransformation::internalConversionFlags()
        );
    }

    // Convert active Krita KoColorSpace (e.g. RGBA8, RGBA16, LABA) to sRGB display QImage via LittleCMS2!
    if (m_compositedSrgb.width() != m_width || m_compositedSrgb.height() != m_height) {
        m_compositedSrgb = QImage(m_width, m_height, QImage::Format_RGBA8888);
    }
    const KoColorSpace *displayCs = KoColorSpaceRegistry::instance()->rgb8();
    if (m_colorSpace == displayCs) {
        // Krita RGB8 internal layout is BGRA8888 on little-endian; convert to RGBA8888 for GL_RGBA
        const quint8 *src = reinterpret_cast<const quint8 *>(m_compositedRaw.constData());
        quint8 *dst = m_compositedSrgb.bits();
        const int count = m_width * m_height;
        for (int i = 0; i < count; ++i) {
            dst[i * 4 + 0] = src[i * 4 + 2];
            dst[i * 4 + 1] = src[i * 4 + 1];
            dst[i * 4 + 2] = src[i * 4 + 0];
            dst[i * 4 + 3] = src[i * 4 + 3];
        }
    } else {
        // Real ICC color transformation via KoColorSpace::convertPixelsTo
        QByteArray bgraOut(m_width * m_height * 4, Qt::Uninitialized);
        m_colorSpace->convertPixelsTo(
            reinterpret_cast<const quint8 *>(m_compositedRaw.constData()),
            reinterpret_cast<quint8 *>(bgraOut.data()),
            displayCs,
            static_cast<quint32>(m_width * m_height),
            KoColorConversionTransformation::internalRenderingIntent(),
            KoColorConversionTransformation::internalConversionFlags()
        );
        const quint8 *src = reinterpret_cast<const quint8 *>(bgraOut.constData());
        quint8 *dst = m_compositedSrgb.bits();
        const int count = m_width * m_height;
        for (int i = 0; i < count; ++i) {
            dst[i * 4 + 0] = src[i * 4 + 2];
            dst[i * 4 + 1] = src[i * 4 + 1];
            dst[i * 4 + 2] = src[i * 4 + 0];
            dst[i * 4 + 3] = src[i * 4 + 3];
        }
    }

    m_textureDirty = true;
    update();
}

void KritaCanvasWidget::applyLayerPatch(int layerIndex, const QRect &rect, const QByteArray &pixels)
{
    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_layers.size()) || !m_colorSpace) return;
    const int pixelSize = static_cast<int>(m_colorSpace->pixelSize());
    const int strideBytes = m_width * pixelSize;
    const int patchRowBytes = rect.width() * pixelSize;

    KritaWasmLayer &layer = m_layers[static_cast<size_t>(layerIndex)];
    for (int y = 0; y < rect.height(); ++y) {
        const char *src = pixels.constData() + y * patchRowBytes;
        char *dst = layer.rawPixels.data() + ((rect.y() + y) * strideBytes) + (rect.x() * pixelSize);
        std::memcpy(dst, src, static_cast<size_t>(patchRowBytes));
    }
    recomposeDocument();
    emit documentChanged();
}

void KritaCanvasWidget::addLayer(const QString &name)
{
    if (!m_colorSpace) return;
    const int pixelSize = static_cast<int>(m_colorSpace->pixelSize());
    KritaWasmLayer layer;
    layer.name = name.isEmpty() ? i18n("Paint Layer %1", static_cast<int>(m_layers.size())) : name;
    layer.visible = true;
    layer.opacity = 255;
    layer.compositeOpId = COMPOSITE_OVER;
    layer.rawPixels.fill(0, m_width * m_height * pixelSize);
    m_layers.push_back(std::move(layer));
    m_activeLayer = static_cast<int>(m_layers.size()) - 1;
    recomposeDocument();
    emit layersChanged();
}

void KritaCanvasWidget::removeActiveLayer()
{
    if (m_layers.size() <= 1 || m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_layers.size())) {
        return;
    }
    m_layers.erase(m_layers.begin() + m_activeLayer);
    if (m_activeLayer >= static_cast<int>(m_layers.size())) {
        m_activeLayer = static_cast<int>(m_layers.size()) - 1;
    }
    recomposeDocument();
    emit layersChanged();
}

void KritaCanvasWidget::setActiveLayer(int index)
{
    if (index >= 0 && index < static_cast<int>(m_layers.size())) {
        m_activeLayer = index;
        emit layersChanged();
    }
}

void KritaCanvasWidget::setLayerVisible(int index, bool visible)
{
    if (index >= 0 && index < static_cast<int>(m_layers.size())) {
        m_layers[static_cast<size_t>(index)].visible = visible;
        recomposeDocument();
        emit layersChanged();
    }
}

void KritaCanvasWidget::setLayerOpacity(int index, quint8 opacity)
{
    if (index >= 0 && index < static_cast<int>(m_layers.size())) {
        m_layers[static_cast<size_t>(index)].opacity = opacity;
        recomposeDocument();
    }
}

void KritaCanvasWidget::setLayerBlendMode(int index, const QString &compositeOpId)
{
    if (index >= 0 && index < static_cast<int>(m_layers.size())) {
        m_layers[static_cast<size_t>(index)].compositeOpId = compositeOpId;
        recomposeDocument();
    }
}

void KritaCanvasWidget::setBrushColor(const QColor &c)
{
    if (c.isValid()) {
        m_brushColor = c;
    }
}

void KritaCanvasWidget::setZoom(double zoom)
{
    m_hasUserViewTransform = true;
    m_zoom = qBound(0.1, zoom, 16.0);
    update();
}

void KritaCanvasWidget::resetView()
{
    if (width() > 64 && height() > 64 && m_width > 0 && m_height > 0) {
        const double zx = (width() - 60.0) / static_cast<double>(m_width);
        const double zy = (height() - 60.0) / static_cast<double>(m_height);
        m_zoom = qBound(0.15, qMin(zx, zy), 2.0);
    } else {
        m_zoom = 0.65;
    }
    m_panOffset = QPointF(0.0, 0.0);
    m_hasUserViewTransform = false;
    update();
}

void KritaCanvasWidget::initializeGL()
{
    initializeOpenGLFunctions();
    const char *renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    m_glInfo = QStringLiteral("%1 (%2)").arg(
        renderer ? QString::fromUtf8(renderer) : QStringLiteral("WebGL"),
        version ? QString::fromUtf8(version) : QStringLiteral("OpenGL ES")
    );
    glGenTextures(1, &m_canvasTex);
    glBindTexture(GL_TEXTURE_2D, m_canvasTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_glInitialized = true;
    m_textureDirty = true;
}

void KritaCanvasWidget::resizeGL(int w, int h)
{
    if (!m_hasUserViewTransform && w > 64 && h > 64 && m_width > 0 && m_height > 0) {
        const double zx = (w - 60.0) / static_cast<double>(m_width);
        const double zy = (h - 60.0) / static_cast<double>(m_height);
        m_zoom = qBound(0.15, qMin(zx, zy), 2.0);
        m_panOffset = QPointF(0.0, 0.0);
    }
}

void KritaCanvasWidget::uploadCompositeTexture()
{
    if (!m_glInitialized || m_compositedSrgb.isNull()) return;
    glBindTexture(GL_TEXTURE_2D, m_canvasTex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_compositedSrgb.width(), m_compositedSrgb.height(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, m_compositedSrgb.constBits());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
    m_textureDirty = false;
}

void KritaCanvasWidget::paintGL()
{
    if (m_textureDirty && m_glInitialized) {
        uploadCompositeTexture();
    }

    QPainter p(this);
    p.fillRect(rect(), QColor(34, 37, 42));

    const double drawW = m_width * m_zoom;
    const double drawH = m_height * m_zoom;
    const double originX = (width() - drawW) * 0.5 + m_panOffset.x();
    const double originY = (height() - drawH) * 0.5 + m_panOffset.y();
    const QRectF targetRect(originX, originY, drawW, drawH);

    // Transparency checkerboard behind canvas
    p.save();
    p.setClipRect(targetRect);
    p.fillRect(targetRect, QColor(180, 180, 180));
    const int step = 16;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(220, 220, 220));
    for (int y = int(originY); y < int(originY + drawH); y += step) {
        for (int x = int(originX); x < int(originX + drawW); x += step) {
            if (((x - int(originX)) / step + (y - int(originY)) / step) % 2 == 0) {
                p.drawRect(x, y, step, step);
            }
        }
    }
    p.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.5);
    p.drawImage(targetRect, m_compositedSrgb);
    p.restore();

    p.setPen(QPen(QColor(72, 78, 88), 1));
    p.drawRect(targetRect);
}

QPointF KritaCanvasWidget::widgetToCanvas(const QPointF &pos) const
{
    const double drawW = m_width * m_zoom;
    const double drawH = m_height * m_zoom;
    const double originX = (width() - drawW) * 0.5 + m_panOffset.x();
    const double originY = (height() - drawH) * 0.5 + m_panOffset.y();
    return QPointF((pos.x() - originX) / m_zoom, (pos.y() - originY) / m_zoom);
}

void KritaCanvasWidget::beginStroke(const QPointF &canvasPos, float pressure, float tiltX, float tiltY)
{
    if (m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_layers.size())) return;
    m_isDrawing = true;
    m_lastCanvasPos = canvasPos;
    m_strokeBeforeRaw = m_layers[static_cast<size_t>(m_activeLayer)].rawPixels;
    m_strokeDirtyRect = QRect();
    stampDab(canvasPos, pressure, tiltX, tiltY);
    recomposeDocument();
}

void KritaCanvasWidget::continueStroke(const QPointF &canvasPos, float pressure, float tiltX, float tiltY)
{
    if (!m_isDrawing) return;
    const double dx = canvasPos.x() - m_lastCanvasPos.x();
    const double dy = canvasPos.y() - m_lastCanvasPos.y();
    const double dist = std::hypot(dx, dy);
    const double radius = qMax(0.5, static_cast<double>(m_brushSize) * (0.25 + 0.75 * pressure) * 0.5);
    const double spacing = qMax(1.0, radius * 0.25);

    if (dist <= spacing) {
        stampDab(canvasPos, pressure, tiltX, tiltY);
    } else {
        const int steps = static_cast<int>(std::ceil(dist / spacing));
        for (int i = 1; i <= steps; ++i) {
            const double t = static_cast<double>(i) / steps;
            const QPointF pt(m_lastCanvasPos.x() + dx * t, m_lastCanvasPos.y() + dy * t);
            stampDab(pt, pressure, tiltX, tiltY);
        }
    }
    m_lastCanvasPos = canvasPos;
    recomposeDocument();
}

void KritaCanvasWidget::endStroke()
{
    if (!m_isDrawing) return;
    m_isDrawing = false;
    if (!m_strokeDirtyRect.isEmpty() && m_activeLayer >= 0 && m_activeLayer < static_cast<int>(m_layers.size()) && m_colorSpace) {
        const int pixelSize = static_cast<int>(m_colorSpace->pixelSize());
        const int stride = m_width * pixelSize;
        const QRect bounded = m_strokeDirtyRect.intersected(QRect(0, 0, m_width, m_height));
        if (!bounded.isEmpty()) {
            const QByteArray beforePatch = extractRegionBytes(m_strokeBeforeRaw, stride, pixelSize, bounded);
            const QByteArray afterPatch = extractRegionBytes(m_layers[static_cast<size_t>(m_activeLayer)].rawPixels, stride, pixelSize, bounded);
            m_undoStack->push(new KisWasmStrokeCommand(
                this,
                m_activeLayer,
                bounded,
                beforePatch,
                afterPatch,
                kundo2_i18n(m_eraserMode ? "Erase Stroke" : "Freehand Brush Stroke")
            ));
        }
    }
    m_strokeBeforeRaw.clear();
    emit documentChanged();
}

void KritaCanvasWidget::stampDab(const QPointF &canvasPos, float pressure, float tiltX, float tiltY)
{
    Q_UNUSED(tiltX);
    Q_UNUSED(tiltY);
    if (!m_colorSpace || m_activeLayer < 0 || m_activeLayer >= static_cast<int>(m_layers.size())) return;

    const float effectivePressure = qBound(0.05f, pressure, 1.0f);
    const float radius = qMax(0.75f, (m_brushSize * 0.5f) * (0.25f + 0.75f * effectivePressure));
    const int minX = qMax(0, static_cast<int>(std::floor(canvasPos.x() - radius - 1.0f)));
    const int maxX = qMin(m_width - 1, static_cast<int>(std::ceil(canvasPos.x() + radius + 1.0f)));
    const int minY = qMax(0, static_cast<int>(std::floor(canvasPos.y() - radius - 1.0f)));
    const int maxY = qMin(m_height - 1, static_cast<int>(std::ceil(canvasPos.y() + radius + 1.0f)));
    if (minX > maxX || minY > maxY) return;

    const int dabW = maxX - minX + 1;
    const int dabH = maxY - minY + 1;
    m_strokeDirtyRect = m_strokeDirtyRect.united(QRect(minX, minY, dabW, dabH));

    const int pixelSize = static_cast<int>(m_colorSpace->pixelSize());
    KoColor dabColor(m_brushColor, m_colorSpace);

    QByteArray dabPixels(dabW * dabH * pixelSize, Qt::Uninitialized);
    QByteArray dabMask(dabW * dabH, Qt::Uninitialized);

    for (int y = 0; y < dabH; ++y) {
        const float dy = static_cast<float>(minY + y) - static_cast<float>(canvasPos.y());
        for (int x = 0; x < dabW; ++x) {
            const float dx = static_cast<float>(minX + x) - static_cast<float>(canvasPos.x());
            const float d = std::sqrt(dx * dx + dy * dy) / radius;
            quint8 alpha = 0;
            if (d < 1.0f) {
                float falloff = 1.0f;
                if (d > m_brushHardness) {
                    const float t = (d - m_brushHardness) / qMax(0.001f, 1.0f - m_brushHardness);
                    falloff = 1.0f - (t * t * (3.0f - 2.0f * t));
                }
                alpha = static_cast<quint8>(qBound(0.0f, falloff * 255.0f, 255.0f));
            }
            const int idx = y * dabW + x;
            dabMask[idx] = static_cast<char>(alpha);
            std::memcpy(dabPixels.data() + idx * pixelSize, dabColor.data(), static_cast<size_t>(pixelSize));
        }
    }

    KritaWasmLayer &layer = m_layers[static_cast<size_t>(m_activeLayer)];
    const KoCompositeOp *op = m_colorSpace->compositeOp(m_eraserMode ? COMPOSITE_ERASE : COMPOSITE_OVER);
    if (!op) op = m_colorSpace->compositeOp(COMPOSITE_OVER);

    const int dstStride = m_width * pixelSize;
    const int srcStride = dabW * pixelSize;
    quint8 *dstPtr = reinterpret_cast<quint8 *>(layer.rawPixels.data()) + (minY * dstStride) + (minX * pixelSize);

    KoCompositeOp::ParameterInfo params;
    params.dstRowStart = dstPtr;
    params.dstRowStride = dstStride;
    params.srcRowStart = reinterpret_cast<const quint8 *>(dabPixels.constData());
    params.srcRowStride = srcStride;
    params.maskRowStart = reinterpret_cast<const quint8 *>(dabMask.constData());
    params.maskRowStride = dabW;
    params.rows = dabH;
    params.cols = dabW;
    params.opacity = qBound(0.01f, m_brushOpacity * effectivePressure, 1.0f);
    params.flow = 1.0f;

    m_colorSpace->bitBlt(
        m_colorSpace,
        params,
        op,
        KoColorConversionTransformation::internalRenderingIntent(),
        KoColorConversionTransformation::internalConversionFlags()
    );
}

void KritaCanvasWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton || (e->button() == Qt::LeftButton && m_spaceDown)) {
        m_isPanning = true;
        m_lastPanPos = e->position();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (e->button() == Qt::LeftButton) {
        beginStroke(widgetToCanvas(e->position()), 0.85f, 0.0f, 0.0f);
    }
}

void KritaCanvasWidget::mouseMoveEvent(QMouseEvent *e)
{
    const QPointF cPos = widgetToCanvas(e->position());
    emit cursorMoved(static_cast<int>(cPos.x()), static_cast<int>(cPos.y()), m_isDrawing ? 0.85f : 0.0f, 0.0f, 0.0f);

    if (m_isPanning) {
        m_hasUserViewTransform = true;
        const QPointF delta = e->position() - m_lastPanPos;
        m_panOffset += delta;
        m_lastPanPos = e->position();
        update();
        return;
    }
    if (m_isDrawing) {
        continueStroke(cPos, 0.85f, 0.0f, 0.0f);
    }
}

void KritaCanvasWidget::mouseReleaseEvent(QMouseEvent *e)
{
    if (m_isPanning) {
        m_isPanning = false;
        setCursor(m_spaceDown ? Qt::OpenHandCursor : Qt::CrossCursor);
        return;
    }
    if (e->button() == Qt::LeftButton && m_isDrawing) {
        endStroke();
    }
}

void KritaCanvasWidget::tabletEvent(QTabletEvent *e)
{
    const QPointF cPos = widgetToCanvas(e->position());
    const float pressure = static_cast<float>(e->pressure());
    const float tiltX = static_cast<float>(e->xTilt());
    const float tiltY = static_cast<float>(e->yTilt());
    emit cursorMoved(static_cast<int>(cPos.x()), static_cast<int>(cPos.y()), pressure, tiltX, tiltY);

    switch (e->type()) {
    case QEvent::TabletPress:
        beginStroke(cPos, pressure, tiltX, tiltY);
        e->accept();
        break;
    case QEvent::TabletMove:
        if (m_isDrawing) {
            continueStroke(cPos, pressure, tiltX, tiltY);
        }
        e->accept();
        break;
    case QEvent::TabletRelease:
        endStroke();
        e->accept();
        break;
    default:
        break;
    }
}

void KritaCanvasWidget::wheelEvent(QWheelEvent *e)
{
    const double factor = e->angleDelta().y() > 0 ? 1.12 : 0.89;
    setZoom(m_zoom * factor);
    e->accept();
}

void KritaCanvasWidget::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
        m_spaceDown = true;
        setCursor(Qt::OpenHandCursor);
    } else if (e->key() == Qt::Key_BracketLeft) {
        setBrushSize(m_brushSize - 4.0f);
    } else if (e->key() == Qt::Key_BracketRight) {
        setBrushSize(m_brushSize + 4.0f);
    } else if (e->key() == Qt::Key_E) {
        m_eraserMode = true;
    } else if (e->key() == Qt::Key_B) {
        m_eraserMode = false;
    } else if (e->key() == Qt::Key_0 && (e->modifiers() & Qt::ControlModifier)) {
        resetView();
    }
    QOpenGLWidget::keyPressEvent(e);
}

void KritaCanvasWidget::keyReleaseEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Space && !e->isAutoRepeat()) {
        m_spaceDown = false;
        setCursor(Qt::CrossCursor);
    }
    QOpenGLWidget::keyReleaseEvent(e);
}

bool KritaCanvasWidget::saveToKra(const QString &vfsPath)
{
    if (!m_colorSpace) return false;
    std::unique_ptr<KoStore> store(KoStore::createStore(vfsPath, KoStore::Write, "application/x-krita", KoStore::Zip));
    if (!store || store->bad()) return false;

    // 1. Write maindoc.xml (Krita ORA/KRA XML document descriptor)
    QDomDocument doc(QStringLiteral("DOC"));
    QDomElement root = doc.createElement(QStringLiteral("DOC"));
    root.setAttribute(QStringLiteral("editor"), QStringLiteral("Krita WebAssembly"));
    root.setAttribute(QStringLiteral("syntaxVersion"), QStringLiteral("2.0"));
    root.setAttribute(QStringLiteral("kritaVersion"), KritaVersionWrapper::versionString());
    doc.appendChild(root);

    QDomElement imageEl = doc.createElement(QStringLiteral("IMAGE"));
    imageEl.setAttribute(QStringLiteral("width"), m_width);
    imageEl.setAttribute(QStringLiteral("height"), m_height);
    imageEl.setAttribute(QStringLiteral("colorspacename"), m_colorSpace->id());
    imageEl.setAttribute(QStringLiteral("mime"), QStringLiteral("application/x-kra"));
    imageEl.setAttribute(QStringLiteral("x-res"), 300);
    imageEl.setAttribute(QStringLiteral("y-res"), 300);
    root.appendChild(imageEl);

    QDomElement layersEl = doc.createElement(QStringLiteral("LAYERS"));
    imageEl.appendChild(layersEl);

    for (size_t i = 0; i < m_layers.size(); ++i) {
        const KritaWasmLayer &l = m_layers[i];
        QDomElement layerEl = doc.createElement(QStringLiteral("layer"));
        layerEl.setAttribute(QStringLiteral("name"), l.name);
        layerEl.setAttribute(QStringLiteral("nodetype"), QStringLiteral("paintlayer"));
        layerEl.setAttribute(QStringLiteral("visible"), l.visible ? 1 : 0);
        layerEl.setAttribute(QStringLiteral("opacity"), l.opacity);
        layerEl.setAttribute(QStringLiteral("compositeop"), l.compositeOpId);
        layerEl.setAttribute(QStringLiteral("colorspacename"), m_colorSpace->id());
        layerEl.setAttribute(QStringLiteral("filename"), QStringLiteral("layer%1").arg(i));
        layersEl.appendChild(layerEl);

        // Save raw layer pixel buffer inside .kra zip archive
        if (store->open(QStringLiteral("krita/layers/layer%1.raw").arg(i))) {
            store->write(l.rawPixels);
            store->close();
        }
    }

    if (store->open(QStringLiteral("maindoc.xml"))) {
        store->write(doc.toByteArray(2));
        store->close();
    }

    // 2. Write mergedimage.png & preview.png
    if (store->open(QStringLiteral("mergedimage.png"))) {
        m_compositedSrgb.save(store->device(), "PNG");
        store->close();
    }
    if (store->open(QStringLiteral("preview.png"))) {
        m_compositedSrgb.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(store->device(), "PNG");
        store->close();
    }

    return store->finalize();
}

bool KritaCanvasWidget::loadFromKra(const QString &vfsPath)
{
    std::unique_ptr<KoStore> store(KoStore::createStore(vfsPath, KoStore::Read, "application/x-krita", KoStore::Zip));
    if (!store || store->bad()) return false;

    if (!store->open(QStringLiteral("maindoc.xml"))) {
        return false;
    }
    const QByteArray xmlBytes = store->read(store->size());
    store->close();

    QDomDocument doc;
    if (!doc.setContent(xmlBytes)) {
        return false;
    }
    QDomElement imageEl = doc.documentElement().firstChildElement(QStringLiteral("IMAGE"));
    if (imageEl.isNull()) return false;

    const int w = imageEl.attribute(QStringLiteral("width")).toInt();
    const int h = imageEl.attribute(QStringLiteral("height")).toInt();
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return false;

    const QString csId = imageEl.attribute(QStringLiteral("colorspacename"), QStringLiteral("RGBA"));
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->colorSpace(csId, QString());
    if (!cs) cs = KoColorSpaceRegistry::instance()->rgb8();

    m_width = w;
    m_height = h;
    m_colorSpace = cs;
    m_layers.clear();
    m_undoStack->clear();

    const int expectedBytes = m_width * m_height * static_cast<int>(m_colorSpace->pixelSize());
    QDomElement layersEl = imageEl.firstChildElement(QStringLiteral("LAYERS"));
    QDomElement layerEl = layersEl.firstChildElement(QStringLiteral("layer"));
    while (!layerEl.isNull()) {
        KritaWasmLayer l;
        l.name = layerEl.attribute(QStringLiteral("name"), QStringLiteral("Layer"));
        l.visible = layerEl.attribute(QStringLiteral("visible"), QStringLiteral("1")).toInt() != 0;
        l.opacity = static_cast<quint8>(qBound(0, layerEl.attribute(QStringLiteral("opacity"), QStringLiteral("255")).toInt(), 255));
        l.compositeOpId = layerEl.attribute(QStringLiteral("compositeop"), COMPOSITE_OVER);
        const QString fname = layerEl.attribute(QStringLiteral("filename"));

        if (!fname.isEmpty() && store->open(QStringLiteral("krita/layers/%1.raw").arg(fname))) {
            l.rawPixels = store->read(store->size());
            store->close();
        }
        if (l.rawPixels.size() != expectedBytes) {
            l.rawPixels.fill(0, expectedBytes);
        }
        m_layers.push_back(std::move(l));
        layerEl = layerEl.nextSiblingElement(QStringLiteral("layer"));
    }

    // Fallback for desktop Krita .kra files that store mergedimage.png instead of our WASM raw layer chunks
    if (m_layers.empty() || std::all_of(m_layers.begin(), m_layers.end(), [](const KritaWasmLayer &l){
        for (char c : l.rawPixels) if (c != 0) return false;
        return true;
    })) {
        if (store->open(QStringLiteral("mergedimage.png"))) {
            const QByteArray pngData = store->read(store->size());
            store->close();
            QImage merged;
            if (merged.loadFromData(pngData, "PNG")) {
                merged = merged.convertToFormat(QImage::Format_RGBA8888);
                KritaWasmLayer imported;
                imported.name = i18n("Merged Krita Image");
                imported.visible = true;
                imported.opacity = 255;
                imported.compositeOpId = COMPOSITE_OVER;
                imported.rawPixels.resize(m_width * m_height * 4);
                const KoColorSpace *rgb8 = KoColorSpaceRegistry::instance()->rgb8();
                m_colorSpace = rgb8;
                const quint8 *src = merged.constBits();
                quint8 *dst = reinterpret_cast<quint8 *>(imported.rawPixels.data());
                const int count = qMin(m_width * m_height, merged.width() * merged.height());
                for (int i = 0; i < count; ++i) {
                    dst[i * 4 + 0] = src[i * 4 + 2];
                    dst[i * 4 + 1] = src[i * 4 + 1];
                    dst[i * 4 + 2] = src[i * 4 + 0];
                    dst[i * 4 + 3] = src[i * 4 + 3];
                }
                if (m_layers.empty()) m_layers.push_back(std::move(imported));
                else m_layers[0] = std::move(imported);
            }
        }
    }

    m_activeLayer = m_layers.empty() ? -1 : static_cast<int>(m_layers.size()) - 1;
    resetView();
    recomposeDocument();
    emit layersChanged();
    emit documentChanged();
    return true;
}

bool KritaCanvasWidget::loadFromRasterImage(const QString &vfsPath)
{
    QImage img(vfsPath);
    if (img.isNull()) return false;
    img = img.convertToFormat(QImage::Format_RGBA8888);
    newDocument(img.width(), img.height(), RGBAColorModelID.id(), Integer8BitsColorDepthID.id());
    if (m_layers.empty()) return false;

    KritaWasmLayer &layer = m_layers[0];
    const quint8 *src = img.constBits();
    quint8 *dst = reinterpret_cast<quint8 *>(layer.rawPixels.data());
    const int count = m_width * m_height;
    for (int i = 0; i < count; ++i) {
        dst[i * 4 + 0] = src[i * 4 + 2];
        dst[i * 4 + 1] = src[i * 4 + 1];
        dst[i * 4 + 2] = src[i * 4 + 0];
        dst[i * 4 + 3] = src[i * 4 + 3];
    }
    recomposeDocument();
    emit layersChanged();
    emit documentChanged();
    return true;
}

bool KritaCanvasWidget::exportToPng(const QString &vfsPath)
{
    return m_compositedSrgb.save(vfsPath, "PNG");
}

KritaWasmMainWindow::KritaWasmMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    g_mainWindow = this;
    setWindowTitle(QStringLiteral("Krita WebAssembly (%1)").arg(KritaVersionWrapper::versionString()));

    m_canvas = new KritaCanvasWidget(this);
    setCentralWidget(m_canvas);

    setupMenusAndToolbars();
    setupDocks();

    m_statusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_statusLabel, 1);

    connect(m_canvas, &KritaCanvasWidget::documentChanged, this, &KritaWasmMainWindow::syncUiFromCanvas);
    connect(m_canvas, &KritaCanvasWidget::layersChanged, this, &KritaWasmMainWindow::syncUiFromCanvas);
    connect(m_canvas, &KritaCanvasWidget::cursorMoved, this, [this](int x, int y, float pressure, float tiltX, float tiltY) {
        const KoColorSpace *cs = m_canvas->colorSpace();
        m_statusLabel->setText(QStringLiteral("%1 x %2 px  |  %3 (%4)  |  Pos: %5, %6  |  Pressure: %7%  |  Tilt: %8, %9  |  Arch: %10")
            .arg(m_canvas->docWidth())
            .arg(m_canvas->docHeight())
            .arg(cs ? cs->name() : QStringLiteral("RGBA"))
            .arg(cs && cs->profile() ? cs->profile()->name() : QStringLiteral("sRGB"))
            .arg(x).arg(y)
            .arg(int(pressure * 100.0f))
            .arg(int(tiltX)).arg(int(tiltY))
            .arg(KisSupportedArchitectures::baseArchName()));
    });

    syncUiFromCanvas();
    notifyBrowserHost(QStringLiteral("ready"), engineStatusJson());
}

KritaWasmMainWindow *KritaWasmMainWindow::instance()
{
    return g_mainWindow;
}

void KritaWasmMainWindow::setupMenusAndToolbars()
{
    QMenu *fileMenu = menuBar()->addMenu(i18n("&File"));
    fileMenu->addAction(i18n("&New Canvas..."), QKeySequence::New, this, &KritaWasmMainWindow::onNewDocumentDialog);
    fileMenu->addAction(i18n("&Open Local File (.kra / PNG / JPG)..."), QKeySequence::Open, this, &KritaWasmMainWindow::onTriggerBrowserOpen);
    fileMenu->addSeparator();
    fileMenu->addAction(i18n("Save Project to &Browser Storage"), QKeySequence::Save, this, &KritaWasmMainWindow::onSaveToBrowserStorage);
    fileMenu->addAction(i18n("Export .&kra Archive..."), this, &KritaWasmMainWindow::onExportKra);
    fileMenu->addAction(i18n("Export &PNG..."), this, &KritaWasmMainWindow::onExportPng);

    QMenu *editMenu = menuBar()->addMenu(i18n("&Edit"));
    QAction *undoAct = m_canvas->undoStack()->createUndoAction(nullptr, i18n("Undo"));
    undoAct->setShortcut(QKeySequence::Undo);
    QAction *redoAct = m_canvas->undoStack()->createRedoAction(nullptr, i18n("Redo"));
    redoAct->setShortcut(QKeySequence::Redo);
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);

    QMenu *layerMenu = menuBar()->addMenu(i18n("&Layer"));
    layerMenu->addAction(i18n("&Add Paint Layer"), this, [this]() { m_canvas->addLayer(); });
    layerMenu->addAction(i18n("&Remove Active Layer"), this, [this]() { m_canvas->removeActiveLayer(); });

    QMenu *viewMenu = menuBar()->addMenu(i18n("&View"));
    viewMenu->addAction(i18n("Zoom &In"), QKeySequence::ZoomIn, this, [this]() { m_canvas->setZoom(m_canvas->zoomFactor() * 1.25); });
    viewMenu->addAction(i18n("Zoom &Out"), QKeySequence::ZoomOut, this, [this]() { m_canvas->setZoom(m_canvas->zoomFactor() * 0.8); });
    viewMenu->addAction(i18n("&Fit Canvas"), this, [this]() { m_canvas->resetView(); });

    QToolBar *tb = addToolBar(i18n("Krita Tools"));
    tb->setMovable(false);
    tb->addAction(i18n("New"), this, &KritaWasmMainWindow::onNewDocumentDialog);
    tb->addAction(i18n("Open"), this, &KritaWasmMainWindow::onTriggerBrowserOpen);
    tb->addAction(i18n("Save .kra"), this, &KritaWasmMainWindow::onExportKra);
    tb->addAction(i18n("Export PNG"), this, &KritaWasmMainWindow::onExportPng);
    tb->addSeparator();
    tb->addAction(undoAct);
    tb->addAction(redoAct);
    tb->addSeparator();

    QAction *brushAct = tb->addAction(i18n("Brush (B)"), this, [this]() { m_canvas->setEraserMode(false); });
    brushAct->setCheckable(true);
    brushAct->setChecked(true);
    QAction *eraserAct = tb->addAction(i18n("Eraser (E)"), this, [this]() { m_canvas->setEraserMode(true); });
    eraserAct->setCheckable(true);
    connect(brushAct, &QAction::triggered, this, [eraserAct]() { eraserAct->setChecked(false); });
    connect(eraserAct, &QAction::triggered, this, [brushAct]() { brushAct->setChecked(false); });

    tb->addSeparator();
    tb->addWidget(new QLabel(QStringLiteral(" Size: "), this));
    QSlider *sizeSlider = new QSlider(Qt::Horizontal, this);
    sizeSlider->setRange(2, 160);
    sizeSlider->setValue(28);
    sizeSlider->setFixedWidth(110);
    connect(sizeSlider, &QSlider::valueChanged, this, [this](int v) { m_canvas->setBrushSize(float(v)); });
    tb->addWidget(sizeSlider);

    tb->addWidget(new QLabel(QStringLiteral(" Opacity: "), this));
    QSlider *opSlider = new QSlider(Qt::Horizontal, this);
    opSlider->setRange(5, 100);
    opSlider->setValue(85);
    opSlider->setFixedWidth(90);
    connect(opSlider, &QSlider::valueChanged, this, [this](int v) { m_canvas->setBrushOpacity(float(v) / 100.0f); });
    tb->addWidget(opSlider);

    tb->addSeparator();
    QPushButton *colorBtn = new QPushButton(i18n("Color"), this);
    connect(colorBtn, &QPushButton::clicked, this, [this, colorBtn]() {
        const QColor chosen = QColorDialog::getColor(Qt::cyan, this, i18n("Select Brush Color"));
        if (chosen.isValid()) {
            m_canvas->setBrushColor(chosen);
            colorBtn->setStyleSheet(QStringLiteral("background:%1;color:%2;").arg(chosen.name(), chosen.lightness() > 128 ? "#111" : "#fff"));
        }
    });
    tb->addWidget(colorBtn);
}

void KritaWasmMainWindow::setupDocks()
{
    QDockWidget *layersDock = new QDockWidget(i18n("Layers"), this);
    QWidget *layerPanel = new QWidget(layersDock);
    QVBoxLayout *lv = new QVBoxLayout(layerPanel);

    QHBoxLayout *blendRow = new QHBoxLayout();
    m_blendCombo = new QComboBox(layerPanel);
    m_blendCombo->addItem(i18n("Normal"), COMPOSITE_OVER);
    m_blendCombo->addItem(i18n("Multiply"), COMPOSITE_MULT);
    m_blendCombo->addItem(i18n("Screen"), COMPOSITE_SCREEN);
    m_blendCombo->addItem(i18n("Overlay"), COMPOSITE_OVERLAY);
    m_blendCombo->addItem(i18n("Addition"), COMPOSITE_ADD);
    m_blendCombo->addItem(i18n("Darken"), COMPOSITE_DARKEN);
    m_blendCombo->addItem(i18n("Lighten"), COMPOSITE_LIGHTEN);
    connect(m_blendCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &KritaWasmMainWindow::onBlendModeChanged);
    blendRow->addWidget(m_blendCombo, 1);

    m_layerOpacitySlider = new QSlider(Qt::Horizontal, layerPanel);
    m_layerOpacitySlider->setRange(0, 255);
    m_layerOpacitySlider->setValue(255);
    connect(m_layerOpacitySlider, &QSlider::valueChanged, this, &KritaWasmMainWindow::onLayerOpacityChanged);
    blendRow->addWidget(m_layerOpacitySlider, 1);
    lv->addLayout(blendRow);

    m_layerList = new QListWidget(layerPanel);
    connect(m_layerList, &QListWidget::currentRowChanged, this, &KritaWasmMainWindow::onLayerSelectionChanged);
    connect(m_layerList, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        const int row = m_layerList->row(item);
        const int idx = static_cast<int>(m_canvas->layers().size()) - 1 - row;
        m_canvas->setLayerVisible(idx, item->checkState() == Qt::Checked);
    });
    lv->addWidget(m_layerList, 1);

    QHBoxLayout *btnRow = new QHBoxLayout();
    QPushButton *addBtn = new QPushButton(i18n("+ Layer"), layerPanel);
    QPushButton *delBtn = new QPushButton(i18n("- Remove"), layerPanel);
    connect(addBtn, &QPushButton::clicked, this, [this]() { m_canvas->addLayer(); });
    connect(delBtn, &QPushButton::clicked, this, [this]() { m_canvas->removeActiveLayer(); });
    btnRow->addWidget(addBtn);
    btnRow->addWidget(delBtn);
    lv->addLayout(btnRow);

    m_colorSpaceCombo = new QComboBox(layerPanel);
    m_colorSpaceCombo->addItem(QStringLiteral("RGBA 8-bit (sRGB)"), QStringList{RGBAColorModelID.id(), Integer8BitsColorDepthID.id()});
    m_colorSpaceCombo->addItem(QStringLiteral("RGBA 16-bit (LCMS2)"), QStringList{RGBAColorModelID.id(), Integer16BitsColorDepthID.id()});
    m_colorSpaceCombo->addItem(QStringLiteral("CIE L*a*b* 16-bit"), QStringList{LABAColorModelID.id(), Integer16BitsColorDepthID.id()});
    connect(m_colorSpaceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &KritaWasmMainWindow::onColorSpaceChanged);
    lv->addWidget(new QLabel(i18n("Pigment Color Space:"), layerPanel));
    lv->addWidget(m_colorSpaceCombo);

    layersDock->setWidget(layerPanel);
    addDockWidget(Qt::RightDockWidgetArea, layersDock);

    QDockWidget *undoDock = new QDockWidget(i18n("Undo History"), this);
    KUndo2View *undoView = new KUndo2View(m_canvas->undoStack(), undoDock);
    undoDock->setWidget(undoView);
    addDockWidget(Qt::RightDockWidgetArea, undoDock);
}

void KritaWasmMainWindow::syncUiFromCanvas()
{
    m_layerList->blockSignals(true);
    m_layerList->clear();
    const auto &layers = m_canvas->layers();
    for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i) {
        const KritaWasmLayer &l = layers[static_cast<size_t>(i)];
        QListWidgetItem *item = new QListWidgetItem(l.name, m_layerList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(l.visible ? Qt::Checked : Qt::Unchecked);
        if (i == m_canvas->activeLayerIndex()) {
            m_layerList->setCurrentItem(item);
        }
    }
    m_layerList->blockSignals(false);

    if (m_canvas->activeLayerIndex() >= 0 && m_canvas->activeLayerIndex() < static_cast<int>(layers.size())) {
        const KritaWasmLayer &active = layers[static_cast<size_t>(m_canvas->activeLayerIndex())];
        m_layerOpacitySlider->blockSignals(true);
        m_layerOpacitySlider->setValue(active.opacity);
        m_layerOpacitySlider->blockSignals(false);

        const int blendIdx = m_blendCombo->findData(active.compositeOpId);
        if (blendIdx >= 0) {
            m_blendCombo->blockSignals(true);
            m_blendCombo->setCurrentIndex(blendIdx);
            m_blendCombo->blockSignals(false);
        }
    }

    const KoColorSpace *cs = m_canvas->colorSpace();
    m_statusLabel->setText(QStringLiteral("%1 x %2 px  |  %3 (%4)  |  Undo steps: %5  |  WebGL: %6")
        .arg(m_canvas->docWidth())
        .arg(m_canvas->docHeight())
        .arg(cs ? cs->name() : QStringLiteral("RGBA"))
        .arg(cs && cs->profile() ? cs->profile()->name() : QStringLiteral("sRGB"))
        .arg(m_canvas->undoStack()->count())
        .arg(m_canvas->isWebGLReady() ? m_canvas->glRendererInfo() : QStringLiteral("Software Fallback")));

    notifyBrowserHost(QStringLiteral("state"), engineStatusJson());
}

QString KritaWasmMainWindow::engineStatusJson() const
{
    const KoColorSpace *cs = m_canvas->colorSpace();
    QJsonObject obj;
    obj[QStringLiteral("kritaVersion")] = KritaVersionWrapper::versionString();
    obj[QStringLiteral("arch")] = KisSupportedArchitectures::baseArchName();
    obj[QStringLiteral("width")] = m_canvas->docWidth();
    obj[QStringLiteral("height")] = m_canvas->docHeight();
    obj[QStringLiteral("colorSpace")] = cs ? cs->id() : QStringLiteral("RGBA");
    obj[QStringLiteral("colorSpaceName")] = cs ? cs->name() : QStringLiteral("RGBA");
    obj[QStringLiteral("profileName")] = (cs && cs->profile()) ? cs->profile()->name() : QStringLiteral("sRGB");
    obj[QStringLiteral("layerCount")] = static_cast<int>(m_canvas->layers().size());
    obj[QStringLiteral("undoCount")] = m_canvas->undoStack()->count();
    obj[QStringLiteral("webgl")] = m_canvas->isWebGLReady();
    obj[QStringLiteral("glRenderer")] = m_canvas->glRendererInfo();
    return QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

void KritaWasmMainWindow::onNewDocumentDialog()
{
    bool ok = false;
    const int w = QInputDialog::getInt(this, i18n("New Krita Document"), i18n("Canvas Width (px):"), 1280, 64, 4096, 64, &ok);
    if (!ok) return;
    const int h = QInputDialog::getInt(this, i18n("New Krita Document"), i18n("Canvas Height (px):"), 800, 64, 4096, 64, &ok);
    if (!ok) return;
    m_canvas->newDocument(w, h, RGBAColorModelID.id(), Integer8BitsColorDepthID.id());
}

void KritaWasmMainWindow::onTriggerBrowserOpen()
{
    notifyBrowserHost(QStringLiteral("request_open_file"));
}

void KritaWasmMainWindow::onExportKra()
{
    const QString outPath = QStringLiteral("/tmp/export.kra");
    if (m_canvas->saveToKra(outPath)) {
        notifyBrowserHost(QStringLiteral("download_file"), QStringLiteral("{\"path\":\"/tmp/export.kra\",\"filename\":\"artwork.kra\"}"));
    }
}

void KritaWasmMainWindow::onExportPng()
{
    const QString outPath = QStringLiteral("/tmp/export.png");
    if (m_canvas->exportToPng(outPath)) {
        notifyBrowserHost(QStringLiteral("download_file"), QStringLiteral("{\"path\":\"/tmp/export.png\",\"filename\":\"artwork.png\"}"));
    }
}

void KritaWasmMainWindow::onSaveToBrowserStorage()
{
    QDir().mkpath(QStringLiteral("/krita-projects"));
    const QString savePath = QStringLiteral("/krita-projects/autosave.kra");
    if (m_canvas->saveToKra(savePath)) {
        notifyBrowserHost(QStringLiteral("sync_idbfs"), QStringLiteral("{\"path\":\"/krita-projects/autosave.kra\"}"));
        statusBar()->showMessage(i18n("Saved .kra project to browser IndexedDB storage."), 3000);
    }
}

void KritaWasmMainWindow::onLayerSelectionChanged(int row)
{
    if (row < 0) return;
    const int idx = static_cast<int>(m_canvas->layers().size()) - 1 - row;
    m_canvas->setActiveLayer(idx);
}

void KritaWasmMainWindow::onBlendModeChanged(int index)
{
    if (index < 0) return;
    m_canvas->setLayerBlendMode(m_canvas->activeLayerIndex(), m_blendCombo->itemData(index).toString());
}

void KritaWasmMainWindow::onLayerOpacityChanged(int value)
{
    m_canvas->setLayerOpacity(m_canvas->activeLayerIndex(), static_cast<quint8>(qBound(0, value, 255)));
}

void KritaWasmMainWindow::onColorSpaceChanged(int index)
{
    if (index < 0) return;
    const QStringList pair = m_colorSpaceCombo->itemData(index).toStringList();
    if (pair.size() == 2) {
        m_canvas->newDocument(m_canvas->docWidth(), m_canvas->docHeight(), pair[0], pair[1]);
    }
}

#ifdef __EMSCRIPTEN__
extern "C" {

EMSCRIPTEN_KEEPALIVE
int krita_wasm_open_file(const char *vfsPath)
{
    if (!g_mainWindow || !vfsPath) return 0;
    const QString path = QString::fromUtf8(vfsPath);
    if (path.endsWith(QStringLiteral(".kra"), Qt::CaseInsensitive)) {
        return g_mainWindow->canvas()->loadFromKra(path) ? 1 : 0;
    }
    return g_mainWindow->canvas()->loadFromRasterImage(path) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int krita_wasm_export_kra(const char *vfsPath)
{
    if (!g_mainWindow || !vfsPath) return 0;
    return g_mainWindow->canvas()->saveToKra(QString::fromUtf8(vfsPath)) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int krita_wasm_export_png(const char *vfsPath)
{
    if (!g_mainWindow || !vfsPath) return 0;
    return g_mainWindow->canvas()->exportToPng(QString::fromUtf8(vfsPath)) ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
void krita_wasm_new_document(int width, int height)
{
    if (!g_mainWindow) return;
    g_mainWindow->canvas()->newDocument(width, height, RGBAColorModelID.id(), Integer8BitsColorDepthID.id());
}

EMSCRIPTEN_KEEPALIVE
const char *krita_wasm_get_engine_info()
{
    static QByteArray s_buf;
    if (!g_mainWindow) return "{}";
    s_buf = g_mainWindow->engineStatusJson().toUtf8();
    return s_buf.constData();
}

EMSCRIPTEN_KEEPALIVE
void krita_wasm_undo()
{
    if (g_mainWindow && g_mainWindow->canvas()->undoStack()->canUndo()) {
        g_mainWindow->canvas()->undoStack()->undo();
    }
}

EMSCRIPTEN_KEEPALIVE
void krita_wasm_redo()
{
    if (g_mainWindow && g_mainWindow->canvas()->undoStack()->canRedo()) {
        g_mainWindow->canvas()->undoStack()->redo();
    }
}

} // extern "C"
#endif
