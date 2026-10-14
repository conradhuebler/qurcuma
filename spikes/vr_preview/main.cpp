/*
 * <qurcuma VR preview: renders the headset scene on the desktop or offscreen.>
 * Copyright (C) 2015 - 2026 Conrad Hübler <Conrad.Huebler@gmx.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// Claude Generated 2026 - Loads Preview.qml (VrScene.qml driven by MockVr/MockMolecule, seen from a
// head at the origin) into a QQuickView. With --shot it renders a PNG and quits; otherwise the window
// stays interactive (left click = panels, right drag = look around, wheel = field of view).

#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QQuickView>
#include <QQuickWindow>
#include <QSurfaceFormat>
#include <QTimer>

#include <cstdio>

namespace {

bool runsOffscreen(int argc, char** argv)
{
    if (qgetenv("QT_QPA_PLATFORM").startsWith("offscreen"))
        return true;
    for (int i = 1; i + 1 < argc; ++i)
        if (qstrcmp(argv[i], "-platform") == 0 && qstrncmp(argv[i + 1], "offscreen", 9) == 0)
            return true;
    return false;
}

/// Synthesised left click (move, press, release) at a window position, like XrVirtualMouse does.
void clickAt(QQuickWindow* window, const QPointF& pos)
{
    const QPointF global = window->mapToGlobal(pos);
    QMouseEvent move(QEvent::MouseMove, pos, global, Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &move);
    QMouseEvent press(QEvent::MouseButtonPress, pos, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &press);
    QMouseEvent release(QEvent::MouseButtonRelease, pos, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(window, &release);
}

} // namespace

int main(int argc, char** argv)
{
    if (runsOffscreen(argc, argv)) {
        // The offscreen platform offers Qt Quick 3D only OpenGL, and its default context (GL 2.1)
        // lacks the GLSL versions baked into Quick 3D's shaders; a core profile has them.
        QSurfaceFormat format;
        format.setVersion(4, 3);
        format.setProfile(QSurfaceFormat::CoreProfile);
        format.setDepthBufferSize(24);
        format.setStencilBufferSize(8);
        QSurfaceFormat::setDefaultFormat(format);
        // The platform reports no OpenGL capability, so Qt Quick would fall back to its software
        // renderer, under which View3D draws nothing.
        if (qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND"))
            qputenv("QT_QUICK_BACKEND", "rhi");
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    }
    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Offline preview of the qurcuma headset scene");
    parser.addHelpOption();
    QCommandLineOption stateOpt("state", "Mock state: idle, trajectory, running, paused, aborted, files, help, hidden.", "name", "idle");
    QCommandLineOption shotOpt("shot", "Render a PNG to <file> and quit.", "file");
    QCommandLineOption sizeOpt("size", "Window size in pixels.", "WxH", "1600x1000");
    QCommandLineOption fovOpt("fov", "Horizontal field of view in degrees.", "deg", "100");
    QCommandLineOption yawOpt("yaw", "Head yaw in degrees (positive = turn left).", "deg", "0");
    QCommandLineOption pitchOpt("pitch", "Head pitch in degrees (positive = look up).", "deg", "-12");
    QCommandLineOption clickOpt("click", "Left click at window position x,y before the shot (repeatable).", "x,y");
    QCommandLineOption qmlOpt("qml", "QML file to load instead of Preview.qml.", "file");
    parser.addOptions({ stateOpt, shotOpt, sizeOpt, fovOpt, yawOpt, pitchOpt, clickOpt, qmlOpt });
    parser.process(app);

    const QStringList size = parser.value(sizeOpt).split('x');
    const int width = size.value(0).toInt() > 0 ? size.value(0).toInt() : 1600;
    const int height = size.value(1).toInt() > 0 ? size.value(1).toInt() : 1000;
    const QString qmlFile = parser.isSet(qmlOpt)
        ? QFileInfo(parser.value(qmlOpt)).absoluteFilePath()
        : QDir(QStringLiteral(VR_PREVIEW_DIR)).filePath("Preview.qml");

    QQuickView view;
    view.setTitle(QStringLiteral("qurcuma VR preview"));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.setInitialProperties({ { "stateName", parser.value(stateOpt) },
        { "fieldOfView", parser.value(fovOpt).toDouble() },
        { "lookYaw", parser.value(yawOpt).toDouble() },
        { "lookPitch", parser.value(pitchOpt).toDouble() } });
    view.setSource(QUrl::fromLocalFile(qmlFile));
    if (view.status() != QQuickView::Ready) {
        for (const QQmlError& e : view.errors())
            std::fprintf(stderr, "%s\n", qPrintable(e.toString()));
        return 1;
    }
    view.resize(width, height);
    view.show();

    const QString shot = parser.value(shotOpt);
    const QStringList clicks = parser.values(clickOpt);
    if (!shot.isEmpty()) {
        // Give the scene a few frames to settle, then deliver the clicks 400 ms apart (a panel that a
        // click opens is pickable only after the next frame), then grab.
        constexpr int settleMs = 600;
        constexpr int stepMs = 400;
        for (int i = 0; i < clicks.size(); ++i) {
            const QStringList xy = clicks[i].split(',');
            if (xy.size() == 2) {
                const QPointF pos(xy[0].toDouble(), xy[1].toDouble());
                QTimer::singleShot(settleMs + i * stepMs, &view, [&view, pos] { clickAt(&view, pos); });
            }
        }
        QTimer::singleShot(settleMs + int(clicks.size()) * stepMs, &view, [&view, shot] {
            const QImage image = view.grabWindow();
            const bool ok = !image.isNull() && image.save(shot);
            std::fprintf(stderr, "%s %s (%dx%d)\n", ok ? "wrote" : "FAILED", qPrintable(shot),
                image.width(), image.height());
            QCoreApplication::exit(ok ? 0 : 2);
        });
    }
    return app.exec();
}
