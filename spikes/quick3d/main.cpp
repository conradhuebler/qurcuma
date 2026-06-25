// Copyright (C) 2025 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
//
// WP0 Qt Quick 3D + Vulkan spike - entry point and QWidget shell.
//   T1  QApplication + Quick3D view
//   T5  Vulkan RHI (default) with --gl OpenGL fallback; backend logged & shown
//   T6  both embedding routes: --embed=quickwidget (default) | container
//   T8  FPS HUD via QQuickWindow::frameSwapped + QElapsedTimer
// Run with QSG_INFO=1 to see the RHI backend confirmation in the log.
// Claude Generated.
#include "scenecontroller.h"

#include <QApplication>
#include <QCheckBox>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QQmlContext>
#include <QQuickView>
#include <QQuickWidget>
#include <QQuickWindow>
#include <QRadioButton>
#include <QSGRendererInterface>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace {

const QUrl kQmlUrl(QStringLiteral("qrc:/qt/qml/QurcumaSpike/Main.qml"));

QString backendToString(QSGRendererInterface::GraphicsApi api)
{
    switch (api) {
    case QSGRendererInterface::Vulkan: return QStringLiteral("Vulkan");
    case QSGRendererInterface::OpenGL: return QStringLiteral("OpenGL");
    case QSGRendererInterface::Metal: return QStringLiteral("Metal");
    case QSGRendererInterface::Direct3D11: return QStringLiteral("D3D11");
    case QSGRendererInterface::Direct3D12: return QStringLiteral("D3D12");
    default: return QStringLiteral("Software/Unknown");
    }
}

/// FPS counter driven by the window's frameSwapped signal (T8). Updates the
/// controller (-> in-scene HUD) and a side-panel label twice per second.
class FpsMeter : public QObject
{
public:
    FpsMeter(QQuickWindow* window, SceneController* controller, QLabel* label)
        : QObject(window)
        , m_controller(controller)
        , m_label(label)
    {
        m_clock.start();
        connect(window, &QQuickWindow::frameSwapped, this, [this] { ++m_frames; });
        auto* tick = new QTimer(this);
        tick->setInterval(500);
        connect(tick, &QTimer::timeout, this, [this] {
            const double secs = m_clock.restart() / 1000.0;
            const double fps = secs > 0.0 ? m_frames / secs : 0.0;
            m_frames = 0;
            m_controller->setFps(fps);
            if (m_label)
                m_label->setText(QStringLiteral("FPS: %1").arg(fps, 0, 'f', 1));
        });
        tick->start();
    }

private:
    SceneController* m_controller;
    QLabel* m_label;
    QElapsedTimer m_clock;
    int m_frames = 0;
};

