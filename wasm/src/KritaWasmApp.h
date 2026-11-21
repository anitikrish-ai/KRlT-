#ifndef KRITA_WASM_APP_H
#define KRITA_WASM_APP_H

#include <QMainWindow>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QImage>
#include <QColor>
#include <QListWidget>
#include <QSlider>
#include <QComboBox>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QPointF>
#include <vector>
#include <memory>

#include <kundo2stack.h>
#include <KoColorSpace.h>
#include <KoColor.h>

struct KritaWasmLayer {
    QString name;
    bool visible {true};
    quint8 opacity {255};
    QString compositeOpId {QStringLiteral("normal")};
    QByteArray rawPixels; // Stored in active KoColorSpace pixel format!
};

class KritaCanvasWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit KritaCanvasWidget(QWidget *parent = nullptr);
    ~KritaCanvasWidget() override;

    void newDocument(int width, int height, const QString &colorModelId, const QString &colorDepthId);
    bool loadFromKra(const QString &vfsPath);
    bool saveToKra(const QString &vfsPath);
    bool loadFromRasterImage(const QString &vfsPath);
    bool exportToPng(const QString &vfsPath);

    void addLayer(const QString &name = QString());
    void removeActiveLayer();
    void setActiveLayer(int index);
    void setLayerVisible(int index, bool visible);
    void setLayerOpacity(int index, quint8 opacity);
    void setLayerBlendMode(int index, const QString &compositeOpId);

    void setBrushSize(float size) { m_brushSize = qBound(1.0f, size, 300.0f); }
    void setBrushOpacity(float opacity) { m_brushOpacity = qBound(0.01f, opacity, 1.0f); }
    void setBrushHardness(float hardness) { m_brushHardness = qBound(0.0f, hardness, 1.0f); }
    void setBrushColor(const QColor &c);
    void setEraserMode(bool eraser) { m_eraserMode = eraser; }
    void setZoom(double zoom);
    void resetView();

    int docWidth() const { return m_width; }
    int docHeight() const { return m_height; }
    int activeLayerIndex() const { return m_activeLayer; }
    const std::vector<KritaWasmLayer> &layers() const { return m_layers; }
    const KoColorSpace *colorSpace() const { return m_colorSpace; }
    KUndo2Stack *undoStack() const { return m_undoStack; }
    bool isWebGLReady() const { return m_glInitialized; }
    QString glRendererInfo() const { return m_glInfo; }
    double zoomFactor() const { return m_zoom; }

    void recomposeDocument();
    void applyLayerPatch(int layerIndex, const QRect &rect, const QByteArray &pixels);

signals:
    void documentChanged();
    void layersChanged();
    void cursorMoved(int x, int y, float pressure, float tiltX, float tiltY);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void tabletEvent(QTabletEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;

private:
    QPointF widgetToCanvas(const QPointF &pos) const;
    void beginStroke(const QPointF &canvasPos, float pressure, float tiltX, float tiltY);
    void continueStroke(const QPointF &canvasPos, float pressure, float tiltX, float tiltY);
    void endStroke();
    void stampDab(const QPointF &canvasPos, float pressure, float tiltX, float tiltY);
    void uploadCompositeTexture();

    int m_width {1920};
    int m_height {1080};
    const KoColorSpace *m_colorSpace {nullptr};
    std::vector<KritaWasmLayer> m_layers;
    int m_activeLayer {0};
    QImage m_compositedSrgb;
    QByteArray m_compositedRaw;

    KUndo2Stack *m_undoStack {nullptr};

    // Brush state
    float m_brushSize {28.0f};
    float m_brushOpacity {0.85f};
    float m_brushHardness {0.75f};
    QColor m_brushColor {QColor(34, 150, 243)};
    bool m_eraserMode {false};

    // Stroke state + undo snapshot
    bool m_isDrawing {false};
    bool m_isPanning {false};
    bool m_spaceDown {false};
    QPointF m_lastCanvasPos;
    QPointF m_lastPanPos;
    QByteArray m_strokeBeforeRaw;
    QRect m_strokeDirtyRect;

    // Viewport transform
    double m_zoom {0.65};
    QPointF m_panOffset {0.0, 0.0};
    bool m_hasUserViewTransform {false};

    // WebGL 2 state
    bool m_glInitialized {false};
    bool m_textureDirty {true};
    GLuint m_canvasTex {0};
    QString m_glInfo;
};

class KritaWasmMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit KritaWasmMainWindow(QWidget *parent = nullptr);
    ~KritaWasmMainWindow() override = default;

    KritaCanvasWidget *canvas() const { return m_canvas; }
    static KritaWasmMainWindow *instance();

    void syncUiFromCanvas();
    QString engineStatusJson() const;

private slots:
    void onNewDocumentDialog();
    void onTriggerBrowserOpen();
    void onExportKra();
    void onExportPng();
    void onSaveToBrowserStorage();
    void onLayerSelectionChanged(int row);
    void onBlendModeChanged(int index);
    void onLayerOpacityChanged(int value);
    void onColorSpaceChanged(int index);

private:
    void setupMenusAndToolbars();
    void setupDocks();

    KritaCanvasWidget *m_canvas {nullptr};
    QListWidget *m_layerList {nullptr};
    QComboBox *m_blendCombo {nullptr};
    QSlider *m_layerOpacitySlider {nullptr};
    QComboBox *m_colorSpaceCombo {nullptr};
    QLabel *m_statusLabel {nullptr};
    QLabel *m_colorSwatch {nullptr};
};

#endif // KRITA_WASM_APP_H
