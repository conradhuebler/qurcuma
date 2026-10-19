// Instance-table mirror test - Claude Generated 2026
//
// The headset scene (viewerxr.qml) draws the desktop SceneController's instance tables through
// MirrorInstancing objects (src/tableinstancing.h, src/xrscenebridge.h). This pins:
//   1. a mirror reports the source's count and bytes, also after a rebuild, without copying;
//   2. XrSceneBridge has one mirror for every QQuick3DInstancing* property of SceneController,
//      under the same name (MoleculeModels.qml reads both through one set of names);
//   3. the atom table skips hidden atoms, and atomIndexForInstance maps a row back to its atom;
//   4. rendered in a second window, a mirror re-renders after a source rebuild, and does NOT when
//      the rebuild is not announced (tableChanged blocked): QQuick3DInstancing fetches a table only
//      after markDirty(), so the mirror's markDirty on tableChanged is what keeps the headset current;
//   5. src/qml/MoleculeModels.qml, fed by an XrSceneBridge under a rotated parent, shows an overlay
//      that is switched on after the first frame, turned with its parent (see ToggledModel there).
// Parts 4 and 5 need Qt Quick 3D on OpenGL (offscreen platform); they are skipped if nothing renders.
// QURCUMA_QML_DIR (environment) overrides the QML directory part 5 loads.

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QMetaProperty>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickView>
#include <QQuickWindow>
#include <QRect>
#include <QSurfaceFormat>
#include <QTemporaryDir>
#include <QThread>
#include <QVector>

#include <iostream>

#include "atominstancing.h"
#include "scenecontroller.h"
#include "tableinstancing.h"
#include "xrscenebridge.h"

namespace {

int g_failures = 0;

void check(bool condition, const std::string& what)
{
    std::cout << (condition ? "  PASS  " : "  FAIL  ") << what << std::endl;
    if (!condition)
        ++g_failures;
}

AtomInstancing::Item item(float x, float y, float radius, const QColor& color)
{
    AtomInstancing::Item it;
    it.position = QVector3D(x, y, 0);
    it.scale = radius;
    it.color = color;
    return it;
}

SceneController::AtomDatum atom(const char* element, float x, float y = 0.0f)
{
    SceneController::AtomDatum a;
    a.element = QString::fromLatin1(element);
    a.position = QVector3D(x, y, 0);
    return a;
}

/// Mirror that counts table fetches and remembers the thread of the last one.
class CountingMirror : public MirrorInstancing
{
public:
    using MirrorInstancing::MirrorInstancing;
    int fetches = 0;
    QThread* fetchThread = nullptr;

protected:
    QByteArray getInstanceBuffer(int* instanceCount) override
    {
        ++fetches;
        fetchThread = QThread::currentThread();
        return MirrorInstancing::getInstanceBuffer(instanceCount);
    }
};

/// Red pixels (a red unlit sphere) in the left and right half of an image.
QPair<int, int> redLeftRight(const QImage& image)
{
    int left = 0, right = 0;
    const QImage img = image.convertToFormat(QImage::Format_RGB32);
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c.red() > 150 && c.green() < 80 && c.blue() < 80)
                (x < img.width() / 2 ? left : right)++;
        }
    return { left, right };
}

QImage render(QQuickView& view)
{
    QElapsedTimer t;
    t.start();
    while (!view.isExposed() && t.elapsed() < 3000)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return view.grabWindow();
}

void testTables()
{
    std::cout << "\n[1] mirror follows the source table" << std::endl;
    AtomInstancing source;
    MirrorInstancing mirror(&source);
    source.setItems({ item(0, 0, 1, Qt::red), item(1, 0, 1, Qt::red), item(2, 0, 1, Qt::red) });
    int count = -1;
    QByteArray bytes = mirror.instanceBuffer(&count);
    check(count == 3, "count 3 after the first build");
    check(bytes == source.table(), "bytes equal the source table");
    check(bytes.constData() == source.table().constData(), "table is shared, not copied");

    QVector<AtomInstancing::Item> five;
    for (int i = 0; i < 5; ++i)
        five.append(item(float(i), 2.0f, 0.5f, Qt::blue));
    source.setItems(five);
    bytes = mirror.instanceBuffer(&count);
    check(count == 5, "count 5 after a rebuild");
    check(bytes == source.table(), "bytes equal the rebuilt source table");
    check(mirror.instancePosition(4) == QVector3D(4, 2, 0), "instance 4 sits where the source put it");

    source.setItems({});
    mirror.instanceBuffer(&count);
    check(count == 0, "count 0 after clearing");
}