/// Build the left-hand control panel (datasets, effect toggles, animation, FPS).
QWidget* buildSidePanel(SceneController* controller, QLabel*& fpsLabelOut)
{
    auto* panel = new QWidget;
    panel->setMaximumWidth(240);
    auto* col = new QVBoxLayout(panel);

    // Dataset selection (T8 stress sizes).
    auto* dsBox = new QGroupBox(QStringLiteral("Dataset"));
    auto* dsCol = new QVBoxLayout(dsBox);
    struct { const char* text; int n; bool checked; } sets[] = {
        { "1k atoms", 1000, true }, { "5k atoms", 5000, false }, { "10k atoms", 10000, false }
    };
    for (const auto& s : sets) {
        auto* rb = new QRadioButton(QString::fromLatin1(s.text));
        rb->setChecked(s.checked);
        const int n = s.n;
        QObject::connect(rb, &QRadioButton::toggled, controller, [controller, n](bool on) {
            if (on)
                controller->loadDataset(n);
        });
        dsCol->addWidget(rb);
    }
    auto* loadBtn = new QPushButton(QStringLiteral("Load XYZ…"));
    QObject::connect(loadBtn, &QPushButton::clicked, panel, [panel, controller] {
        const QString f = QFileDialog::getOpenFileName(panel,
            QStringLiteral("Open XYZ"), QString(), QStringLiteral("XYZ (*.xyz);;All files (*)"));
        if (!f.isEmpty())
            controller->loadFile(f);
    });
    dsCol->addWidget(loadBtn);
    col->addWidget(dsBox);

    // Effect toggles (T4).
    auto* fxBox = new QGroupBox(QStringLiteral("Effects"));
    auto* fxCol = new QVBoxLayout(fxBox);
    auto addToggle = [&](const QString& text, bool initial, auto setter) {
        auto* cb = new QCheckBox(text);
        cb->setChecked(initial);
        QObject::connect(cb, &QCheckBox::toggled, controller, setter);
        fxCol->addWidget(cb);
    };
    addToggle(QStringLiteral("SSAO"), controller->ssaoEnabled(), &SceneController::setSsaoEnabled);
    addToggle(QStringLiteral("Shadows"), controller->shadowsEnabled(), &SceneController::setShadowsEnabled);
    addToggle(QStringLiteral("Bloom"), controller->bloomEnabled(), &SceneController::setBloomEnabled);
    addToggle(QStringLiteral("Tonemap"), controller->tonemapEnabled(), &SceneController::setTonemapEnabled);
    addToggle(QStringLiteral("Bonds"), controller->bondsVisible(), &SceneController::setBondsVisible);
    col->addWidget(fxBox);

    // Animation (MD-proxy, K2).
    auto* animCb = new QCheckBox(QStringLiteral("Animate (MD proxy)"));
    QObject::connect(animCb, &QCheckBox::toggled, controller, &SceneController::setAnimating);
    col->addWidget(animCb);

    // Live info.
    fpsLabelOut = new QLabel(QStringLiteral("FPS: —"));
    fpsLabelOut->setStyleSheet(QStringLiteral("font-weight:bold;"));
    col->addWidget(fpsLabelOut);
    auto* infoLabel = new QLabel;
    auto refreshInfo = [infoLabel, controller] {
        infoLabel->setText(QStringLiteral("Backend: %1\nEmbed: %2\nAtoms: %3  Bonds: %4")
                               .arg(controller->backendName(), controller->embedMode())
                               .arg(controller->atomCount())
                               .arg(controller->bondCount()));
    };
    QObject::connect(controller, &SceneController::infoChanged, infoLabel, refreshInfo);
    QObject::connect(controller, &SceneController::structureChanged, infoLabel, refreshInfo);
    refreshInfo();
    col->addWidget(infoLabel);

    auto* selLabel = new QLabel(QStringLiteral("Picked: —"));
    selLabel->setWordWrap(true);
    QObject::connect(controller, &SceneController::selectionChanged, selLabel, [selLabel, controller] {
        selLabel->setText(QStringLiteral("Picked: %1").arg(controller->selectionLabel()));
    });
    col->addWidget(selLabel);

    col->addStretch();
    return panel;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("qurcuma-quick3d-spike"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("WP0 Qt Quick 3D + Vulkan spike"));
    parser.addHelpOption();
    QCommandLineOption embedOpt(QStringLiteral("embed"),
        QStringLiteral("Embedding route: quickwidget (default) | container"),
        QStringLiteral("mode"), QStringLiteral("quickwidget"));
    QCommandLineOption glOpt(QStringLiteral("gl"),
        QStringLiteral("Use the OpenGL RHI backend instead of Vulkan (fallback/comparison)."));
    parser.addOption(embedOpt);
    parser.addOption(glOpt);
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Optional .xyz to load"));
    parser.process(app);

    // T5 - force the RHI backend before any Quick window is created. QSG_RHI_BACKEND
    // env var also works and overrides this if set.
    const bool useGl = parser.isSet(glOpt);
    QQuickWindow::setGraphicsApi(useGl ? QSGRendererInterface::OpenGL
                                       : QSGRendererInterface::Vulkan);

    const bool useContainer = parser.value(embedOpt).compare(
                                  QStringLiteral("container"), Qt::CaseInsensitive) == 0;

    auto* controller = new SceneController(&app);
    controller->setEmbedMode(useContainer ? QStringLiteral("WindowContainer")
                                           : QStringLiteral("QQuickWidget"));

    // --- Shell window: side panel + 3D surface ---
    auto* shell = new QWidget;
    shell->setWindowTitle(QStringLiteral("qurcuma · Qt Quick 3D + Vulkan spike (WP0)"));
    shell->resize(1280, 800);
    auto* row = new QHBoxLayout(shell);
    row->setContentsMargins(0, 0, 0, 0);
    QLabel* fpsLabel = nullptr;
    row->addWidget(buildSidePanel(controller, fpsLabel));

    QQuickWindow* quickWindow = nullptr; // for FPS + backend confirmation

    // T6 - two embedding routes loading the *same* Main.qml.
    if (useContainer) {
        // Variant B: native QQuickView in a window container (qurcuma's current pattern).
        auto* view = new QQuickView;
        view->setResizeMode(QQuickView::SizeRootObjectToView);
        view->rootContext()->setContextProperty(QStringLiteral("controller"), controller);
        view->setSource(kQmlUrl);
        quickWindow = view;
        auto* container = QWidget::createWindowContainer(view, shell);
        container->setMinimumSize(640, 480);
        container->setFocusPolicy(Qt::StrongFocus);
        row->addWidget(container, 1);
    } else {
        // Variant A: QQuickWidget (real widget, overlays possible).
        auto* widget = new QQuickWidget(shell);
        widget->setResizeMode(QQuickWidget::SizeRootObjectToView);
        widget->rootContext()->setContextProperty(QStringLiteral("controller"), controller);
        widget->setSource(kQmlUrl);
        quickWindow = widget->quickWindow();
        row->addWidget(widget, 1);
    }

    shell->show();

    // Confirm the actual backend once the scene graph is initialised (T5/K6) and
    // start the FPS meter (T8).
    if (quickWindow) {
        QObject::connect(quickWindow, &QQuickWindow::sceneGraphInitialized, controller,
            [quickWindow, controller] {
                const auto api = quickWindow->rendererInterface()->graphicsApi();
                const QString name = backendToString(api);
                controller->setBackendName(name);
                qInfo("Quick3D spike: RHI backend = %s", qPrintable(name));
            },
            Qt::QueuedConnection);
        // Fall back to the requested backend name immediately in case the signal
        // already fired before we connected.
        controller->setBackendName(useGl ? QStringLiteral("OpenGL (requested)")
                                         : QStringLiteral("Vulkan (requested)"));
        new FpsMeter(quickWindow, controller, fpsLabel);
    }

    // Optional positional file argument.
    const QStringList pos = parser.positionalArguments();
    if (!pos.isEmpty())
        controller->loadFile(pos.first());

    return app.exec();
}
