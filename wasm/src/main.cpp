#include <QApplication>
#include <QDir>
#include <QPalette>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <KoColorSpaceRegistry.h>
#include "KritaWasmApp.h"

static void applyKritaDarkPalette(QApplication &app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette pal;
    pal.setColor(QPalette::Window, QColor(35, 38, 41));
    pal.setColor(QPalette::WindowText, QColor(239, 240, 241));
    pal.setColor(QPalette::Base, QColor(27, 30, 32));
    pal.setColor(QPalette::AlternateBase, QColor(49, 54, 59));
    pal.setColor(QPalette::ToolTipBase, QColor(49, 54, 59));
    pal.setColor(QPalette::ToolTipText, QColor(239, 240, 241));
    pal.setColor(QPalette::Text, QColor(239, 240, 241));
    pal.setColor(QPalette::Button, QColor(49, 54, 59));
    pal.setColor(QPalette::ButtonText, QColor(239, 240, 241));
    pal.setColor(QPalette::BrightText, Qt::white);
    pal.setColor(QPalette::Highlight, QColor(61, 174, 233));
    pal.setColor(QPalette::HighlightedText, Qt::white);
    app.setPalette(pal);
}

int main(int argc, char *argv[])
{
    QSurfaceFormat fmt;
    fmt.setRenderableType(QSurfaceFormat::OpenGLES);
    fmt.setVersion(3, 0);
    fmt.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Krita WebAssembly"));
    app.setOrganizationName(QStringLiteral("KDE"));
    applyKritaDarkPalette(app);

    QDir().mkpath(QStringLiteral("/krita-data/config"));
    QDir().mkpath(QStringLiteral("/krita-projects"));
    QDir().mkpath(QStringLiteral("/tmp"));

    // Initialize Krita's global KoColorSpaceRegistry + LCMS2 ICC color spaces
    KoColorSpaceRegistry::instance();

    KritaWasmMainWindow *mainWin = new KritaWasmMainWindow();
    mainWin->showMaximized();

    // Auto-restore browser-saved project if present in IDBFS
    if (QFile::exists(QStringLiteral("/krita-projects/autosave.kra"))) {
        mainWin->canvas()->loadFromKra(QStringLiteral("/krita-projects/autosave.kra"));
    }

    return app.exec();
}