void testBridge()
{
    std::cout << "\n[2] one mirror per SceneController table" << std::endl;
    SceneController controller;
    XrSceneBridge bridge(&controller);
    const QMetaObject* meta = controller.metaObject();
    int tables = 0;
    for (int i = meta->propertyOffset(); i < meta->propertyCount(); ++i) {
        const QMetaProperty p = meta->property(i);
        if (qstrcmp(p.typeName(), "QQuick3DInstancing*") != 0)
            continue;
        ++tables;
        auto* source = p.read(&controller).value<QQuick3DInstancing*>();
        auto* mirror = qobject_cast<MirrorInstancing*>(bridge.property(p.name()).value<QQuick3DInstancing*>());
        check(mirror && source && mirror->source() == source,
            std::string("bridge.") + p.name() + " mirrors controller." + p.name());
    }
    check(tables == 14, "SceneController has 14 instance tables (got " + std::to_string(tables) + ")");

    QVector<SceneController::AtomDatum> atoms = { atom("O", 0), atom("H", 0.96f), atom("H", -0.24f, 0.93f) };
    QVector<SceneController::BondDatum> bonds = { { 0, 1, 1 }, { 0, 2, 1 } };
    controller.setStructure(atoms, bonds);
    int count = -1;
    const QByteArray atomBytes = bridge.atomInstancing()->instanceBuffer(&count);
    int sourceCount = -1;
    const QByteArray sourceBytes = controller.atomInstancing()->instanceBuffer(&sourceCount);
    check(count == 3 && sourceCount == 3, "atom mirror has the 3 atoms of the structure");

    // Claude Generated 2026 - selection shells (headset only): one per selected atom, in the mirror
    controller.setSelection({ 1 });
    int haloCount = -1;
    bridge.selectionHaloInstancing()->instanceBuffer(&haloCount);
    check(haloCount == 1, "selecting one atom gives one selection shell in the mirror");
    controller.setSelection({});
    bridge.selectionHaloInstancing()->instanceBuffer(&haloCount);
    check(haloCount == 0, "clearing the selection removes the shell");
    check(atomBytes == sourceBytes, "atom mirror bytes equal the controller's table");
    int bondCount = -1;
    bridge.bondInstancing()->instanceBuffer(&bondCount);
    int sourceBondCount = -1;
    controller.bondInstancing()->instanceBuffer(&sourceBondCount);
    check(bondCount == sourceBondCount && bondCount > 0, "bond mirror has the controller's bond segments");
}

void testIndexMapping()
{
    std::cout << "\n[3] instance row -> atom index with hidden hydrogens" << std::endl;
    // Methane first (its H are bonded only to C, so Polar hides them), then water (O-H stays).
    QVector<SceneController::AtomDatum> atoms = {
        atom("C", 0), atom("H", 1, 0), atom("H", -1, 0), atom("H", 0, 1), atom("H", 0, -1),
        atom("O", 5), atom("H", 6, 0), atom("H", 5, 1) };
    QVector<SceneController::BondDatum> bonds = {
        { 0, 1, 1 }, { 0, 2, 1 }, { 0, 3, 1 }, { 0, 4, 1 }, { 5, 6, 1 }, { 5, 7, 1 } };
    SceneController controller;
    controller.setStructure(atoms, bonds);
    controller.setHydrogenDisplay(SceneController::PolarHydrogens);
    auto* table = controller.atomInstancing();
    int rows = -1;
    table->instanceBuffer(&rows);
    check(rows == 4, "4 rows drawn (C, O and the two O-H hydrogens), got " + std::to_string(rows));
    bool allMatch = true;
    for (int r = 0; r < rows; ++r) {
        const int a = controller.atomIndexForInstance(r);
        allMatch = allMatch && a >= 0 && table->instancePosition(r) == controller.atomPosition(a);
    }
    check(allMatch, "every row maps to the atom at that row's position");
    check(controller.atomIndexForInstance(1) == 5, "row 1 is atom 5 (O), not atom 1");
    check(controller.atomIndexForInstance(rows) == -1 && controller.atomIndexForInstance(-1) == -1,
        "rows out of range map to -1");
    controller.setHydrogenDisplay(SceneController::AllHydrogens);
    table->instanceBuffer(&rows);
    check(rows == 8 && controller.atomIndexForInstance(7) == 7, "all shown: row = atom index");
}

void testRendering()
{
    std::cout << "\n[4] mirror rendered in a second window" << std::endl;
    QTemporaryDir dir;
    const QString qmlPath = dir.filePath(QStringLiteral("scene.qml"));
    QFile qml(qmlPath);
    if (!qml.open(QIODevice::WriteOnly)) {
        check(false, "write the test scene");
        return;
    }
    // Camera on +Z looking at the origin; an unlit red sphere at x = -3 lands in the left half.
    qml.write(R"(import QtQuick
import QtQuick3D
View3D {
    width: 240; height: 160
    environment: SceneEnvironment { backgroundMode: SceneEnvironment.Color; clearColor: "black" }
    PerspectiveCamera { z: 12; fieldOfView: 50 }
    Model {
        source: "#Sphere"
        instancing: table
        materials: PrincipledMaterial { baseColor: "white"; lighting: PrincipledMaterial.NoLighting }
    }
}
)");
    qml.close();

    AtomInstancing source;
    CountingMirror mirror(&source);
    source.setItems({ item(-3, 0, 1, Qt::red) });

    QQuickView desktop;   // the source's own window
    desktop.rootContext()->setContextProperty(QStringLiteral("table"), &source);
    desktop.setSource(QUrl::fromLocalFile(qmlPath));
    QQuickView headset;   // a second window with the mirror
    headset.rootContext()->setContextProperty(QStringLiteral("table"), &mirror);
    headset.setSource(QUrl::fromLocalFile(qmlPath));
    if (desktop.status() != QQuickView::Ready || headset.status() != QQuickView::Ready) {
        check(false, "test scene loads");
        return;
    }
    desktop.show();
    headset.show();

    const auto d0 = redLeftRight(render(desktop));
    if (d0.first == 0) {
        std::cout << "  SKIP  nothing rendered (no OpenGL for Qt Quick 3D here)" << std::endl;
        return;
    }
    const auto h0 = redLeftRight(render(headset));
    check(h0.first > 50 && h0.second == 0, "mirror draws the sphere on the left like the source");
    const int fetches0 = mirror.fetches;
    std::cout << "        mirror table fetched on the "
              << (mirror.fetchThread == QThread::currentThread() ? "GUI thread" : "render thread")
              << std::endl;

    source.setItems({ item(3, 0, 1, Qt::red) });
    const auto h1 = redLeftRight(render(headset));
    check(h1.first == 0 && h1.second > 50, "after a source rebuild the mirror draws it on the right");
    check(mirror.fetches > fetches0, "the rebuild made the mirror fetch the table again");

    // Same rebuild, but not announced: the mirror is not marked dirty and keeps the old table,
    // while the source window, marked dirty by the source itself, follows.
    const int fetches1 = mirror.fetches;
    source.blockSignals(true);
    source.setItems({ item(-3, 0, 1, Qt::red) });
    source.blockSignals(false);
    const auto h2 = redLeftRight(render(headset));
    const auto d2 = redLeftRight(render(desktop));
    check(h2.first == 0 && h2.second > 50 && mirror.fetches == fetches1,
        "without tableChanged the mirror is not fetched again (stays on the right)");
    check(d2.first > 50 && d2.second == 0, "the source window follows its own rebuild (left)");
}

/// Bounding box of the cyan (measurement line) pixels; empty if there are none.
QRect cyanBox(const QImage& image)
{
    QRect box;
    const QImage img = image.convertToFormat(QImage::Format_RGB32);
    for (int y = 0; y < img.height(); ++y)
        for (int x = 0; x < img.width(); ++x) {
            const QColor c = img.pixelColor(x, y);
            if (c.blue() > 200 && c.green() > 170 && c.red() < 150)
                box = box.united(QRect(x, y, 1, 1));
        }
    return box;
}

void testHeadsetContent()
{
    std::cout << "\n[5] MoleculeModels.qml in a rotated scene, fed by XrSceneBridge" << std::endl;
    // Two atoms on the x axis; the scene turns the molecule by 90 degrees about z (as the headset's
    // moleculeAnchor does), so a measurement line between them must come out vertical.
    SceneController controller;
    controller.setStructure({ atom("C", -4), atom("C", 4) }, {});
    XrSceneBridge bridge(&controller);

    QTemporaryDir dir;
    const QString qmlPath = dir.filePath(QStringLiteral("headset.qml"));
    QFile qml(qmlPath);
    if (!qml.open(QIODevice::WriteOnly)) {
        check(false, "write the test scene");
        return;
    }
    const QString qmlDir = qEnvironmentVariable("QURCUMA_QML_DIR", QStringLiteral(QURCUMA_QML_DIR));
    qml.write(QStringLiteral(R"(import QtQuick
import QtQuick3D
import "%1"
View3D {
    width: 240; height: 240
    environment: SceneEnvironment { backgroundMode: SceneEnvironment.Color; clearColor: "black" }
    PerspectiveCamera { z: 10; fieldOfView: 60; clipNear: 1 }
    Node {
        eulerRotation.z: 90
        Node {
            position: Qt.vector3d(-controller.sceneCenter.x, -controller.sceneCenter.y, -controller.sceneCenter.z)
            MoleculeModels { ctl: controller; inst: tables }
        }
    }
}
)").arg(QUrl::fromLocalFile(qmlDir).toString()).toUtf8());
    qml.close();

    QQuickView view;
    view.rootContext()->setContextProperty(QStringLiteral("controller"), &controller);
    view.rootContext()->setContextProperty(QStringLiteral("tables"), &bridge);
    view.setSource(QUrl::fromLocalFile(qmlPath));
    if (view.status() != QQuickView::Ready) {
        for (const QQmlError& e : view.errors())
            std::cout << "        " << e.toString().toStdString() << std::endl;
        check(false, "MoleculeModels.qml loads with the bridge");
        return;
    }
    view.show();
    const QImage before = render(view);
    if (before.isNull()) {
        std::cout << "  SKIP  nothing rendered" << std::endl;
        return;
    }
    check(cyanBox(before).isEmpty(), "no measurement line before the measurement");

    // Switched on after the first frame: the line Model was hidden when the window first synced.
    controller.setMeasurement({ { QVector3D(-4, 0, 0), QVector3D(4, 0, 0) } }, QStringLiteral("8.000"));
    const QRect box = cyanBox(render(view));
    std::cout << "        cyan box " << box.width() << " x " << box.height() << " px" << std::endl;
    check(box.height() > 100 && box.height() > 4 * box.width(),
        "the line shows up and turns with its parent (vertical, not the unrotated horizontal)");
}

} // namespace

int main(int argc, char* argv[])
{
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", "offscreen");
    // As in spikes/vr_preview: offscreen offers Qt Quick 3D only OpenGL, and its default GL 2.1
    // context lacks the GLSL versions of Quick 3D's shaders; Qt Quick must not fall back to software.
    QSurfaceFormat format;
    format.setVersion(4, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    if (qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND"))
        qputenv("QT_QUICK_BACKEND", "rhi");
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QGuiApplication app(argc, argv);

    std::cout << "Instance-table mirror test" << std::endl;
    std::cout << "==========================" << std::endl;
    testTables();
    testBridge();
    testIndexMapping();
    testRendering();
    testHeadsetContent();

    std::cout << "\n" << (g_failures == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string(g_failures)) << std::endl;
    return g_failures == 0 ? 0 : 1;
}
